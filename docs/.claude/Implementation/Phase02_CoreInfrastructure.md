# Phase 02: Core Infrastructure & Configuration

**Status**: 🔴 Not Started

---

## Overview

Implement foundational classes that all other components depend on: configuration management, logging system, and basic error handling infrastructure.

**Goals**:
- Robust configuration loading and validation
- Flexible logging system with file/console/IPC routing
- Debug logging utilities
- Error handling patterns
- Foundation for all subsequent phases

---

## Prerequisites

- ✅ Phase 01 complete (project structure and build system working)
- ✅ Stub files exist for APConfig, APLogger, APDebugLog
- ✅ nlohmann/json available

---

## Components

### 1. APConfig - Configuration Management
### 2. APLogger - Logging System
### 3. APDebugLog - Debug Utilities
### 4. Error Handling Infrastructure

---

## Implementation Details

### Component 1: APConfig

**Purpose**: Load, validate, and provide access to framework configuration from `framework_config.json`.

**File**: `APFrameworkCore/include/ap_config.h`

```cpp
#pragma once
#include "ap_types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <chrono>
#include <filesystem>
#include <optional>

namespace APFramework {

class APConfig {
public:
    // Singleton access
    static APConfig& instance();

    // Delete copy/move
    APConfig(const APConfig&) = delete;
    APConfig& operator=(const APConfig&) = delete;

    // Load configuration from file
    bool load(const std::filesystem::path& config_path);

    // Validate configuration
    bool validate() const;

    // Get configuration errors
    std::vector<std::string> get_errors() const;

    // AP Server settings
    const std::string& get_server() const { return ap_server_.server; }
    int get_port() const { return ap_server_.port; }
    const std::string& get_slot_name() const { return ap_server_.slot_name; }
    const std::string& get_password() const { return ap_server_.password; }
    bool get_auto_reconnect() const { return ap_server_.auto_reconnect; }
    std::chrono::milliseconds get_reconnect_delay() const {
        return std::chrono::milliseconds(ap_server_.reconnect_delay_ms);
    }
    std::chrono::milliseconds get_connection_timeout() const {
        return std::chrono::milliseconds(ap_server_.connection_timeout_ms);
    }

    // Framework settings
    std::chrono::milliseconds get_polling_interval() const {
        return std::chrono::milliseconds(framework_.polling_interval_ms);
    }
    std::chrono::milliseconds get_registration_timeout() const {
        return std::chrono::milliseconds(framework_.registration_timeout_ms);
    }
    std::chrono::milliseconds get_priority_registration_timeout() const {
        return std::chrono::milliseconds(framework_.priority_registration_timeout_ms);
    }
    std::filesystem::path get_mods_directory() const {
        return std::filesystem::path(framework_.mods_directory);
    }
    const std::string& get_game_name() const { return framework_.game_name; }

    // Logging settings
    bool is_logging_enabled() const { return logging_.enabled; }
    LogLevel get_log_level() const { return logging_.level; }
    std::filesystem::path get_log_file_path() const {
        return std::filesystem::path(logging_.file);
    }
    bool should_log_to_console() const { return logging_.console; }

    // IPC settings
    const std::string& get_ipc_endpoint() const { return ipc_.endpoint; }
    std::chrono::milliseconds get_ipc_timeout() const {
        return std::chrono::milliseconds(ipc_.timeout_ms);
    }

private:
    APConfig() = default;

    // Configuration structures
    struct APServerConfig {
        std::string server = "archipelago.gg";
        int port = 38281;
        std::string slot_name;
        std::string password;
        bool auto_reconnect = true;
        int reconnect_delay_ms = 5000;
        int connection_timeout_ms = 30000;
    } ap_server_;

    struct FrameworkConfig {
        int polling_interval_ms = 16;
        int registration_timeout_ms = 180000;      // 3 minutes
        int priority_registration_timeout_ms = 60000;  // 1 minute
        std::string mods_directory = "Mods";
        std::string game_name = "Palworld";
    } framework_;

    struct LoggingConfig {
        bool enabled = true;
        LogLevel level = LogLevel::INFO;
        std::string file = "APFramework.log";
        bool console = false;  // If true, logs go to priority clients
    } logging_;

    struct IPCConfig {
        std::string endpoint = "\\\\.\\pipe\\APFramework";
        int timeout_ms = 5000;
    } ipc_;

    mutable std::vector<std::string> errors_;

    // Helper methods
    LogLevel parse_log_level(const std::string& level_str) const;
    void validate_field(bool condition, const std::string& error_msg) const;
};

} // namespace APFramework
```

