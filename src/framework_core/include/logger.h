#pragma once

// Prevent Windows.h macros from interfering
#ifdef ERROR
#undef ERROR
#endif

#include <string>
#include <fstream>
#include <mutex>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace APFramework {

/**
 * @brief Simple file logger for APFrameworkCore
 *
 * Writes log messages to a file with timestamps.
 * Thread-safe for concurrent logging from multiple components.
 */
class Logger {
public:
    enum class Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };

    static Logger& instance();

    void initialize(const std::string& log_file_path);
    void shutdown();

    void log(Level level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

private:
    Logger() = default;
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string get_timestamp();
    std::string level_to_string(Level level);

    std::ofstream log_file_;
    std::mutex mutex_;
    bool initialized_ = false;
};

// Convenience macros
#define LOG_DEBUG(msg) APFramework::Logger::instance().debug(msg)
#define LOG_INFO(msg) APFramework::Logger::instance().info(msg)
#define LOG_WARNING(msg) APFramework::Logger::instance().warning(msg)
#define LOG_ERROR(msg) APFramework::Logger::instance().error(msg)

} // namespace APFramework
