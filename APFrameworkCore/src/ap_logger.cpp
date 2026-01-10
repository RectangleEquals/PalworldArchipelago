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

void APLogger::log(LogLevel level, const std::string& component, const std::string& message) {
    log(level, "[" + component + "] " + message);
}

void APLogger::trace(const std::string& message) { log(LogLevel::LOG_TRACE, message); }
void APLogger::debug(const std::string& message) { log(LogLevel::LOG_DEBUG, message); }
void APLogger::info(const std::string& message) { log(LogLevel::LOG_INFO, message); }
void APLogger::warn(const std::string& message) { log(LogLevel::LOG_WARN, message); }
void APLogger::error(const std::string& message) { log(LogLevel::LOG_ERROR, message); }
void APLogger::fatal(const std::string& message) { log(LogLevel::LOG_FATAL, message); }

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
        case LogLevel::LOG_TRACE: return "TRACE";
        case LogLevel::LOG_DEBUG: return "DEBUG";
        case LogLevel::LOG_INFO:  return "INFO ";
        case LogLevel::LOG_WARN:  return "WARN ";
        case LogLevel::LOG_ERROR: return "ERROR";
        case LogLevel::LOG_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

} // namespace APFramework
