#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>

namespace APFramework {

/**
 * @brief Generates APCapabilities.json from registered mod data
 *
 * Responsible for creating the Archipelago capabilities file that describes
 * all items, locations, and regions provided by registered mods. This file
 * is used by the Archipelago world generator to create valid multiworld seeds.
 *
 * The generator merges capabilities from all registered mods and handles:
 * - Item ID ranges and names
 * - Location ID ranges and names
 * - Region definitions and connections
 * - Game metadata (version, tags, etc.)
 */
class CapabilitiesGenerator {
public:
    struct ItemDefinition {
        int64_t id;
        std::string name;
        std::string classification; // "filler", "useful", "progression", "trap"
        std::string mod_id; // Which mod provides this item
    };

    struct LocationDefinition {
        int64_t id;
        std::string name;
        std::string region;
        std::string mod_id; // Which mod provides this location
    };

    struct RegionDefinition {
        std::string name;
        std::vector<std::string> connects_to;
        std::vector<int64_t> locations; // Location IDs in this region
        std::string mod_id; // Which mod provides this region
    };

    CapabilitiesGenerator();

    // Add mod capabilities
    void add_mod_items(const std::string& mod_id, const std::vector<ItemDefinition>& items);
    void add_mod_locations(const std::string& mod_id, const std::vector<LocationDefinition>& locations);
    void add_mod_regions(const std::string& mod_id, const std::vector<RegionDefinition>& regions);

    // Bulk registration from mod's ap_config.json
    bool register_mod_from_config(const std::string& mod_id, const std::string& config_json);

    // Generate capabilities file
    std::string generate_json() const;
    bool write_to_file(const std::string& output_path) const;

    // Query capabilities
    int get_total_item_count() const;
    int get_total_location_count() const;
    std::vector<std::string> get_contributing_mods() const;

    // Validation
    bool validate() const;
    std::vector<std::string> get_validation_errors() const;

    // Clear all capabilities
    void clear();

private:
    std::map<int64_t, ItemDefinition> items_;
    std::map<int64_t, LocationDefinition> locations_;
    std::map<std::string, RegionDefinition> regions_;
    std::map<std::string, std::string> game_metadata_;

    mutable std::mutex capabilities_mutex_;

    // Helper methods
    bool validate_item_id_ranges() const;
    bool validate_location_id_ranges() const;
    bool validate_region_connections() const;
};

} // namespace APFramework
