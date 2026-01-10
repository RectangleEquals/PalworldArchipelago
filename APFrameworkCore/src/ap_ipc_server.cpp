#include "ap_ipc_server.h"
#include "ap_logger.h"
#include "ap_debug_log.h"
#include <sstream>
#include <algorithm>

namespace APFramework {

// Maximum message size (1MB default)
constexpr size_t MAX_IPC_MESSAGE_SIZE = 1024 * 1024;

// IPCMessage serialization
std::string IPCMessage::to_json() const {
    nlohmann::json j;
    j["type"] = type;
    j["from_mod_id"] = from_mod_id;
    j["to_mod_id"] = to_mod_id;
    j["msg_id"] = msg_id;
    j["data"] = data;
    return j.dump();
}

IPCMessage IPCMessage::from_json(const std::string& json_str) {
    IPCMessage msg;
    try {
        auto j = nlohmann::json::parse(json_str);
        msg.type = j.value("type", "");
        msg.from_mod_id = j.value("from_mod_id", "");
        msg.to_mod_id = j.value("to_mod_id", "");
        msg.msg_id = j.value("msg_id", "");
        msg.data = j.value("data", nlohmann::json::object());
    } catch (const nlohmann::json::exception& e) {
        AP_LOG_ERROR(std::string("Failed to parse IPC message: ") + e.what());
    }
    return msg;
}

// APIPCServer implementation
APIPCServer::APIPCServer() {
}

APIPCServer::~APIPCServer() {
    stop();
}

VoidResult APIPCServer::init(const std::string& pipe_name) {
    if (is_running_) {
        return VoidResult::failure(ErrorCode::INIT_ERROR, "Server already running");
    }

    pipe_name_ = pipe_name;
    AP_LOG_INFO("IPC Server initialized with pipe: " + pipe_name_);
    return VoidResult::success();
}

VoidResult APIPCServer::start() {
    if (is_running_) {
        return VoidResult::failure(ErrorCode::INIT_ERROR, "Server already running");
    }

    if (pipe_name_.empty()) {
        return VoidResult::failure(ErrorCode::INIT_ERROR, "Pipe name not set. Call init() first");
    }

    should_stop_ = false;
    is_running_ = true;

    // Start accept thread
    accept_thread_ = std::thread(&APIPCServer::accept_connections_loop, this);

    AP_LOG_INFO("IPC Server started");
    return VoidResult::success();
}

void APIPCServer::stop() {
    if (!is_running_) {
        return;
    }

    AP_LOG_INFO("Stopping IPC Server...");
    should_stop_ = true;

    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& [mod_id, client] : clients_) {
            client->should_stop = true;
            if (client->pipe_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(client->pipe_handle);
                client->pipe_handle = INVALID_HANDLE_VALUE;
            }
            if (client->read_thread.joinable()) {
                client->read_thread.join();
            }
        }
        clients_.clear();
    }

    // Wait for accept thread to finish
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    is_running_ = false;
    AP_LOG_INFO("IPC Server stopped");
}

