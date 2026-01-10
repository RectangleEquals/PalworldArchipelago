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

// Error codes for standardized error handling
enum class ErrorCode {
    SUCCESS = 0,
    CONFIG_ERROR,
    INIT_ERROR,
    IPC_ERROR,
    CONNECTION_ERROR,
    TIMEOUT_ERROR,
    VALIDATION_ERROR,
    INTERNAL_ERROR,
    UNKNOWN_ERROR
};

// Result wrapper for operations with return values
template<typename T>
struct Result {
    T value;
    ErrorCode error = ErrorCode::SUCCESS;
    std::string error_message;

    bool is_success() const { return error == ErrorCode::SUCCESS; }
    bool is_error() const { return !is_success(); }

    static Result<T> success(T val) {
        return Result<T>{std::move(val), ErrorCode::SUCCESS, ""};
    }

    static Result<T> failure(ErrorCode err, const std::string& msg) {
        return Result<T>{T{}, err, msg};
    }
};

// Void result for operations without return value
struct VoidResult {
    ErrorCode error = ErrorCode::SUCCESS;
    std::string error_message;

    bool is_success() const { return error == ErrorCode::SUCCESS; }
    bool is_error() const { return !is_success(); }

    static VoidResult success() {
        return VoidResult{ErrorCode::SUCCESS, ""};
    }

    static VoidResult failure(ErrorCode err, const std::string& msg) {
        return VoidResult{err, msg};
    }
};

} // namespace APFramework