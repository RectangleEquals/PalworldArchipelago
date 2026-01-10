#pragma once
#include "ap_types.h"
#include <filesystem>

namespace APFramework {

// Forward declarations
class APLogger;

/**
 * APCapabilitiesGenerator - Capability Aggregation & Conflict Detection
 *
 * Responsibilities:
 * - Aggregate capabilities from all registered mods
 * - Compute SHA256 checksum of aggregated capabilities
 * - Detect conflicts (duplicate IDs, names, invalid references)
 * - Save aggregated capabilities to JSON file
 */
class APCapabilitiesGenerator {
public:
    APCapabilitiesGenerator(APLogger* logger);
    ~APCapabilitiesGenerator();

    // Generate aggregated capabilities from all registered mods
    // Includes all items, locations, regions, etc. from all mods
    Result<nlohmann::json> generate_capabilities(
        const std::vector<ModInfo>& mods,
        const std::string& game_name);

    // Compute SHA256 checksum of capabilities JSON
    // Used for ecosystem validation (must match on all players)
    std::string compute_checksum(const nlohmann::json& capabilities);

    // Detect conflicts in aggregated capabilities
    // Returns list of conflicts found (empty if no conflicts)
    std::vector<ConflictInfo> detect_conflicts(const nlohmann::json& capabilities);

    // Save capabilities to file (e.g., palworld_capabilities.json)
    VoidResult save_to_file(const nlohmann::json& capabilities,
                            const std::filesystem::path& output_path);

    // Load capabilities from file (for validation)
    Result<nlohmann::json> load_from_file(const std::filesystem::path& input_path);

private:
    // Aggregate items from all mods
    nlohmann::json aggregate_items(const std::vector<ModInfo>& mods);

    // Aggregate locations from all mods
    nlohmann::json aggregate_locations(const std::vector<ModInfo>& mods);

    // Aggregate regions from all mods
    nlohmann::json aggregate_regions(const std::vector<ModInfo>& mods);

    // Aggregate options from all mods
    nlohmann::json aggregate_options(const std::vector<ModInfo>& mods);

    // Detect duplicate item IDs
    std::vector<ConflictInfo> detect_duplicate_item_ids(const nlohmann::json& capabilities);

    // Detect duplicate location IDs
    std::vector<ConflictInfo> detect_duplicate_location_ids(const nlohmann::json& capabilities);

    // Detect duplicate item names
    std::vector<ConflictInfo> detect_duplicate_item_names(const nlohmann::json& capabilities);

    // Detect duplicate location names
    std::vector<ConflictInfo> detect_duplicate_location_names(const nlohmann::json& capabilities);

    // Detect invalid region references
    std::vector<ConflictInfo> detect_invalid_regions(const nlohmann::json& capabilities);

    // SHA256 hashing helper
    std::string compute_sha256(const std::string& data);

    APLogger* logger_;  // Non-owning
};

} // namespace APFramework