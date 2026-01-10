#pragma once
#include "ap_types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>
#include <windows.h>

namespace APFramework {

// IPC Message structure
struct IPCMessage {
    std::string type;           // "register", "command", "response", "notification", "ap_message"
    std::string from_mod_id;
    std::string to_mod_id;      // Empty for broadcasts
    std::string msg_id;         // For request/response correlation
    nlohmann::json data;

    // Serialize to JSON string
    std::string to_json() const;

    // Deserialize from JSON string
    static IPCMessage from_json(const std::string& json_str);
};

class APIPCServer {
public:
    APIPCServer();
    ~APIPCServer();

    // Initialize server with pipe name from config
    VoidResult init(const std::string& pipe_name);

    // Start accepting connections
    VoidResult start();

    // Stop server and disconnect all clients
    void stop();

    // Send message to specific mod
    VoidResult send_to_mod(const std::string& mod_id, const IPCMessage& message);

    // Broadcast message to all connected mods
    VoidResult broadcast(const IPCMessage& message);

    // Broadcast message to priority clients only
    VoidResult broadcast_to_priority_clients(const IPCMessage& message);

    // Set callback for received messages
    using MessageCallback = std::function<void(const IPCMessage&)>;
    void set_message_callback(MessageCallback callback);

    // Get list of connected mod IDs
    std::vector<std::string> get_connected_mods() const;

    // Check if specific mod is connected
    bool is_mod_connected(const std::string& mod_id) const;

    // Check if running
    bool is_running() const { return is_running_; }

private:
    struct ClientConnection {
        HANDLE pipe_handle;
        std::string mod_id;
        bool is_priority;
        std::thread read_thread;
        std::atomic<bool> should_stop;

        ClientConnection() : pipe_handle(INVALID_HANDLE_VALUE),
                            is_priority(false),
                            should_stop(false) {}
    };

    void accept_connections_loop();
    void handle_client(ClientConnection* client);
    bool read_message(HANDLE pipe, IPCMessage& out_message);
    bool write_message(HANDLE pipe, const IPCMessage& message);

    // Helper to check if mod_id is priority client
    bool is_priority_mod_id(const std::string& mod_id) const;

    std::string pipe_name_;
    std::thread accept_thread_;
    std::atomic<bool> should_stop_{false};
    std::atomic<bool> is_running_{false};

    std::map<std::string, std::unique_ptr<ClientConnection>> clients_;
    mutable std::mutex clients_mutex_;

    MessageCallback message_callback_;
    std::mutex callback_mutex_;
};

} // namespace APFramework