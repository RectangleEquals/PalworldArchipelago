#include "capabilities_generator.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace APFramework {

using json = nlohmann::json;

CapabilitiesGenerator::CapabilitiesGenerator() {
}

void CapabilitiesGenerator::add_mod_data(const std::string& mod_id, const std::string& capabilities_json) {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);

    try {
        json mod_data = json::parse(capabilities_json);
        mod_capabilities_[mod_id] = mod_data;
    } catch (const std::exception&) {
        // Invalid JSON - skip this mod's data
    }
}

void CapabilitiesGenerator::remove_mod_data(const std::string& mod_id) {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);
    mod_capabilities_.erase(mod_id);
}

std::string CapabilitiesGenerator::generate_capabilities_json() const {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);

    try {
        json capabilities;

        // Initialize arrays
        capabilities["items"] = json::array();
        capabilities["locations"] = json::array();
        capabilities["regions"] = json::array();

        // Aggregate all mod data
        for (const auto& [mod_id, mod_data] : mod_capabilities_) {
            // Merge items
            if (mod_data.contains("items") && mod_data["items"].is_array()) {
                for (const auto& item : mod_data["items"]) {
                    capabilities["items"].push_back(item);
                }
            }

            // Merge locations
            if (mod_data.contains("locations") && mod_data["locations"].is_array()) {
                for (const auto& location : mod_data["locations"]) {
                    capabilities["locations"].push_back(location);
                }
            }

            // Merge regions
            if (mod_data.contains("regions") && mod_data["regions"].is_array()) {
                for (const auto& region : mod_data["regions"]) {
                    capabilities["regions"].push_back(region);
                }
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

bool CapabilitiesGenerator::save_to_file(const std::string& file_path) const {
    try {
        std::string capabilities_json = generate_capabilities_json();

        std::ofstream file(file_path);
        if (!file.is_open()) {
            return false;
        }

        file << capabilities_json;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void CapabilitiesGenerator::clear() {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);
    mod_capabilities_.clear();
}

size_t CapabilitiesGenerator::get_mod_count() const {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);
    return mod_capabilities_.size();
}

bool CapabilitiesGenerator::has_mod_data(const std::string& mod_id) const {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);
    return mod_capabilities_.find(mod_id) != mod_capabilities_.end();
}

std::vector<std::string> CapabilitiesGenerator::get_registered_mods() const {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);

    std::vector<std::string> mod_ids;
    mod_ids.reserve(mod_capabilities_.size());

    for (const auto& [mod_id, _] : mod_capabilities_) {
        mod_ids.push_back(mod_id);
    }

    return mod_ids;
}

} // namespace APFramework