**File**: `APFrameworkCore/src/ap_config.cpp`

```cpp
#include "ap_config.h"
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace APFramework {

APConfig& APConfig::instance() {
    static APConfig instance;
    return instance;
}

bool APConfig::load(const std::filesystem::path& config_path) {
    errors_.clear();

    try {
        // Check if file exists
        if (!std::filesystem::exists(config_path)) {
            errors_.push_back("Config file not found: " + config_path.string());
            return false;
        }

        // Read file
        std::ifstream file(config_path);
        if (!file.is_open()) {
            errors_.push_back("Failed to open config file: " + config_path.string());
            return false;
        }

        // Parse JSON
        json config = json::parse(file);

        // Parse ap_server section
        if (config.contains("ap_server")) {
            auto& ap = config["ap_server"];
            if (ap.contains("server")) ap_server_.server = ap["server"];
            if (ap.contains("port")) ap_server_.port = ap["port"];
            if (ap.contains("slot_name")) ap_server_.slot_name = ap["slot_name"];
            if (ap.contains("password")) ap_server_.password = ap["password"];
            if (ap.contains("auto_reconnect")) ap_server_.auto_reconnect = ap["auto_reconnect"];
            if (ap.contains("reconnect_delay_ms")) ap_server_.reconnect_delay_ms = ap["reconnect_delay_ms"];
            if (ap.contains("connection_timeout_ms")) ap_server_.connection_timeout_ms = ap["connection_timeout_ms"];
        }

        // Parse framework section
        if (config.contains("framework")) {
            auto& fw = config["framework"];
            if (fw.contains("polling_interval_ms")) framework_.polling_interval_ms = fw["polling_interval_ms"];
            if (fw.contains("registration_timeout_ms")) framework_.registration_timeout_ms = fw["registration_timeout_ms"];
            if (fw.contains("priority_registration_timeout_ms"))
                framework_.priority_registration_timeout_ms = fw["priority_registration_timeout_ms"];
            if (fw.contains("mods_directory")) framework_.mods_directory = fw["mods_directory"];
            if (fw.contains("game_name")) framework_.game_name = fw["game_name"];
        }

        // Parse logging section
        if (config.contains("logging")) {
            auto& log = config["logging"];
            if (log.contains("enabled")) logging_.enabled = log["enabled"];
            if (log.contains("level")) logging_.level = parse_log_level(log["level"]);
            if (log.contains("file")) logging_.file = log["file"];
            if (log.contains("console")) logging_.console = log["console"];
        }

        // Parse ipc section
        if (config.contains("ipc")) {
            auto& ipc = config["ipc"];
            if (ipc.contains("endpoint")) ipc_.endpoint = ipc["endpoint"];
            if (ipc.contains("timeout_ms")) ipc_.timeout_ms = ipc["timeout_ms"];
        }

        return validate();

    } catch (const json::exception& e) {
        errors_.push_back(std::string("JSON parse error: ") + e.what());
        return false;
    } catch (const std::exception& e) {
        errors_.push_back(std::string("Config load error: ") + e.what());
        return false;
    }
}

bool APConfig::validate() const {
    errors_.clear();

    // Validate AP server settings
    validate_field(!ap_server_.server.empty(), "ap_server.server cannot be empty");
    validate_field(ap_server_.port > 0 && ap_server_.port < 65536,
                   "ap_server.port must be between 1 and 65535");
    validate_field(!ap_server_.slot_name.empty(), "ap_server.slot_name cannot be empty");
    validate_field(ap_server_.connection_timeout_ms > 0,
                   "ap_server.connection_timeout_ms must be positive");

    // Validate framework settings
    validate_field(framework_.polling_interval_ms > 0 && framework_.polling_interval_ms <= 1000,
                   "framework.polling_interval_ms must be between 1 and 1000");
    validate_field(framework_.registration_timeout_ms >= 1000,
                   "framework.registration_timeout_ms must be at least 1000ms");
    validate_field(framework_.priority_registration_timeout_ms >= 1000,
                   "framework.priority_registration_timeout_ms must be at least 1000ms");
    validate_field(!framework_.mods_directory.empty(),
                   "framework.mods_directory cannot be empty");
    validate_field(!framework_.game_name.empty(),
                   "framework.game_name cannot be empty");

    // Validate logging settings
    validate_field(!logging_.file.empty() || logging_.console,
                   "Must enable either file logging or console logging");

    // Validate IPC settings
    validate_field(!ipc_.endpoint.empty(), "ipc.endpoint cannot be empty");
    validate_field(ipc_.timeout_ms > 0, "ipc.timeout_ms must be positive");

    return errors_.empty();
}

std::vector<std::string> APConfig::get_errors() const {
    return errors_;
}

LogLevel APConfig::parse_log_level(const std::string& level_str) const {
    if (level_str == "trace") return LogLevel::TRACE;
    if (level_str == "debug") return LogLevel::DEBUG;
    if (level_str == "info") return LogLevel::INFO;
    if (level_str == "warn") return LogLevel::WARN;
    if (level_str == "error") return LogLevel::ERROR;
    if (level_str == "fatal") return LogLevel::FATAL;

    errors_.push_back("Unknown log level: " + level_str + ", defaulting to INFO");
    return LogLevel::INFO;
}

void APConfig::validate_field(bool condition, const std::string& error_msg) const {
    if (!condition) {
        errors_.push_back(error_msg);
    }
}

} // namespace APFramework
```

