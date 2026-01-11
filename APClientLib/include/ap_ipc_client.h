#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <functional>
#include <atomic>
#include <chrono>
#include <windows.h>

namespace APClientLib {

// IPC Message structure (matches APFramework::IPCMessage)
struct IPCMessage {
    std::string type;
    std::string from_mod_id;
    std::string to_mod_id;
    std::string msg_id;
    nlohmann::json data;

    // Serialize to JSON string
    std::string to_json() const;

    // Deserialize from JSON string
    static IPCMessage from_json(const std::string& json_str);
};

// Simple result type for client library
struct VoidResult {
    bool success;
    std::string error_message;

    static VoidResult success_result() { return {true, ""}; }
    static VoidResult failure(const std::string& msg) { return {false, msg}; }

    // Methods for consistency with APFramework::VoidResult
    bool is_success() const { return success; }
    bool is_failure() const { return !success; }
};

class APIPCClient {
public:
    APIPCClient();
    ~APIPCClient();

    // Connect to framework's Named Pipe
    VoidResult connect(const std::string& pipe_name, std::chrono::milliseconds timeout);

    // Disconnect from server
    void disconnect();

    // Send message to framework/other mods
    VoidResult send(const IPCMessage& message);

    // Get pending messages (non-blocking)
    std::vector<IPCMessage> get_messages();

    // Set callback for received messages (optional, alternative to polling)
    using MessageCallback = std::function<void(const IPCMessage&)>;
    void set_message_callback(MessageCallback callback);

    // Check if connected
    bool is_connected() const;

    // Convenience methods
    VoidResult register_mod(const std::string& mod_id, const nlohmann::json& mod_info);
    VoidResult send_location_check(int64_t location_id);
    VoidResult send_location_checks(const std::vector<int64_t>& location_ids);

private:
    void read_loop();
    bool read_message(IPCMessage& out_message);
    bool write_message(const IPCMessage& message);

    HANDLE pipe_handle_ = INVALID_HANDLE_VALUE;
    std::thread read_thread_;
    std::atomic<bool> should_stop_{false};
    std::atomic<bool> is_connected_{false};

    std::vector<IPCMessage> pending_messages_;
    mutable std::mutex messages_mutex_;

    MessageCallback message_callback_;
    std::mutex callback_mutex_;

    std::string mod_id_;
};

} // namespace APClientLib