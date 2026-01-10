#pragma once
#include <string>
#include <cstdint>

namespace APFramework {

// Forward declarations
class APManager;
class APClient;
class APIPCServer;

// Common enums
enum class LifecyclePhase {
    UNINITIALIZED,
    DISCOVERING_MODS,
    AWAITING_PRIORITY_REGISTRATION,
    AWAITING_REGULAR_REGISTRATION,
    VALIDATING_CAPABILITIES,
    READY_FOR_CONNECTION,
    CONNECTING,
    CONNECTED_AND_SYNCING,
    GENERATING_CAPABILITIES,
    RUNNING,
    ERROR_STATE
};

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

// Result type for operations that may fail
struct VoidResult {
    bool success;
    std::string error_message;

    static VoidResult success_result() {
        return {true, ""};
    }

    static VoidResult failure(const std::string& msg) {
        return {false, msg};
    }

    bool is_success() const { return success; }
    bool is_failure() const { return !success; }
};

} // namespace APFramework