**Acceptance Criteria**:
- ✅ Loads JSON configuration from file
- ✅ Validates all required fields
- ✅ Provides type-safe getters with correct return types
- ✅ Returns detailed error messages on validation failure
- ✅ Singleton pattern for global access
- ✅ Handles missing optional fields with defaults

---

### Component 2: APLogger

**Purpose**: Flexible logging system that can write to files, route to console (via priority clients), or suppress logs.

**File**: `APFrameworkCore/include/ap_logger.h`

```cpp
#pragma once
#include "ap_types.h"
#include <string>
#include <fstream>
#include <mutex>
#include <functional>
#include <sstream>

namespace APFramework {

class APLogger {
public:
    static APLogger& instance();

    // Delete copy/move
    APLogger(const APLogger&) = delete;
    APLogger& operator=(const APLogger&) = delete;

    // Initialize logger with config
    bool init(LogLevel min_level, const std::string& log_file_path, bool console_mode);

    // Shutdown logger (flush and close file)
    void shutdown();

    // Log methods
    void trace(const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);

    // Generic log with level
    void log(LogLevel level, const std::string& message);

    // Set callback for console mode (used by APManager to route logs to priority clients)
    using LogCallback = std::function<void(LogLevel level, const std::string& message)>;
    void set_log_callback(LogCallback callback);

private:
    APLogger() = default;
    ~APLogger();

    void write_to_file(LogLevel level, const std::string& message);
    void write_to_callback(LogLevel level, const std::string& message);
    std::string get_timestamp() const;
    std::string level_to_string(LogLevel level) const;

    LogLevel min_level_ = LogLevel::INFO;
    std::ofstream log_file_;
    bool console_mode_ = false;
    LogCallback log_callback_;
    mutable std::mutex mutex_;
};

// Convenience macros for logging
#define AP_LOG_TRACE(msg) APFramework::APLogger::instance().trace(msg)
#define AP_LOG_DEBUG(msg) APFramework::APLogger::instance().debug(msg)
#define AP_LOG_INFO(msg) APFramework::APLogger::instance().info(msg)
#define AP_LOG_WARN(msg) APFramework::APLogger::instance().warn(msg)
#define AP_LOG_ERROR(msg) APFramework::APLogger::instance().error(msg)
#define AP_LOG_FATAL(msg) APFramework::APLogger::instance().fatal(msg)

} // namespace APFramework
```

**File**: `APFrameworkCore/src/ap_logger.cpp`

