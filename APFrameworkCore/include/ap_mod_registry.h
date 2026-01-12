#pragma once
#include "ap_types.h"
#include <map>
#include <mutex>
#include <filesystem>

namespace APFramework {

// Forward declarations
class APLogger;

/**
 * APModRegistry - Mod Discovery & Registration Tracking
 *
 * Responsibilities:
 * - Discover mods from Mods/ directory (scan for AP_Config.json)
 * - Track mod registrations (priority vs regular)
 * - Validate mod IDs and detect duplicates
 * - Track registration timeouts
 */
class APModRegistry {
public:
    APModRegistry(APLogger* logger);
    ~APModRegistry();

    // Discover mods in Mods/ directory by scanning for AP_Config.json
    VoidResult discover_mods(const std::filesystem::path& mods_dir);

    // Register mod via IPC (called when mod connects)
    VoidResult register_mod(const std::string& mod_id,
                            const nlohmann::json& capabilities,
                            bool is_priority);

    // Get all registered mods
    std::vector<ModInfo> get_registered_mods() const;

    // Get only priority mods
    std::vector<ModInfo> get_priority_mods() const;

    // Get only regular (non-priority) mods
    std::vector<ModInfo> get_regular_mods() const;

    // Get specific mod info
    Result<ModInfo> get_mod_info(const std::string& mod_id) const;

    // Check if mod is registered
    bool is_mod_registered(const std::string& mod_id) const;

    // Check if all discovered priority mods have registered
    bool all_priority_mods_registered() const;

    // Check if all discovered mods have registered
    bool all_mods_registered() const;

    // Get count of discovered vs registered mods
    size_t get_discovered_count() const;
    size_t get_registered_count() const;
    size_t get_priority_count() const;  // Returns count of REGISTERED priority mods
    size_t get_discovered_priority_count() const;  // Returns count of DISCOVERED priority mods

    // Clear all registrations (for resync)
    void clear();

private:
    struct ModEntry {
        ModInfo info;
        bool discovered{false};      // Found during mod discovery
        bool registered{false};      // Registered via IPC
    };

    // Check if mod_id matches priority client pattern (archipelago.<game>.*)
    bool is_priority_pattern(const std::string& mod_id) const;

    // Load AP_Config.json for a mod
    Result<ModInfo> load_mod_config(const std::filesystem::path& config_path);

    APLogger* logger_;  // Non-owning

    std::map<std::string, ModEntry> mods_;
    mutable std::mutex mods_mutex_;
};

} // namespace APFramework