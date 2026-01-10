#include "ap_ipc_client.h"
#include <sstream>
#include <random>

namespace APClientLib {

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
    } catch (const nlohmann::json::exception&) {
        // Silently fail - server will log the error
    }
    return msg;
}

// Helper to generate UUID-like message IDs
static std::string generate_msg_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
        ss << dis(gen);
    }
    return ss.str();
}

// APIPCClient implementation
APIPCClient::APIPCClient() {
}

APIPCClient::~APIPCClient() {
    disconnect();
}

VoidResult APIPCClient::connect(const std::string& pipe_name, std::chrono::milliseconds timeout) {
    if (is_connected_) {
        return VoidResult::failure("Already connected");
    }

    // Try to connect to the named pipe with timeout
    auto start_time = std::chrono::steady_clock::now();

    while (true) {
        pipe_handle_ = CreateFileA(
            pipe_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (pipe_handle_ != INVALID_HANDLE_VALUE) {
            break;
        }

        DWORD error = GetLastError();
        if (error != ERROR_PIPE_BUSY) {
            return VoidResult::failure("Failed to connect to pipe: " + std::to_string(error));
        }

        // Check timeout
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= timeout) {
            return VoidResult::failure("Connection timeout");
        }

        // Wait and retry
        if (!WaitNamedPipeA(pipe_name.c_str(), static_cast<DWORD>(timeout.count()))) {
            return VoidResult::failure("Pipe not available");
        }
    }

    // Set pipe mode to message
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe_handle_, &mode, NULL, NULL)) {
        CloseHandle(pipe_handle_);
        pipe_handle_ = INVALID_HANDLE_VALUE;
        return VoidResult::failure("Failed to set pipe mode");
    }

    is_connected_ = true;
    should_stop_ = false;

    // Start read thread
    read_thread_ = std::thread(&APIPCClient::read_loop, this);

    return VoidResult::success_result();
}

void APIPCClient::disconnect() {
    if (!is_connected_) {
        return;
    }

    should_stop_ = true;

    if (pipe_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_handle_);
        pipe_handle_ = INVALID_HANDLE_VALUE;
    }

    if (read_thread_.joinable()) {
        read_thread_.join();
    }

    is_connected_ = false;
}

VoidResult APIPCClient::send(const IPCMessage& message) {
    if (!is_connected_) {
        return VoidResult::failure("Not connected");
    }

    if (!write_message(message)) {
        return VoidResult::failure("Failed to send message");
    }

    return VoidResult::success_result();
}

std::vector<IPCMessage> APIPCClient::get_messages() {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    std::vector<IPCMessage> messages;
    messages.swap(pending_messages_);
    return messages;
}

void APIPCClient::set_message_callback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    message_callback_ = callback;
}

bool APIPCClient::is_connected() const {
    return is_connected_;
}

VoidResult APIPCClient::register_mod(const std::string& mod_id, const nlohmann::json& mod_info) {
    mod_id_ = mod_id;

    IPCMessage msg;
    msg.type = "register";
    msg.from_mod_id = mod_id;
    msg.msg_id = generate_msg_id();
    msg.data = mod_info;

    return send(msg);
}

VoidResult APIPCClient::send_location_check(int64_t location_id) {
    return send_location_checks({location_id});
}

VoidResult APIPCClient::send_location_checks(const std::vector<int64_t>& location_ids) {
    IPCMessage msg;
    msg.type = "notification";
    msg.from_mod_id = mod_id_;
    msg.data["event"] = "LOCATION_CHECK";
    msg.data["location_ids"] = location_ids;

    return send(msg);
}

void APIPCClient::read_loop() {
    while (!should_stop_ && is_connected_) {
        IPCMessage msg;
        if (!read_message(msg)) {
            // Connection closed or error
            is_connected_ = false;
            break;
        }

        // Store message
        {
            std::lock_guard<std::mutex> lock(messages_mutex_);
            pending_messages_.push_back(msg);
        }

        // Call callback if set
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (message_callback_) {
                message_callback_(msg);
            }
        }
    }
}

bool APIPCClient::read_message(IPCMessage& out_message) {
    // Read message length (4 bytes)
    DWORD length = 0;
    DWORD bytes_read = 0;

    if (!ReadFile(pipe_handle_, &length, sizeof(length), &bytes_read, NULL)) {
        return false;
    }

    if (bytes_read != sizeof(length)) {
        return false;
    }

    // Validate message size
    if (length == 0 || length > MAX_IPC_MESSAGE_SIZE) {
        return false;
    }

    // Read message content
    std::vector<char> buffer(length);
    if (!ReadFile(pipe_handle_, buffer.data(), length, &bytes_read, NULL)) {
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

bool APIPCClient::write_message(const IPCMessage& message) {
    std::string json_str = message.to_json();
    DWORD length = static_cast<DWORD>(json_str.size());

    if (length > MAX_IPC_MESSAGE_SIZE) {
        return false;
    }

    // Write message length
    DWORD bytes_written = 0;
    if (!WriteFile(pipe_handle_, &length, sizeof(length), &bytes_written, NULL)) {
        return false;
    }

    // Write message content
    if (!WriteFile(pipe_handle_, json_str.data(), length, &bytes_written, NULL)) {
        return false;
    }

    FlushFileBuffers(pipe_handle_);
    return true;
}

} // namespace APClientLib