```cpp
#include "ap_logger.h"
#include <chrono>
#include <iomanip>
#include <iostream>

namespace APFramework {

APLogger& APLogger::instance() {
    static APLogger instance;
    return instance;
}

APLogger::~APLogger() {
    shutdown();
}

bool APLogger::init(LogLevel min_level, const std::string& log_file_path, bool console_mode) {
    std::lock_guard<std::mutex> lock(mutex_);

    min_level_ = min_level;
    console_mode_ = console_mode;

    if (!console_mode_) {
        // Open log file
        log_file_.open(log_file_path, std::ios::out | std::ios::app);
        if (!log_file_.is_open()) {
            std::cerr << "Failed to open log file: " << log_file_path << std::endl;
            return false;
        }
        log_file_ << "\n=== APFramework Session Started at " << get_timestamp() << " ===\n";
    }

    return true;
}

void APLogger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (log_file_.is_open()) {
        log_file_ << "=== APFramework Session Ended at " << get_timestamp() << " ===\n";
        log_file_.close();
    }
}

void APLogger::log(LogLevel level, const std::string& message) {
    if (level < min_level_) {
        return;  // Below minimum log level
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (console_mode_ && log_callback_) {
        write_to_callback(level, message);
    } else {
        write_to_file(level, message);
    }
}

void APLogger::trace(const std::string& message) { log(LogLevel::TRACE, message); }
void APLogger::debug(const std::string& message) { log(LogLevel::DEBUG, message); }
void APLogger::info(const std::string& message) { log(LogLevel::INFO, message); }
void APLogger::warn(const std::string& message) { log(LogLevel::WARN, message); }
void APLogger::error(const std::string& message) { log(LogLevel::ERROR, message); }
void APLogger::fatal(const std::string& message) { log(LogLevel::FATAL, message); }

void APLogger::set_log_callback(LogCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    log_callback_ = callback;
}

void APLogger::write_to_file(LogLevel level, const std::string& message) {
    if (log_file_.is_open()) {
        log_file_ << "[" << get_timestamp() << "] "
                  << "[" << level_to_string(level) << "] "
                  << message << std::endl;
        log_file_.flush();
    }
}

void APLogger::write_to_callback(LogLevel level, const std::string& message) {
    if (log_callback_) {
        log_callback_(level, message);
    }
}

std::string APLogger::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string APLogger::level_to_string(LogLevel level) const {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

} // namespace APFramework
```

**Acceptance Criteria**:
- ✅ Thread-safe logging with mutex
- ✅ Timestamp on every log entry
- ✅ Log level filtering
- ✅ File mode writes to log file
- ✅ Console mode uses callback (for IPC routing to priority clients)
- ✅ Proper file handling (flush, close on shutdown)
- ✅ Convenience macros for easy logging

---

### Component 3: APDebugLog

**Purpose**: Additional debug logging utilities for development and troubleshooting.

**File**: `APFrameworkCore/include/ap_debug_log.h`

```cpp
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
```

**File**: `APFrameworkCore/src/ap_debug_log.cpp`

```cpp
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
```

**Acceptance Criteria**:
- ✅ Utility functions for common debug scenarios
- ✅ JSON logging helper
- ✅ State transition logging
- ✅ IPC message tracing
- ✅ Performance timing helper
- ✅ Hex dump for binary data inspection

---

### Component 4: Error Handling Infrastructure

**File**: `APFrameworkCore/include/ap_types.h` (additions)

```cpp
// Error types
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

// Result wrapper
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
```

**Usage Example**:
```cpp
Result<std::string> load_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return Result<std::string>::failure(
            ErrorCode::INIT_ERROR,
            "Failed to open file: " + path
        );
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    return Result<std::string>::success(content);
}

// Usage:
auto result = load_file("config.json");
if (result.is_error()) {
    AP_LOG_ERROR("Error loading file: " + result.error_message);
    return;
}
// Use result.value
```

**Acceptance Criteria**:
- ✅ Standardized error codes
- ✅ Result<T> wrapper for operations with return values
- ✅ VoidResult for operations without return values
- ✅ Helper methods for success/failure cases
- ✅ Error message propagation

---

## Testing

### Unit Tests

**File**: `tests/unit/test_config.cpp`

```cpp
#include <gtest/gtest.h>
#include "ap_config.h"
#include <fstream>

using namespace APFramework;

class APConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test config file
        test_config_path_ = "test_framework_config.json";
    }

    void TearDown() override {
        // Clean up test file
        std::filesystem::remove(test_config_path_);
    }

    void write_config(const std::string& json_content) {
        std::ofstream file(test_config_path_);
        file << json_content;
        file.close();
    }

    std::string test_config_path_;
};

TEST_F(APConfigTest, LoadValidConfig) {
    write_config(R"({
        "ap_server": {
            "server": "archipelago.gg",
            "port": 38281,
            "slot_name": "TestPlayer",
            "password": ""
        },
        "framework": {
            "game_name": "Palworld"
        }
    })");

    auto& config = APConfig::instance();
    ASSERT_TRUE(config.load(test_config_path_));
    EXPECT_EQ(config.get_server(), "archipelago.gg");
    EXPECT_EQ(config.get_port(), 38281);
    EXPECT_EQ(config.get_slot_name(), "TestPlayer");
    EXPECT_EQ(config.get_game_name(), "Palworld");
}

TEST_F(APConfigTest, ValidateRequiredFields) {
    write_config(R"({
        "ap_server": {
            "server": "",
            "port": 38281
        }
    })");

    auto& config = APConfig::instance();
    ASSERT_FALSE(config.load(test_config_path_));
    auto errors = config.get_errors();
    EXPECT_FALSE(errors.empty());
}

TEST_F(APConfigTest, DefaultValues) {
    write_config(R"({
        "ap_server": {
            "slot_name": "Player1"
        }
    })");

    auto& config = APConfig::instance();
    ASSERT_TRUE(config.load(test_config_path_));
    EXPECT_EQ(config.get_server(), "archipelago.gg");  // Default value
    EXPECT_EQ(config.get_polling_interval().count(), 16);  // Default 16ms
}
```

