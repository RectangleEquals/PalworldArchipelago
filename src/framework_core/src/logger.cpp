#include "logger.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace APFramework {

Logger::Logger()
    : mode_(LogMode::FRAMEWORK_ONLY)
    , verbosity_(LogLevel::INFO)
    , callback_(nullptr)
    , file_logging_enabled_(false) {
}

void Logger::set_mode(LogMode mode) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    mode_ = mode;
}

void Logger::set_verbosity(LogLevel level) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    verbosity_ = level;
}

void Logger::set_log_callback(LogCallback callback) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    callback_ = callback;
}

bool Logger::enable_file_logging(const std::string& log_file_path, bool append) {
    std::lock_guard<std::mutex> lock(log_mutex_);

    // Close existing file if open
    if (log_file_.is_open()) {
        log_file_.close();
    }

    // Convert to absolute path if relative
    std::filesystem::path file_path(log_file_path);

    // Open file in append or truncate mode
    auto mode = append ? (std::ios::out | std::ios::app) : (std::ios::out | std::ios::trunc);
    log_file_.open(file_path, mode);

    if (!log_file_.is_open()) {
        file_logging_enabled_ = false;
        return false;
    }

    file_logging_enabled_ = true;

    // Write header to log file
    log_file_ << "========================================\n";
    log_file_ << "APFramework Log - " << get_timestamp() << "\n";
    log_file_ << "========================================\n";
    log_file_.flush();

    return true;
}

void Logger::disable_file_logging() {
    std::lock_guard<std::mutex> lock(log_mutex_);

    if (log_file_.is_open()) {
        log_file_.close();
    }

    file_logging_enabled_ = false;
}

void Logger::debug(const std::string& component, const std::string& message) {
    log(LogLevel::DEBUG, component, message);
}

void Logger::info(const std::string& component, const std::string& message) {
    log(LogLevel::INFO, component, message);
}

void Logger::warning(const std::string& component, const std::string& message) {
    log(LogLevel::WARNING, component, message);
}

void Logger::error(const std::string& component, const std::string& message) {
    log(LogLevel::ERROR, component, message);
}

void Logger::log(LogLevel level, const std::string& component, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);

    // Always write to file if file logging is enabled (bypasses all filters)
    if (file_logging_enabled_) {
        write_to_file(level, component, message);
    }

    // Check if this log should be forwarded via IPC
    if (!should_forward(level, component)) {
        return;
    }

    // Forward to callback if registered
    if (callback_) {
        callback_(level, component, message);
    }
}

bool Logger::should_forward(LogLevel level, const std::string& component) const {
    // Check verbosity level first
    if (level < verbosity_) {
        return false;
    }

    // Check mode-based filtering
    switch (mode_) {
        case LogMode::MINIMAL:
            // Only ERROR level
            return level == LogLevel::ERROR;

        case LogMode::FRAMEWORK_ONLY:
            // WARNING and ERROR only
            return level >= LogLevel::WARNING;

        case LogMode::ALL:
            // All logs that pass verbosity check
            return true;

        default:
            return false;
    }
}

std::string Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "debug";
        case LogLevel::INFO:    return "info";
        case LogLevel::WARNING: return "warning";
        case LogLevel::ERROR:   return "error";
        default:                return "unknown";
    }
}

LogLevel Logger::string_to_level(const std::string& level_str) {
    std::string lower = level_str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "debug") return LogLevel::DEBUG;
    if (lower == "info") return LogLevel::INFO;
    if (lower == "warning" || lower == "warn") return LogLevel::WARNING;
    if (lower == "error") return LogLevel::ERROR;

    return LogLevel::INFO; // Default
}

LogMode Logger::string_to_mode(const std::string& mode_str) {
    std::string lower = mode_str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "minimal") return LogMode::MINIMAL;
    if (lower == "framework_only") return LogMode::FRAMEWORK_ONLY;
    if (lower == "all") return LogMode::ALL;

    return LogMode::FRAMEWORK_ONLY; // Default
}

void Logger::write_to_file(LogLevel level, const std::string& component, const std::string& message) {
    if (!log_file_.is_open()) {
        return;
    }

    // Format: [TIMESTAMP] [LEVEL] [Component] Message
    log_file_ << "[" << get_timestamp() << "] ";
    log_file_ << "[" << level_to_string(level) << "] ";
    log_file_ << "[" << component << "] ";
    log_file_ << message << "\n";
    log_file_.flush();
}

std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

} // namespace APFramework