void APIPCServer::accept_connections_loop() {
    while (!should_stop_) {
        // Create named pipe instance
        HANDLE pipe_handle = CreateNamedPipeA(
            pipe_name_.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            static_cast<DWORD>(MAX_IPC_MESSAGE_SIZE),
            static_cast<DWORD>(MAX_IPC_MESSAGE_SIZE),
            0,
            NULL
        );

        if (pipe_handle == INVALID_HANDLE_VALUE) {
            AP_LOG_ERROR("Failed to create named pipe: " + std::to_string(GetLastError()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        // Wait for client connection
        BOOL connected = ConnectNamedPipe(pipe_handle, NULL) ?
            TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (!connected || should_stop_) {
            CloseHandle(pipe_handle);
            continue;
        }

        // Create client connection object
        auto client = std::make_unique<ClientConnection>();
        client->pipe_handle = pipe_handle;
        client->should_stop = false;

        // Start read thread for this client
        client->read_thread = std::thread(&APIPCServer::handle_client, this, client.get());

        // Note: We'll add the client to the map once we receive their REGISTER message
        // For now, just keep the client object alive by moving it to a temporary storage
        // The read thread will handle registration

        // We need to transfer ownership - store temporarily with a placeholder key
        std::string temp_key = "temp_" + std::to_string(reinterpret_cast<uintptr_t>(client.get()));
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_[temp_key] = std::move(client);
        }
    }
}

void APIPCServer::handle_client(ClientConnection* client) {
    while (!client->should_stop && !should_stop_) {
        IPCMessage msg;
        if (!read_message(client->pipe_handle, msg)) {
            // Read failed or connection closed
            break;
        }

        APDebugLog::log_ipc_message("RECV", msg.from_mod_id, msg.to_mod_id, msg.type);

        // Handle REGISTER message specially - it sets the mod_id
        if (msg.type == "register" && client->mod_id.empty()) {
            client->mod_id = msg.from_mod_id;
            client->is_priority = is_priority_mod_id(msg.from_mod_id);

            // Move client from temp storage to proper mod_id key
            std::lock_guard<std::mutex> lock(clients_mutex_);
            auto temp_key = "temp_" + std::to_string(reinterpret_cast<uintptr_t>(client));
            auto it = clients_.find(temp_key);
            if (it != clients_.end()) {
                clients_[client->mod_id] = std::move(it->second);
                clients_.erase(it);
            }

            AP_LOG_INFO("Client registered: " + client->mod_id +
                       (client->is_priority ? " (priority)" : " (regular)"));
        }

        // Forward message to callback
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (message_callback_) {
                message_callback_(msg);
            }
        }
    }

    // Client disconnected
    if (!client->mod_id.empty()) {
        AP_LOG_INFO("Client disconnected: " + client->mod_id);

        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.erase(client->mod_id);
    }

    if (client->pipe_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(client->pipe_handle);
        client->pipe_handle = INVALID_HANDLE_VALUE;
    }
}

bool APIPCServer::read_message(HANDLE pipe, IPCMessage& out_message) {
    // Read message length (4 bytes)
    DWORD length = 0;
    DWORD bytes_read = 0;

    if (!ReadFile(pipe, &length, sizeof(length), &bytes_read, NULL)) {
        return false;
    }

    if (bytes_read != sizeof(length)) {
        return false;
    }

    // Validate message size
    if (length == 0 || length > MAX_IPC_MESSAGE_SIZE) {
        AP_LOG_ERROR("Invalid message size: " + std::to_string(length));
        return false;
    }

    // Read message content
    std::vector<char> buffer(length);
    if (!ReadFile(pipe, buffer.data(), length, &bytes_read, NULL)) {
        return false;
    }

    if (bytes_read != length) {
        return false;
    }

    // Parse message
    std::string json_str(buffer.data(), length);
    out_message = IPCMessage::from_json(json_str);

    return true;
}

bool APIPCServer::write_message(HANDLE pipe, const IPCMessage& message) {
    std::string json_str = message.to_json();
    DWORD length = static_cast<DWORD>(json_str.size());

    if (length > MAX_IPC_MESSAGE_SIZE) {
        AP_LOG_ERROR("Message too large: " + std::to_string(length) + " bytes");
        return false;
    }

    // Write message length
    DWORD bytes_written = 0;
    if (!WriteFile(pipe, &length, sizeof(length), &bytes_written, NULL)) {
        return false;
    }

    // Write message content
    if (!WriteFile(pipe, json_str.data(), length, &bytes_written, NULL)) {
        return false;
    }

    FlushFileBuffers(pipe);
    return true;
}

VoidResult APIPCServer::send_to_mod(const std::string& mod_id, const IPCMessage& message) {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    auto it = clients_.find(mod_id);
    if (it == clients_.end()) {
        return VoidResult::failure(ErrorCode::IPC_ERROR, "Mod not connected: " + mod_id);
    }

    APDebugLog::log_ipc_message("SEND", message.from_mod_id, mod_id, message.type);

    if (!write_message(it->second->pipe_handle, message)) {
        return VoidResult::failure(ErrorCode::IPC_ERROR, "Failed to send message to: " + mod_id);
    }

    return VoidResult::success();
}

VoidResult APIPCServer::broadcast(const IPCMessage& message) {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    APDebugLog::log_ipc_message("BROADCAST", message.from_mod_id, "all", message.type);

    for (const auto& [mod_id, client] : clients_) {
        if (!write_message(client->pipe_handle, message)) {
            AP_LOG_WARN("Failed to broadcast to: " + mod_id);
        }
    }

    return VoidResult::success();
}

VoidResult APIPCServer::broadcast_to_priority_clients(const IPCMessage& message) {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    APDebugLog::log_ipc_message("BROADCAST", message.from_mod_id, "priority", message.type);

    for (const auto& [mod_id, client] : clients_) {
        if (client->is_priority) {
            if (!write_message(client->pipe_handle, message)) {
                AP_LOG_WARN("Failed to broadcast to priority client: " + mod_id);
            }
        }
    }

    return VoidResult::success();
}

void APIPCServer::set_message_callback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    message_callback_ = callback;
}

std::vector<std::string> APIPCServer::get_connected_mods() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    std::vector<std::string> mod_ids;
    for (const auto& [mod_id, client] : clients_) {
        if (!mod_id.empty() && mod_id.find("temp_") != 0) {
            mod_ids.push_back(mod_id);
        }
    }
    return mod_ids;
}

bool APIPCServer::is_mod_connected(const std::string& mod_id) const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return clients_.find(mod_id) != clients_.end();
}

bool APIPCServer::is_priority_mod_id(const std::string& mod_id) const {
    // Priority mods match pattern: archipelago.<game_name>.*
    // Example: archipelago.palworld.framework, archipelago.palworld.framework_ui
    return mod_id.find("archipelago.") == 0;
}

} // namespace APFramework