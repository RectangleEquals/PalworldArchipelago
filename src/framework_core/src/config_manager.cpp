#include "config_manager.h"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

namespace APFramework {

using json = nlohmann::json;

ConfigManager::ConfigManager()
    : current_profile_index_(0) {
}

bool ConfigManager::load_from_file(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(config_mutex_);

    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return false;
        }

        json config_json;
        file >> config_json;

        // Clear existing profiles
        profiles_.clear();

        // Parse profiles array
        if (config_json.contains("profiles") && config_json["profiles"].is_array()) {
            for (const auto& profile_json : config_json["profiles"]) {
                ConnectionProfile profile;

                if (profile_json.contains("name") && profile_json["name"].is_string()) {
                    profile.name = profile_json["name"];
                } else {
                    continue; // Skip invalid profiles
                }

                if (profile_json.contains("server") && profile_json["server"].is_string()) {
                    profile.server = profile_json["server"];
                }

                if (profile_json.contains("port") && profile_json["port"].is_number_integer()) {
                    profile.port = profile_json["port"];
                }

                if (profile_json.contains("slot") && profile_json["slot"].is_string()) {
                    profile.slot = profile_json["slot"];
                }

                if (profile_json.contains("password") && profile_json["password"].is_string()) {
                    profile.password = profile_json["password"];
                }

                profiles_.push_back(profile);
            }
        }

        // Parse current profile index
        if (config_json.contains("current_profile") && config_json["current_profile"].is_number_integer()) {
            size_t index = config_json["current_profile"];
            if (index < profiles_.size()) {
                current_profile_index_ = index;
            }
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ConfigManager::save_to_file(const std::string& file_path) const {
    std::lock_guard<std::mutex> lock(config_mutex_);

    try {
        json config_json;
        json profiles_array = json::array();

        // Serialize profiles
        for (const auto& profile : profiles_) {
            json profile_json;
            profile_json["name"] = profile.name;
            profile_json["server"] = profile.server;
            profile_json["port"] = profile.port;
            profile_json["slot"] = profile.slot;
            profile_json["password"] = profile.password;
            profiles_array.push_back(profile_json);
        }

        config_json["profiles"] = profiles_array;
        config_json["current_profile"] = current_profile_index_;

        // Write to file with indentation
        std::ofstream file(file_path);
        if (!file.is_open()) {
            return false;
        }

        file << config_json.dump(4); // 4-space indentation
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void ConfigManager::add_profile(const ConnectionProfile& profile) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    profiles_.push_back(profile);
}

bool ConfigManager::remove_profile(size_t index) {
    std::lock_guard<std::mutex> lock(config_mutex_);

    if (index >= profiles_.size()) {
        return false;
    }

    profiles_.erase(profiles_.begin() + index);

    // Adjust current profile index if necessary
    if (current_profile_index_ >= profiles_.size() && !profiles_.empty()) {
        current_profile_index_ = profiles_.size() - 1;
    }

    return true;
}

bool ConfigManager::update_profile(size_t index, const ConnectionProfile& profile) {
    std::lock_guard<std::mutex> lock(config_mutex_);

    if (index >= profiles_.size()) {
        return false;
    }

    profiles_[index] = profile;
    return true;
}

std::optional<ConnectionProfile> ConfigManager::get_profile(size_t index) const {
    std::lock_guard<std::mutex> lock(config_mutex_);

    if (index >= profiles_.size()) {
        return std::nullopt;
    }

    return profiles_[index];
}

std::vector<ConnectionProfile> ConfigManager::get_all_profiles() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return profiles_;
}

bool ConfigManager::set_current_profile(size_t index) {
    std::lock_guard<std::mutex> lock(config_mutex_);

    if (index >= profiles_.size()) {
        return false;
    }

    current_profile_index_ = index;
    return true;
}

std::optional<ConnectionProfile> ConfigManager::get_current_profile() const {
    std::lock_guard<std::mutex> lock(config_mutex_);

    if (profiles_.empty()) {
        return std::nullopt;
    }

    if (current_profile_index_ >= profiles_.size()) {
        return std::nullopt;
    }

    return profiles_[current_profile_index_];
}

size_t ConfigManager::get_current_profile_index() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return current_profile_index_;
}

size_t ConfigManager::get_profile_count() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return profiles_.size();
}

} // namespace APFramework
