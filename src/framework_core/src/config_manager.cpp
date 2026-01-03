#include "config_manager.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace APFramework {

using json = nlohmann::json;

ConfigManager::ConfigManager() {
    // Initialize with defaults
    config_.polling_interval_ms = 16; // ~60fps
    config_.enable_logging = true;
    config_.log_level = "info";
    config_.log_mode = "framework_only";
    config_.log_to_file = true;
    config_.log_file_path = "apframework.log";
    config_.active_profile.name = "default";
    config_.active_profile.server = "archipelago.gg";
    config_.active_profile.port = 38281;
    config_.active_profile.slot_name = "";
    config_.active_profile.password = "";
    config_.active_profile.autoconnect = false;
}

bool ConfigManager::load_config(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(config_mutex_);

    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

        return deserialize_from_json(content);
    } catch (const std::exception&) {
        return false;
    }
}

bool ConfigManager::save_config(const std::string& config_path) const {
    std::lock_guard<std::mutex> lock(config_mutex_);

    try {
        std::string json_str = serialize_to_json();

        std::ofstream file(config_path);
        if (!file.is_open()) {
            return false;
        }

        file << json_str;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

ConfigManager::FrameworkConfig ConfigManager::get_config() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

void ConfigManager::set_config(const FrameworkConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;
}

ConfigManager::ConnectionProfile ConfigManager::get_active_profile() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_.active_profile;
}

void ConfigManager::set_active_profile(const std::string& profile_name) {
    std::lock_guard<std::mutex> lock(config_mutex_);

    auto it = config_.saved_profiles.find(profile_name);
    if (it != config_.saved_profiles.end()) {
        config_.active_profile = it->second;
    }
}

void ConfigManager::add_profile(const ConnectionProfile& profile) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.saved_profiles[profile.name] = profile;
}

void ConfigManager::remove_profile(const std::string& profile_name) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.saved_profiles.erase(profile_name);
}

std::vector<std::string> ConfigManager::list_profiles() const {
    std::lock_guard<std::mutex> lock(config_mutex_);

    std::vector<std::string> names;
    names.reserve(config_.saved_profiles.size());

    for (const auto& [name, _] : config_.saved_profiles) {
        names.push_back(name);
    }

    return names;
}

int ConfigManager::get_polling_interval() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_.polling_interval_ms;
}

void ConfigManager::set_polling_interval(int interval_ms) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.polling_interval_ms = interval_ms;
}

bool ConfigManager::is_autoconnect_enabled() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_.active_profile.autoconnect;
}

void ConfigManager::set_autoconnect(bool enabled) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.active_profile.autoconnect = enabled;
}

std::string ConfigManager::serialize_to_json() const {
    try {
        json root;

        // Serialize active profile
        json active;
        active["name"] = config_.active_profile.name;
        active["server"] = config_.active_profile.server;
        active["port"] = config_.active_profile.port;
        active["slot_name"] = config_.active_profile.slot_name;
        active["password"] = config_.active_profile.password;
        active["autoconnect"] = config_.active_profile.autoconnect;
        root["active_profile"] = active;

        // Serialize saved profiles
        json profiles = json::object();
        for (const auto& [name, profile] : config_.saved_profiles) {
            json p;
            p["name"] = profile.name;
            p["server"] = profile.server;
            p["port"] = profile.port;
            p["slot_name"] = profile.slot_name;
            p["password"] = profile.password;
            p["autoconnect"] = profile.autoconnect;
            profiles[name] = p;
        }
        root["saved_profiles"] = profiles;

        // Serialize other settings
        root["polling_interval_ms"] = config_.polling_interval_ms;
        root["enable_logging"] = config_.enable_logging;
        root["log_level"] = config_.log_level;
        root["log_mode"] = config_.log_mode;
        root["log_to_file"] = config_.log_to_file;
        root["log_file_path"] = config_.log_file_path;
        root["mod_overrides"] = config_.mod_overrides;

        return root.dump(4); // 4-space indentation
    } catch (const std::exception&) {
        return "{}";
    }
}

bool ConfigManager::deserialize_from_json(const std::string& json_str) {
    try {
        json root = json::parse(json_str);

        // Parse active profile
        if (root.contains("active_profile")) {
            const auto& active = root["active_profile"];
            config_.active_profile.name = active.value("name", "default");
            config_.active_profile.server = active.value("server", "archipelago.gg");
            config_.active_profile.port = active.value("port", 38281);
            config_.active_profile.slot_name = active.value("slot_name", "");
            config_.active_profile.password = active.value("password", "");
            config_.active_profile.autoconnect = active.value("autoconnect", false);
        }

        // Parse saved profiles
        if (root.contains("saved_profiles") && root["saved_profiles"].is_object()) {
            config_.saved_profiles.clear();
            for (const auto& [name, p] : root["saved_profiles"].items()) {
                ConnectionProfile profile;
                profile.name = p.value("name", name);
                profile.server = p.value("server", "archipelago.gg");
                profile.port = p.value("port", 38281);
                profile.slot_name = p.value("slot_name", "");
                profile.password = p.value("password", "");
                profile.autoconnect = p.value("autoconnect", false);
                config_.saved_profiles[name] = profile;
            }
        }

        // Parse other settings
        config_.polling_interval_ms = root.value("polling_interval_ms", 16);
        config_.enable_logging = root.value("enable_logging", true);
        config_.log_level = root.value("log_level", "info");
        config_.log_mode = root.value("log_mode", "framework_only");
        config_.log_to_file = root.value("log_to_file", true);
        config_.log_file_path = root.value("log_file_path", "apframework.log");

        if (root.contains("mod_overrides") && root["mod_overrides"].is_object()) {
            config_.mod_overrides.clear();
            for (const auto& [key, value] : root["mod_overrides"].items()) {
                if (value.is_string()) {
                    config_.mod_overrides[key] = value.get<std::string>();
                }
            }
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace APFramework
