#pragma once
#include <string>
#include <sstream>

namespace APFramework {

class APDebugLog {
public:
    // Format and log JSON objects
    static void log_json(const std::string& label, const std::string& json_str);

    // Log lifecycle state transitions
    static void log_state_transition(const std::string& from_state,
                                      const std::string& to_state,
                                      const std::string& reason = "");

    // Log IPC message details
    static void log_ipc_message(const std::string& direction,  // "SENT" or "RECV"
                                const std::string& from_mod,
                                const std::string& to_mod,
                                const std::string& message_type);

    // Log timing information
    static void log_timing(const std::string& operation, double duration_ms);

    // Log hex dump of binary data (useful for IPC debugging)
    static void log_hex_dump(const std::string& label, const void* data, size_t size);

private:
    APDebugLog() = delete;
};

} // namespace APFramework