#include "ap_debug_log.h"
#include "ap_logger.h"
#include <iomanip>
#include <sstream>

namespace APFramework {

void APDebugLog::log_json(const std::string& label, const std::string& json_str) {
    std::stringstream ss;
    ss << label << ": " << json_str;
    AP_LOG_DEBUG(ss.str());
}

void APDebugLog::log_state_transition(const std::string& from_state,
                                      const std::string& to_state,
                                      const std::string& reason) {
    std::stringstream ss;
    ss << "STATE TRANSITION: " << from_state << " -> " << to_state;
    if (!reason.empty()) {
        ss << " (Reason: " << reason << ")";
    }
    AP_LOG_INFO(ss.str());
}

void APDebugLog::log_ipc_message(const std::string& direction,
                                 const std::string& from_mod,
                                 const std::string& to_mod,
                                 const std::string& message_type) {
    std::stringstream ss;
    ss << "IPC [" << direction << "] " << from_mod << " -> " << to_mod
       << " | Type: " << message_type;
    AP_LOG_TRACE(ss.str());
}

void APDebugLog::log_timing(const std::string& operation, double duration_ms) {
    std::stringstream ss;
    ss << "TIMING: " << operation << " took " << std::fixed
       << std::setprecision(2) << duration_ms << "ms";
    AP_LOG_DEBUG(ss.str());
}

void APDebugLog::log_hex_dump(const std::string& label, const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    std::stringstream ss;
    ss << label << " (hex dump, " << size << " bytes):\n";

    for (size_t i = 0; i < size; i += 16) {
        ss << std::hex << std::setfill('0') << std::setw(4) << i << ": ";

        // Hex bytes
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < size) {
                ss << std::setw(2) << static_cast<int>(bytes[i + j]) << " ";
            } else {
                ss << "   ";
            }
        }

        ss << " | ";

        // ASCII representation
        for (size_t j = 0; j < 16 && i + j < size; ++j) {
            uint8_t c = bytes[i + j];
            ss << (c >= 32 && c <= 126 ? static_cast<char>(c) : '.');
        }

        ss << "\n";
    }

    AP_LOG_DEBUG(ss.str());
}

} // namespace APFramework
