#include "mod_registry.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace APFramework {

using json = nlohmann::json;
namespace fs = std::filesystem;

ModRegistry::ModRegistry() {
}

void ModRegistry::discover_mods(const std::string& mods_directory) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    try {
        // Recursively search for ap_config.json files
        for (const auto& entry : fs::recursive_directory_iterator(mods_directory)) {
            if (entry.is_regular_file() && entry.path().filename() == "ap_config.json") {
                std::string mod_id = parse_mod_id_from_config(entry.path().string());
                if (!mod_id.empty()) {
                    discovered_mods_.insert(mod_id);
                }
            }
        }
    } catch (const fs::filesystem_error&) {
        // Directory doesn't exist or can't be accessed - skip
    }
}

std::set<std::string> ModRegistry::get_discovered_mods() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return discovered_mods_;
}

void ModRegistry::register_mod(const std::string& mod_id, const ModCapabilities& capabilities) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    registered_mods_[mod_id] = capabilities;
}

void ModRegistry::unregister_mod(const std::string& mod_id) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    registered_mods_.erase(mod_id);
}

bool ModRegistry::is_mod_discovered(const std::string& mod_id) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return discovered_mods_.find(mod_id) != discovered_mods_.end();
}

bool ModRegistry::is_mod_registered(const std::string& mod_id) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return registered_mods_.find(mod_id) != registered_mods_.end();
}

bool ModRegistry::all_discovered_mods_registered() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    // Check if every discovered mod has registered
    for (const auto& mod_id : discovered_mods_) {
        if (registered_mods_.find(mod_id) == registered_mods_.end()) {
            return false;
        }
    }

    return true;
}

std::vector<std::string> ModRegistry::get_pending_registrations() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    std::vector<std::string> pending;

    for (const auto& mod_id : discovered_mods_) {
        if (registered_mods_.find(mod_id) == registered_mods_.end()) {
            pending.push_back(mod_id);
        }
    }

    return pending;
}

ModCapabilities ModRegistry::get_mod_capabilities(const std::string& mod_id) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = registered_mods_.find(mod_id);
    if (it != registered_mods_.end()) {
        return it->second;
    }

    return ModCapabilities{}; // Return empty capabilities
}

std::vector<std::string> ModRegistry::get_all_registered_mods() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    std::vector<std::string> mod_ids;
    mod_ids.reserve(registered_mods_.size());

    for (const auto& [mod_id, _] : registered_mods_) {
        mod_ids.push_back(mod_id);
    }

    return mod_ids;
}

std::map<std::string, ModCapabilities> ModRegistry::get_all_capabilities() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return registered_mods_;
}

std::string ModRegistry::generate_capabilities_json() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    try {
        json capabilities;
        capabilities["items"] = json::array();
        capabilities["locations"] = json::array();
        capabilities["regions"] = json::array();

        // Aggregate capabilities from all registered mods
        for (const auto& [mod_id, caps] : registered_mods_) {
            // Add items
            for (int64_t item_id : caps.items) {
                json item;
                item["id"] = item_id;
                item["mod_id"] = mod_id;
                capabilities["items"].push_back(item);
            }

            // Add locations
            for (int64_t location_id : caps.locations) {
                json location;
                location["id"] = location_id;
                location["mod_id"] = mod_id;
                capabilities["locations"].push_back(location);
            }

            // Add regions
            for (const auto& region : caps.regions) {
                json region_obj;
                region_obj["name"] = region;
                region_obj["mod_id"] = mod_id;
                capabilities["regions"].push_back(region_obj);
            }
        }

        return capabilities.dump(4); // 4-space indentation
    } catch (const std::exception&) {
        // Return empty capabilities on error
        return R"({
    "items": [],
    "locations": [],
    "regions": []
})";
    }
}

void ModRegistry::clear() {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    discovered_mods_.clear();
    registered_mods_.clear();
}

std::string ModRegistry::parse_mod_id_from_config(const std::string& config_path) const {
    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            return "";
        }

        json config_json;
        file >> config_json;

        if (config_json.contains("mod_id") && config_json["mod_id"].is_string()) {
            return config_json["mod_id"];
        }

        return "";
    } catch (const std::exception&) {
        return "";
    }
}

} // namespace APFramework
