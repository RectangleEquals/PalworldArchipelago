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

    // Log with component prefix
    void log(LogLevel level, const std::string& component, const std::string& message);

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