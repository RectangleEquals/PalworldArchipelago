#include "capabilities_generator.h"
#include <fstream>
#include <set>
#include <nlohmann/json.hpp>

namespace APFramework {

using json = nlohmann::json;

CapabilitiesGenerator::CapabilitiesGenerator() {
}

void CapabilitiesGenerator::add_mod_items(const std::string& mod_id,
                                          const std::vector<ItemDefinition>& items) {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);

    for (const auto& item : items) {
        items_[item.id] = item;
    }
}

void CapabilitiesGenerator::add_mod_locations(const std::string& mod_id,
                                               const std::vector<LocationDefinition>& locations) {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);

    for (const auto& location : locations) {
        locations_[location.id] = location;
    }
}

void CapabilitiesGenerator::add_mod_regions(const std::string& mod_id,
                                            const std::vector<RegionDefinition>& regions) {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);

    for (const auto& region : regions) {
        regions_[region.name] = region;
    }
}

bool CapabilitiesGenerator::register_mod_from_config(const std::string& mod_id,
                                                      const std::string& config_json) {
    try {
        json config = json::parse(config_json);

        std::vector<ItemDefinition> items;
        std::vector<LocationDefinition> locations;
        std::vector<RegionDefinition> regions;

        // Parse items
        if (config.contains("items") && config["items"].is_array()) {
            for (const auto& item_json : config["items"]) {
                ItemDefinition item;
                item.id = item_json.value("id", 0);
                item.name = item_json.value("name", "");
                item.classification = item_json.value("classification", "filler");
                item.mod_id = mod_id;
                items.push_back(item);
            }
        }

        // Parse locations
        if (config.contains("locations") && config["locations"].is_array()) {
            for (const auto& loc_json : config["locations"]) {
                LocationDefinition location;
                location.id = loc_json.value("id", 0);
                location.name = loc_json.value("name", "");
                location.region = loc_json.value("region", "");
                location.mod_id = mod_id;
                locations.push_back(location);
            }
        }

        // Parse regions
        if (config.contains("regions") && config["regions"].is_array()) {
            for (const auto& reg_json : config["regions"]) {
                RegionDefinition region;
                region.name = reg_json.value("name", "");
                region.mod_id = mod_id;

                if (reg_json.contains("connects_to") && reg_json["connects_to"].is_array()) {
                    for (const auto& connection : reg_json["connects_to"]) {
                        if (connection.is_string()) {
                            region.connects_to.push_back(connection.get<std::string>());
                        }
                    }
                }

                if (reg_json.contains("locations") && reg_json["locations"].is_array()) {
                    for (const auto& loc_id : reg_json["locations"]) {
                        if (loc_id.is_number_integer()) {
                            region.locations.push_back(loc_id.get<int64_t>());
                        }
                    }
                }

                regions.push_back(region);
            }
        }

        // Add to generator
        add_mod_items(mod_id, items);
        add_mod_locations(mod_id, locations);
        add_mod_regions(mod_id, regions);

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::string CapabilitiesGenerator::generate_json() const {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);

    try {
        json capabilities;

        // Serialize items
        json items_array = json::array();
        for (const auto& [id, item] : items_) {
            json item_json;
            item_json["id"] = item.id;
            item_json["name"] = item.name;
            item_json["classification"] = item.classification;
            item_json["mod_id"] = item.mod_id;
            items_array.push_back(item_json);
        }
        capabilities["items"] = items_array;

        // Serialize locations
        json locations_array = json::array();
        for (const auto& [id, location] : locations_) {
            json loc_json;
            loc_json["id"] = location.id;
            loc_json["name"] = location.name;
            loc_json["region"] = location.region;
            loc_json["mod_id"] = location.mod_id;
            locations_array.push_back(loc_json);
        }
        capabilities["locations"] = locations_array;

        // Serialize regions
        json regions_array = json::array();
        for (const auto& [name, region] : regions_) {
            json reg_json;
            reg_json["name"] = region.name;
            reg_json["connects_to"] = region.connects_to;
            reg_json["locations"] = region.locations;
            reg_json["mod_id"] = region.mod_id;
            regions_array.push_back(reg_json);
        }
        capabilities["regions"] = regions_array;

        // Add metadata
        capabilities["game"] = "Palworld";
        capabilities["version"] = "2.0.0";

        return capabilities.dump(4); // 4-space indentation
    } catch (const std::exception&) {
        return R"({
    "items": [],
    "locations": [],
    "regions": [],
    "game": "Palworld",
    "version": "2.0.0"
})";
    }
}

bool CapabilitiesGenerator::write_to_file(const std::string& output_path) const {
    try {
        std::string json_str = generate_json();

        std::ofstream file(output_path);
        if (!file.is_open()) {
            return false;
        }

        file << json_str;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

int CapabilitiesGenerator::get_total_item_count() const {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);
    return static_cast<int>(items_.size());
}

int CapabilitiesGenerator::get_total_location_count() const {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);
    return static_cast<int>(locations_.size());
}

std::vector<std::string> CapabilitiesGenerator::get_contributing_mods() const {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);

    std::set<std::string> mod_ids;

    for (const auto& [_, item] : items_) {
        mod_ids.insert(item.mod_id);
    }
    for (const auto& [_, location] : locations_) {
        mod_ids.insert(location.mod_id);
    }
    for (const auto& [_, region] : regions_) {
        mod_ids.insert(region.mod_id);
    }

    return std::vector<std::string>(mod_ids.begin(), mod_ids.end());
}

bool CapabilitiesGenerator::validate() const {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);

    return validate_item_id_ranges() &&
           validate_location_id_ranges() &&
           validate_region_connections();
}

std::vector<std::string> CapabilitiesGenerator::get_validation_errors() const {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);

    std::vector<std::string> errors;

    // Check for duplicate item IDs
    // (Already handled by map, but could check for conflicts)

    // Check for duplicate location IDs
    // (Already handled by map)

    // Check region connections reference valid regions
    for (const auto& [name, region] : regions_) {
        for (const auto& connection : region.connects_to) {
            if (regions_.find(connection) == regions_.end()) {
                errors.push_back("Region '" + name + "' connects to non-existent region '" + connection + "'");
            }
        }

        // Check locations in region exist
        for (int64_t loc_id : region.locations) {
            if (locations_.find(loc_id) == locations_.end()) {
                errors.push_back("Region '" + name + "' references non-existent location ID " + std::to_string(loc_id));
            }
        }
    }

    return errors;
}

void CapabilitiesGenerator::clear() {
    std::lock_guard<std::mutex> lock(capabilities_mutex_);
    items_.clear();
    locations_.clear();
    regions_.clear();
    game_metadata_.clear();
}

bool CapabilitiesGenerator::validate_item_id_ranges() const {
    // Could check for ID range conflicts per mod
    // For now, just return true
    return true;
}

bool CapabilitiesGenerator::validate_location_id_ranges() const {
    // Could check for ID range conflicts per mod
    // For now, just return true
    return true;
}

bool CapabilitiesGenerator::validate_region_connections() const {
    // Check all region connections are valid
    for (const auto& [name, region] : regions_) {
        for (const auto& connection : region.connects_to) {
            if (regions_.find(connection) == regions_.end()) {
                return false;
            }
        }
    }
    return true;
}

} // namespace APFramework
