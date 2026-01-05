#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

// TEMPORARY: Emergency debug logging that writes directly to file
// This bypasses all the framework's logging system for debugging deadlocks
class DebugLog {
public:
    static DebugLog& instance() {
        static DebugLog instance;
        return instance;
    }

    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!file_.is_open()) {
            file_.open("E:\\SteamLibrary\\steamapps\\common\\Palworld\\Pal\\Binaries\\Win64\\ue4ss\\Mods\\APFramework\\Scripts\\debug_temp.log",
                      std::ios::out | std::ios::app);
        }

        if (file_.is_open()) {
            file_ << "[" << get_timestamp() << "] " << message << "\n";
            file_.flush();
        }
    }

private:
    DebugLog() {
        // Truncate file on startup
        std::ofstream truncate("E:\\SteamLibrary\\steamapps\\common\\Palworld\\Pal\\Binaries\\Win64\\ue4ss\\Mods\\APFramework\\Scripts\\debug_temp.log",
                              std::ios::out | std::ios::trunc);
        truncate.close();
    }

    ~DebugLog() {
        if (file_.is_open()) {
            file_.close();
        }
    }

    std::string get_timestamp() {
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
        oss << std::put_time(&tm_buf, "%H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    std::ofstream file_;
    std::mutex mutex_;
};

// Convenience macro
#define DEBUG_LOG(msg) DebugLog::instance().log(msg)