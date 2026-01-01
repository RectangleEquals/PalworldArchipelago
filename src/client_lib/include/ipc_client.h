#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <windows.h>

namespace APClientLib {

/**
 * @brief IPC message structure for communication with framework
 */
struct IPCMessage {
    std::string type;       // Message type (e.g., "register", "item_received")
    std::string mod_id;     // Mod identifier
    std::string data;       // JSON data payload
};

/**
 * @brief Named Pipes IPC client for communicating with APFrameworkCore
 */
class IPCClient {
public:
    explicit IPCClient(const std::string& pipe_name);
    ~IPCClient();

    // Connection management
    bool connect();
    void disconnect();
    bool is_connected() const;

    // Messaging
    bool send_message(const std::string& type, const std::string& mod_id, const std::string& data_json);
    bool poll_messages(std::vector<IPCMessage>& out_messages);

    // Error handling
    std::string get_last_error() const;

private:
    std::string pipe_name_;
    HANDLE pipe_handle_;
    mutable std::mutex mutex_;
    std::string last_error_;

    bool read_message(IPCMessage& out_message);
    void set_error(const std::string& error);
};

} // namespace APClientLib
