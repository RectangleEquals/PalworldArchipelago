#pragma once
#include "ap_types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <chrono>
#include <filesystem>
#include <optional>
#include <vector>

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
    std::filesystem::path get_mods_directory() const;
    const std::string& get_game_name() const { return framework_.game_name; }

    // Logging settings
    bool is_logging_enabled() const { return logging_.enabled; }
    LogLevel get_log_level() const { return logging_.level; }
    std::filesystem::path get_log_file_path() const;
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
        LogLevel level = LogLevel::LOG_INFO;
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