**File**: `tests/unit/test_logger.cpp`

```cpp
#include <gtest/gtest.h>
#include "ap_logger.h"
#include <fstream>
#include <filesystem>

using namespace APFramework;

class APLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_log_file_ = "test_apframework.log";
        // Clean up any existing test log
        std::filesystem::remove(test_log_file_);
    }

    void TearDown() override {
        APLogger::instance().shutdown();
        std::filesystem::remove(test_log_file_);
    }

    std::string test_log_file_;
};

TEST_F(APLoggerTest, InitializeLogger) {
    auto& logger = APLogger::instance();
    ASSERT_TRUE(logger.init(LogLevel::DEBUG, test_log_file_, false));
    EXPECT_TRUE(std::filesystem::exists(test_log_file_));
}

TEST_F(APLoggerTest, WriteToFile) {
    auto& logger = APLogger::instance();
    logger.init(LogLevel::INFO, test_log_file_, false);

    logger.info("Test message");
    logger.warn("Warning message");

    logger.shutdown();

    // Read log file
    std::ifstream file(test_log_file_);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("Test message"), std::string::npos);
    EXPECT_NE(content.find("Warning message"), std::string::npos);
}

TEST_F(APLoggerTest, LogLevelFiltering) {
    auto& logger = APLogger::instance();
    logger.init(LogLevel::WARN, test_log_file_, false);

    logger.debug("Debug message");  // Should be filtered
    logger.warn("Warning message"); // Should appear
    logger.error("Error message");  // Should appear

    logger.shutdown();

    std::ifstream file(test_log_file_);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    EXPECT_EQ(content.find("Debug message"), std::string::npos);
    EXPECT_NE(content.find("Warning message"), std::string::npos);
    EXPECT_NE(content.find("Error message"), std::string::npos);
}

TEST_F(APLoggerTest, ConsoleMode) {
    auto& logger = APLogger::instance();
    logger.init(LogLevel::INFO, "", true);

    bool callback_invoked = false;
    std::string received_message;

    logger.set_log_callback([&](LogLevel level, const std::string& msg) {
        callback_invoked = true;
        received_message = msg;
    });

    logger.info("Callback test");

    EXPECT_TRUE(callback_invoked);
    EXPECT_EQ(received_message, "Callback test");
}
```

---

## Integration Points

### With APManager
- APManager loads config during initialization
- APManager sets up logger based on config
- APManager provides log callback for console mode

### With Other Components
- All components use AP_LOG_* macros for logging
- All components use Result<T> for error handling
- Configuration accessible via singleton throughout framework

---

## Acceptance Criteria

**Phase 02 is complete when**:
- ✅ APConfig loads and validates `framework_config.json`
- ✅ APConfig provides type-safe getters for all settings
- ✅ APConfig validates all required fields with detailed errors
- ✅ APLogger writes to file with timestamps and log levels
- ✅ APLogger supports console mode with callback routing
- ✅ APLogger is thread-safe
- ✅ APDebugLog provides utility functions for common debug scenarios
- ✅ Error handling infrastructure (ErrorCode, Result<T>) defined
- ✅ Unit tests pass for APConfig and APLogger
- ✅ Example `framework_config.json` created and documented

---

## Known Issues / Blockers

**None currently identified**

---

## Deliverables

1. ✅ `ap_config.h` and `ap_config.cpp` - Complete implementation
2. ✅ `ap_logger.h` and `ap_logger.cpp` - Complete implementation
3. ✅ `ap_debug_log.h` and `ap_debug_log.cpp` - Complete implementation
4. ✅ Updated `ap_types.h` with error handling types
5. ✅ Unit tests for config and logging
6. ✅ Example `framework_config.json` in project root

---

## Next Phase

[Phase 03: IPC Communication System](Phase03_IPCSystem.md)

**Prerequisites from Phase 02**:
- Configuration management working
- Logging infrastructure ready
- Error handling patterns established

---

**Last Updated**: 2026-01-09
**Status**: 🔴 Not Started