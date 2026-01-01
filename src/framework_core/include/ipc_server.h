#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <map>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <windows.h>
#include "message_queue.h"

namespace APFramework {

/**
 * @brief IPC message structure for mod communication
 *
 * Represents a message sent between the framework and mods via Named Pipes.
 * All messages are JSON-encoded for flexibility and human readability.
 */
struct IPCMessage {
    std::string type;      // Message type (e.g., "register", "location_check", "item_received")
    std::string mod_id;    // Sender/receiver mod ID
    std::string data_json; // Message payload as JSON
};

/**
 * @brief Named Pipes IPC server for mod communication
 *
 * Manages communication with AP-enabled mods via Windows Named Pipes:
 * - Listens for incoming mod connections on a named pipe
 * - Maintains per-mod message queues for incoming/outgoing messages
 * - Handles multiple simultaneous mod connections
 * - Provides thread-safe message sending to specific mods or all mods
 *
 * The server runs on a dedicated thread and dispatches incoming messages
 * to a registered handler function (typically the framework core).
 */
class IPCServer {
public:
    explicit IPCServer(const std::string& pipe_name);
    ~IPCServer();

    // Server lifecycle
    void start();
    void stop();
    bool is_running() const;

    // Message handling
    void send_to_mod(const std::string& mod_id, const IPCMessage& msg);
    void send_to_all_mods(const IPCMessage& msg);
    void set_message_handler(std::function<void(const IPCMessage&)> handler);

    // Mod management
    void register_mod(const std::string& mod_id);
    void unregister_mod(const std::string& mod_id);
    std::vector<std::string> get_registered_mods() const;

private:
    void server_loop();
    void handle_client(HANDLE pipe_handle);
    std::string read_message(HANDLE pipe_handle);
    bool write_message(HANDLE pipe_handle, const std::string& message);

    std::string pipe_name_;
    std::thread server_thread_;
    std::atomic<bool> running_;

    // Per-mod message queues
    std::map<std::string, MessageQueue<IPCMessage>> mod_queues_;
    mutable std::mutex queues_mutex_;

    // Per-mod pipe handles
    std::map<std::string, HANDLE> mod_pipes_;
    mutable std::mutex pipes_mutex_;

    std::function<void(const IPCMessage&)> message_handler_;
};

} // namespace APFramework
