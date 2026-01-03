#pragma once

// Prevent Windows.h macros from interfering
#ifdef ERROR
#undef ERROR
#endif

#include <string>
#include <functional>
#include <mutex>
#include <fstream>

namespace APFramework {

/**
 * @brief Log levels for filtering messages
 */
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3
};

/**
 * @brief Log routing modes
 *
 * Controls which logs are forwarded to mods via IPC:
 * - MINIMAL: Only critical errors (ERROR level)
 * - FRAMEWORK_ONLY: Framework logs (WARNING and ERROR)
 * - ALL: All logs including mod-specific logs
 */
enum class LogMode {
    MINIMAL,
    FRAMEWORK_ONLY,
    ALL
};

/**
 * @brief Centralized logging system with IPC routing
 *
 * The Logger provides:
 * - Leveled logging (DEBUG, INFO, WARNING, ERROR)
 * - Filtering by mode (minimal, framework_only, all)
 * - Filtering by verbosity level
 * - IPC routing to forward logs to mods
 * - Thread-safe operation
 *
 * Logs are forwarded to mods via IPC message type "log" which allows
 * the Lua framework mod to display C++ logs in the UE4SS console.
 * This is essential for debugging since users can't directly see C++ output.
 *
 * Usage:
 *   logger->debug("MyComponent", "Detailed debug info");
 *   logger->info("MyComponent", "Something happened");
 *   logger->warning("MyComponent", "Potential issue");
 *   logger->error("MyComponent", "Critical error!");
 */
class Logger {
public:
    using LogCallback = std::function<void(LogLevel level, const std::string& component, const std::string& message)>;

    Logger();

    /**
     * @brief Set the log routing mode
     * @param mode MINIMAL, FRAMEWORK_ONLY, or ALL
     */
    void set_mode(LogMode mode);

    /**
     * @brief Set the minimum verbosity level to forward
     * @param level Only logs at this level or higher are forwarded
     */
    void set_verbosity(LogLevel level);

    /**
     * @brief Set callback for forwarding logs via IPC
     * @param callback Function to call when a log should be forwarded
     */
    void set_log_callback(LogCallback callback);

    /**
     * @brief Enable file logging
     * @param log_file_path Path to log file (relative or absolute)
     * @param append If true, append to existing file; if false, truncate
     * @return true if file was opened successfully
     */
    bool enable_file_logging(const std::string& log_file_path, bool append = true);

    /**
     * @brief Disable file logging
     */
    void disable_file_logging();

    /**
     * @brief Log a debug message
     * @param component Component name (e.g., "IPCServer", "ModRegistry")
     * @param message Log message
     */
    void debug(const std::string& component, const std::string& message);

    /**
     * @brief Log an info message
     * @param component Component name
     * @param message Log message
     */
    void info(const std::string& component, const std::string& message);

    /**
     * @brief Log a warning message
     * @param component Component name
     * @param message Log message
     */
    void warning(const std::string& component, const std::string& message);

    /**
     * @brief Log an error message
     * @param component Component name
     * @param message Log message
     */
    void error(const std::string& component, const std::string& message);

    /**
     * @brief Convert LogLevel to string for serialization
     */
    static std::string level_to_string(LogLevel level);

    /**
     * @brief Convert string to LogLevel
     */
    static LogLevel string_to_level(const std::string& level_str);

    /**
     * @brief Convert string to LogMode
     */
    static LogMode string_to_mode(const std::string& mode_str);

private:
    LogMode mode_;
    LogLevel verbosity_;
    LogCallback callback_;
    std::mutex log_mutex_;

    // File logging
    std::ofstream log_file_;
    bool file_logging_enabled_;

    /**
     * @brief Internal logging function
     * @param level Log level
     * @param component Component name
     * @param message Log message
     */
    void log(LogLevel level, const std::string& component, const std::string& message);

    /**
     * @brief Check if a log should be forwarded based on mode and verbosity
     * @param level Log level
     * @param component Component name
     * @return true if the log should be forwarded
     */
    bool should_forward(LogLevel level, const std::string& component) const;

    /**
     * @brief Write log to file (bypasses all filters)
     * @param level Log level
     * @param component Component name
     * @param message Log message
     */
    void write_to_file(LogLevel level, const std::string& component, const std::string& message);

    /**
     * @brief Get current timestamp as string
     * @return Formatted timestamp string
     */
    static std::string get_timestamp();
};

} // namespace APFramework
