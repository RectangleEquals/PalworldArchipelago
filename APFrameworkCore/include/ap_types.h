#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <filesystem>
#include <optional>
#include <nlohmann/json.hpp>

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
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
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

// Mod information structure
struct IncompatibleEntry {
    std::string id;                          // Other mod ID
    std::optional<std::string> versions;     // Version constraint or list
};

struct ModInfo {
    std::string mod_id;                      // "author.game.mod_name"
    std::string name;                        // "Friendly Mod Name"
    std::string version;                     // "1.0.0"
    std::string description;
    std::vector<IncompatibleEntry> incompatible;
    std::filesystem::path config_path;
    nlohmann::json capabilities;             // Mod's capability declaration
    bool is_priority{false};                 // Priority client flag
};

// Conflict detection structures
enum class ConflictType {
    DUPLICATE_ITEM_ID,
    DUPLICATE_LOCATION_ID,
    DUPLICATE_ITEM_NAME,
    DUPLICATE_LOCATION_NAME,
    INVALID_REGION_REFERENCE,
    INCOMPATIBLE_MOD
};

struct ConflictInfo {
    ConflictType type;
    std::string description;
    std::vector<std::string> involved_mods;  // Mod IDs involved in conflict
    std::string details;                     // Additional context (e.g., "item_id: 1000")
};

} // namespace APFramework