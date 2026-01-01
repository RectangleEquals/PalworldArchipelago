#include "ipc_server.h"
#include <nlohmann/json.hpp>
#include <sstream>

namespace APFramework {

using json = nlohmann::json;

IPCServer::IPCServer(const std::string& pipe_name)
    : pipe_name_(pipe_name), running_(false) {
}

IPCServer::~IPCServer() {
    stop();
}

void IPCServer::start() {
    if (running_.load()) {
        return;
    }

    running_.store(true);
    server_thread_ = std::thread(&IPCServer::server_loop, this);
}

void IPCServer::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    // Close all pipe handles
    {
        std::lock_guard<std::mutex> lock(pipes_mutex_);
        for (auto& [mod_id, handle] : mod_pipes_) {
            CloseHandle(handle);
        }
        mod_pipes_.clear();
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

bool IPCServer::is_running() const {
    return running_.load();
}

void IPCServer::send_to_mod(const std::string& mod_id, const IPCMessage& msg) {
    std::lock_guard<std::mutex> lock(pipes_mutex_);

    auto it = mod_pipes_.find(mod_id);
    if (it == mod_pipes_.end()) {
        return; // Mod not connected
    }

    // Serialize message to JSON
    try {
        json msg_json;
        msg_json["type"] = msg.type;
        msg_json["mod_id"] = msg.mod_id;
        msg_json["data"] = json::parse(msg.data_json);

        std::string serialized = msg_json.dump();
        write_message(it->second, serialized);
    } catch (const std::exception&) {
        // Serialization failed - skip
    }
}

void IPCServer::send_to_all_mods(const IPCMessage& msg) {
    std::lock_guard<std::mutex> lock(pipes_mutex_);

    // Serialize message once
    std::string serialized;
    try {
        json msg_json;
        msg_json["type"] = msg.type;
        msg_json["mod_id"] = msg.mod_id;
        msg_json["data"] = json::parse(msg.data_json);

        serialized = msg_json.dump();
    } catch (const std::exception&) {
        return; // Serialization failed
    }

    // Send to all connected mods
    for (auto& [mod_id, handle] : mod_pipes_) {
        write_message(handle, serialized);
    }
}

void IPCServer::set_message_handler(std::function<void(const IPCMessage&)> handler) {
    message_handler_ = handler;
}

void IPCServer::register_mod(const std::string& mod_id) {
    std::lock_guard<std::mutex> lock(queues_mutex_);
    mod_queues_[mod_id]; // Create queue for this mod
}

void IPCServer::unregister_mod(const std::string& mod_id) {
    {
        std::lock_guard<std::mutex> lock(queues_mutex_);
        mod_queues_.erase(mod_id);
    }

    {
        std::lock_guard<std::mutex> lock(pipes_mutex_);
        auto it = mod_pipes_.find(mod_id);
        if (it != mod_pipes_.end()) {
            CloseHandle(it->second);
            mod_pipes_.erase(it);
        }
    }
}

std::vector<std::string> IPCServer::get_registered_mods() const {
    std::lock_guard<std::mutex> lock(queues_mutex_);

    std::vector<std::string> mod_ids;
    mod_ids.reserve(mod_queues_.size());

    for (const auto& [mod_id, _] : mod_queues_) {
        mod_ids.push_back(mod_id);
    }

    return mod_ids;
}

void IPCServer::server_loop() {
    std::string full_pipe_name = "\\\\.\\pipe\\" + pipe_name_;

    while (running_.load()) {
        // Create named pipe instance
        HANDLE pipe_handle = CreateNamedPipeA(
            full_pipe_name.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            4096, // Output buffer size
            4096, // Input buffer size
            0,    // Default timeout
            NULL  // Default security
        );

        if (pipe_handle == INVALID_HANDLE_VALUE) {
            // Failed to create pipe - wait and retry
            Sleep(1000);
            continue;
        }

        // Wait for client connection
        BOOL connected = ConnectNamedPipe(pipe_handle, NULL);
        if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(pipe_handle);
            continue;
        }

        // Handle client in separate thread (simple approach - spawn thread per client)
        std::thread client_thread(&IPCServer::handle_client, this, pipe_handle);
        client_thread.detach();
    }
}

void IPCServer::handle_client(HANDLE pipe_handle) {
    std::string mod_id;

    while (running_.load()) {
        std::string message = read_message(pipe_handle);
        if (message.empty()) {
            break; // Connection closed or error
        }

        try {
            json msg_json = json::parse(message);

            IPCMessage ipc_msg;
            ipc_msg.type = msg_json.value("type", "");
            ipc_msg.mod_id = msg_json.value("mod_id", "");

            // Extract data field (could be object or string)
            if (msg_json.contains("data")) {
                if (msg_json["data"].is_string()) {
                    ipc_msg.data_json = msg_json["data"];
                } else {
                    ipc_msg.data_json = msg_json["data"].dump();
                }
            }

            // Track mod_id for this connection
            if (mod_id.empty() && !ipc_msg.mod_id.empty()) {
                mod_id = ipc_msg.mod_id;

                std::lock_guard<std::mutex> lock(pipes_mutex_);
                mod_pipes_[mod_id] = pipe_handle;
            }

            // Dispatch to handler
            if (message_handler_) {
                message_handler_(ipc_msg);
            }
        } catch (const std::exception&) {
            // Invalid JSON or processing error - continue
        }
    }

    // Cleanup on disconnect
    if (!mod_id.empty()) {
        std::lock_guard<std::mutex> lock(pipes_mutex_);
        mod_pipes_.erase(mod_id);
    }

    CloseHandle(pipe_handle);
}

std::string IPCServer::read_message(HANDLE pipe_handle) {
    char buffer[4096];
    DWORD bytes_read = 0;

    BOOL success = ReadFile(
        pipe_handle,
        buffer,
        sizeof(buffer) - 1,
        &bytes_read,
        NULL
    );

    if (!success || bytes_read == 0) {
        return "";
    }

    buffer[bytes_read] = '\0';
    return std::string(buffer, bytes_read);
}

bool IPCServer::write_message(HANDLE pipe_handle, const std::string& message) {
    DWORD bytes_written = 0;

    BOOL success = WriteFile(
        pipe_handle,
        message.c_str(),
        static_cast<DWORD>(message.size()),
        &bytes_written,
        NULL
    );

    return success && bytes_written == message.size();
}

} // namespace APFramework
