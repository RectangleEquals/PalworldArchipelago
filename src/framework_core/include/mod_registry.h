#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <cstdint>

namespace APFramework {

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

    // Generate APCapabilities.json from all registered mods
    std::string generate_capabilities_json() const;

    // Clear registry (for testing or reset)
    void clear();

private:
    // Discovered mods (from ap_config.json scanning)
    std::set<std::string> discovered_mods_;

    // Registered mods (completed registration via IPC)
    std::map<std::string, ModCapabilities> registered_mods_;

    mutable std::mutex registry_mutex_;

    // Helper: parse ap_config.json to extract mod_id
    std::string parse_mod_id_from_config(const std::string& config_path) const;
};

} // namespace APFramework
