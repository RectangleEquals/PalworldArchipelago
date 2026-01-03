#include "logger.h"
#include <iostream>

namespace APFramework {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    shutdown();
}

void Logger::initialize(const std::string& log_file_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return;
    }

    log_file_.open(log_file_path, std::ios::out | std::ios::app);

    if (log_file_.is_open()) {
        initialized_ = true;
        log_file_ << "\n=== APFramework Core Logger Initialized ===\n";
        log_file_.flush();
    }
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (log_file_.is_open()) {
        log_file_ << "=== APFramework Core Logger Shutdown ===\n";
        log_file_.close();
    }

    initialized_ = false;
}

std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string Logger::level_to_string(Level level) {
    switch (level) {
        case Level::DEBUG:   return "DEBUG";
        case Level::INFO:    return "INFO ";
        case Level::WARNING: return "WARN ";
        case Level::ERROR:   return "ERROR";
        default:             return "UNKNOWN";
    }
}

void Logger::log(Level level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !log_file_.is_open()) {
        return;
    }

    log_file_ << "[" << get_timestamp() << "] "
              << "[" << level_to_string(level) << "] "
              << message << "\n";
    log_file_.flush();
}

void Logger::debug(const std::string& message) {
    log(Level::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(Level::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(Level::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(Level::ERROR, message);
}

} // namespace APFramework
