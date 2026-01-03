#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <cstdint>
#include <optional>
#include "nlohmann/json.hpp"

namespace APFramework {

using json = nlohmann::json;

/**
 * @brief Forward declaration for IncompatibilitySpec (defined below)
 */
struct IncompatibilitySpec;

/**
 * @brief Version range for dependency specification
 *
 * Supports simple min/max version constraints.
 * Phase 3 will add full semantic versioning support.
 */
struct VersionRange {
    std::string min_version;  // Empty = no minimum
    std::string max_version;  // Empty = no maximum
    bool any_version;         // true = no version constraint

    VersionRange() : any_version(true) {}

    /**
     * @brief Check if a version satisfies this range
     * @param version Version string to check (e.g., "1.0.0")
     * @return true if version is within range
     */
    bool is_satisfied_by(const std::string& version) const;
};

/**
 * @brief Mod metadata from ap_config.json discovery
 *
 * Contains all metadata about a discovered mod including dependencies,
 * incompatibilities, and capabilities.
 */
struct ModMetadata {
    std::string mod_id;
    std::string version;
    std::string display_name;
    std::string description;
    bool enabled = true;
    bool is_framework_mod = false;

    // Dependencies (Feature 1.2)
    std::map<std::string, VersionRange> dependencies;

    // Incompatibilities (Feature 1.3)
    std::map<std::string, IncompatibilitySpec> incompatible_mods;

    // Capabilities
    json capabilities;
};

/**
 * @brief Incompatibility specification for conflict detection
 *
 * Supports three types of incompatibilities:
 * - Complete: No version is compatible
 * - Version range: Specific version range is incompatible
 * - Specific versions: List of specific incompatible versions
 */
struct IncompatibilitySpec {
    enum Type {
        COMPLETE,           // All versions incompatible
        VERSION_RANGE,      // Specific range incompatible
        SPECIFIC_VERSIONS   // Specific versions incompatible
    };

    Type type = COMPLETE;
    VersionRange version_range;                    // Used for VERSION_RANGE
    std::vector<std::string> specific_versions;    // Used for SPECIFIC_VERSIONS
    std::string reason;                            // Human-readable reason

    /**
     * @brief Check if a version is incompatible
     * @param version Version string to check
     * @return true if the version is incompatible
     */
    bool is_incompatible_with(const std::string& version) const;
};

/**
 * @brief Mod capability definition from ap_config.json
 *
 * Describes what items, locations, and regions a mod provides to the
 * Archipelago world. Populated during mod registration.
 */
struct ModCapabilities {
    std::string mod_id;
    std::vector<int64_t> items;
    std::vector<int64_t> locations;
    std::vector<std::string> regions;
};

/**
 * @brief Manages mod discovery, registration, and capability tracking
 *
 * Implements the promise-based registration system where:
 * 1. Discovery phase: Scans for ap_config.json files to find AP-enabled mods
 * 2. Registration phase: Waits for discovered mods to connect via IPC
 * 3. Completion: All discovered mods have registered, capabilities generated
 *
 * Responsibilities:
 * - Scan mod directories for ap_config.json files (auto-discovery)
 * - Track which mods have been discovered vs registered
 * - Store mod capabilities for routing and APCapabilities.json generation
 * - Determine when all expected mods have registered (promise fulfillment)
 * - Generate APCapabilities.json from registered mods
 *
 * The registry ensures the framework knows what mods to expect and doesn't
 * prematurely connect to the AP server before all mods are ready.
 */
class ModRegistry {
public:
    ModRegistry();

    // Discovery phase: scan for ap_config.json files
    void discover_mods(const std::string& mods_directory);
    std::set<std::string> get_discovered_mods() const;

    // Registration phase: mods connect and register capabilities
    void register_mod(const std::string& mod_id, const ModCapabilities& capabilities);
    void unregister_mod(const std::string& mod_id);

    // Query registration status
    bool is_mod_discovered(const std::string& mod_id) const;
    bool is_mod_registered(const std::string& mod_id) const;
    bool all_discovered_mods_registered() const;
    std::vector<std::string> get_pending_registrations() const;

    // Access registered mod data
    ModCapabilities get_mod_capabilities(const std::string& mod_id) const;
    std::vector<std::string> get_all_registered_mods() const;
    std::map<std::string, ModCapabilities> get_all_capabilities() const;

    // Dependency and incompatibility validation (NEW)
    bool validate_dependencies();
    ModMetadata* get_mod_metadata(const std::string& mod_id);

    // Generate APCapabilities.json from all registered mods
    std::string generate_capabilities_json() const;

    // Clear registry (for testing or reset)
    void clear();

private:
    // Discovered mods with full metadata (from ap_config.json scanning)
    std::map<std::string, ModMetadata> discovered_mods_;

    // Registered mods (completed registration via IPC)
    std::map<std::string, ModCapabilities> registered_mods_;

    mutable std::mutex registry_mutex_;

    // Helper methods for dependency/incompatibility system
    bool parse_mod_config(const std::string& config_path, ModMetadata& metadata);
    void parse_dependencies(const json& deps_json, std::map<std::string, VersionRange>& dependencies);
    void parse_incompatibilities(const json& incompat_json, std::map<std::string, IncompatibilitySpec>& incompatibilities);
    void cascade_disable_dependents();
    void check_incompatibilities();
};

} // namespace APFramework
