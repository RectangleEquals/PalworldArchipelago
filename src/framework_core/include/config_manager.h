#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace APFramework {

/**
 * @brief Manages framework configuration and connection profiles
 *
 * Handles loading, saving, and managing framework configuration including:
 * - AP server connection settings (host, port, slot, password)
 * - Connection profiles for quick switching
 * - Framework behavior settings (autoconnect, polling interval, etc.)
 * - Per-mod configuration overrides
 *
 * Configuration is stored in JSON format and can be modified at runtime.
 */
class ConfigManager {
public:
    struct ConnectionProfile {
        std::string name;
        std::string server;
        int port;
        std::string slot_name;
        std::string password;
        bool autoconnect;
    };

    struct FrameworkConfig {
        ConnectionProfile active_profile;
        std::map<std::string, ConnectionProfile> saved_profiles;
        int polling_interval_ms;
        bool enable_logging;
        std::string log_level; // "debug", "info", "warn", "error"
        std::map<std::string, std::string> mod_overrides;
    };

    ConfigManager();

    // Load/Save configuration
    bool load_config(const std::string& config_path);
    bool save_config(const std::string& config_path) const;

    // Configuration access
    FrameworkConfig get_config() const;
    void set_config(const FrameworkConfig& config);

    // Profile management
    ConnectionProfile get_active_profile() const;
    void set_active_profile(const std::string& profile_name);
    void add_profile(const ConnectionProfile& profile);
    void remove_profile(const std::string& profile_name);
    std::vector<std::string> list_profiles() const;

    // Quick access to common settings
    int get_polling_interval() const;
    void set_polling_interval(int interval_ms);
    bool is_autoconnect_enabled() const;
    void set_autoconnect(bool enabled);

private:
    FrameworkConfig config_;
    mutable std::mutex config_mutex_;

    std::string serialize_to_json() const;
    bool deserialize_from_json(const std::string& json);
};

} // namespace APFramework
