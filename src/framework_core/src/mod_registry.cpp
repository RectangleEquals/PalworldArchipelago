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
                ModMetadata metadata;
                if (parse_mod_config(entry.path().string(), metadata)) {
                    // Check if mod is enabled
                    if (!metadata.enabled) {
                        // Skip disabled mods
                        continue;
                    }

                    // Add to discovered mods
                    discovered_mods_[metadata.mod_id] = metadata;
                }
            }
        }

        // Validate dependencies and incompatibilities
        validate_dependencies();
        check_incompatibilities();

    } catch (const fs::filesystem_error&) {
        // Directory doesn't exist or can't be accessed - skip
    }
}

std::set<std::string> ModRegistry::get_discovered_mods() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    std::set<std::string> mod_ids;
    for (const auto& [mod_id, metadata] : discovered_mods_) {
        if (metadata.enabled) {
            mod_ids.insert(mod_id);
        }
    }
    return mod_ids;
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
    auto it = discovered_mods_.find(mod_id);
    return it != discovered_mods_.end() && it->second.enabled;
}

bool ModRegistry::is_mod_registered(const std::string& mod_id) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return registered_mods_.find(mod_id) != registered_mods_.end();
}

bool ModRegistry::all_discovered_mods_registered() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    // Check if every discovered and enabled mod has registered
    for (const auto& [mod_id, metadata] : discovered_mods_) {
        if (metadata.enabled && registered_mods_.find(mod_id) == registered_mods_.end()) {
            return false;
        }
    }

    return true;
}

std::vector<std::string> ModRegistry::get_pending_registrations() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    std::vector<std::string> pending;

    for (const auto& [mod_id, metadata] : discovered_mods_) {
        if (metadata.enabled && registered_mods_.find(mod_id) == registered_mods_.end()) {
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

ModMetadata* ModRegistry::get_mod_metadata(const std::string& mod_id) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = discovered_mods_.find(mod_id);
    if (it != discovered_mods_.end()) {
        return &it->second;
    }

    return nullptr;
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

// NEW: Parse mod config file
bool ModRegistry::parse_mod_config(const std::string& config_path, ModMetadata& metadata) {
    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            return false;
        }

        json config;
        file >> config;

        // Required fields
        if (!config.contains("mod_id") || !config["mod_id"].is_string()) {
            return false;
        }
        metadata.mod_id = config["mod_id"];

        // Optional fields
        metadata.version = config.value("version", "0.0.0");
        metadata.display_name = config.value("display_name", metadata.mod_id);
        metadata.description = config.value("description", "");
        metadata.enabled = config.value("enabled", true);

        // Parse dependencies
        if (config.contains("dependencies")) {
            parse_dependencies(config["dependencies"], metadata.dependencies);
        }

        // Parse incompatibilities
        if (config.contains("incompatible_mods")) {
            parse_incompatibilities(config["incompatible_mods"], metadata.incompatible_mods);
        }

        // Store full capabilities JSON
        if (config.contains("capabilities")) {
            metadata.capabilities = config["capabilities"];
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// NEW: Parse dependencies from JSON
void ModRegistry::parse_dependencies(const json& deps_json, std::map<std::string, VersionRange>& dependencies) {
    if (!deps_json.is_object()) {
        return;
    }

    for (auto& [mod_id, spec] : deps_json.items()) {
        VersionRange range;

        if (spec.is_boolean() && spec.get<bool>()) {
            // "mod.id": true → any version
            range.any_version = true;
        } else if (spec.is_object()) {
            // "mod.id": {"min_version": "1.0", "max_version": "2.0"}
            range.any_version = false;
            range.min_version = spec.value("min_version", "");
            range.max_version = spec.value("max_version", "");
        } else {
            // Invalid spec, skip
            continue;
        }

        dependencies[mod_id] = range;
    }
}

// NEW: Parse incompatibilities from JSON
void ModRegistry::parse_incompatibilities(const json& incompat_json, std::map<std::string, IncompatibilitySpec>& incompatibilities) {
    if (!incompat_json.is_object()) {
        return;
    }

    for (auto& [mod_id, spec] : incompat_json.items()) {
        IncompatibilitySpec incompat;

        if (spec.is_boolean() && spec.get<bool>()) {
            // "mod.id": true → completely incompatible
            incompat.type = IncompatibilitySpec::COMPLETE;
            incompat.reason = "Complete incompatibility";
        } else if (spec.is_object()) {
            std::string type_str = spec.value("type", "complete");

            if (type_str == "complete") {
                incompat.type = IncompatibilitySpec::COMPLETE;
            } else if (type_str == "version_range") {
                incompat.type = IncompatibilitySpec::VERSION_RANGE;
                incompat.version_range.min_version = spec.value("min_version", "");
                incompat.version_range.max_version = spec.value("max_version", "");
                incompat.version_range.any_version = false;
            } else if (type_str == "specific_versions") {
                incompat.type = IncompatibilitySpec::SPECIFIC_VERSIONS;
                if (spec.contains("versions") && spec["versions"].is_array()) {
                    for (const auto& v : spec["versions"]) {
                        if (v.is_string()) {
                            incompat.specific_versions.push_back(v.get<std::string>());
                        }
                    }
                }
            }

            incompat.reason = spec.value("reason", "Incompatible mod");
        } else {
            continue;
        }

        incompatibilities[mod_id] = incompat;
    }
}

// NEW: Validate dependencies
bool ModRegistry::validate_dependencies() {
    std::vector<std::string> mods_to_disable;

    for (auto& [mod_id, metadata] : discovered_mods_) {
        if (!metadata.enabled) continue;

        // Check each dependency
        for (const auto& [dep_mod_id, version_range] : metadata.dependencies) {
            // Check if dependency exists
            auto it = discovered_mods_.find(dep_mod_id);
            if (it == discovered_mods_.end()) {
                // Dependency not installed
                mods_to_disable.push_back(mod_id);
                break;
            }

            // Check if dependency is enabled
            if (!it->second.enabled) {
                // Dependency is disabled
                mods_to_disable.push_back(mod_id);
                break;
            }

            // Check version constraint
            if (!version_range.is_satisfied_by(it->second.version)) {
                // Version doesn't satisfy constraint
                mods_to_disable.push_back(mod_id);
                break;
            }
        }
    }

    // Disable mods with missing/invalid dependencies
    for (const auto& mod_id : mods_to_disable) {
        discovered_mods_[mod_id].enabled = false;
    }

    // Cascade disable: disable mods that depend on disabled mods
    cascade_disable_dependents();

    return mods_to_disable.empty();
}

// NEW: Cascade disable dependents
void ModRegistry::cascade_disable_dependents() {
    bool changed = true;

    while (changed) {
        changed = false;

        for (auto& [mod_id, metadata] : discovered_mods_) {
            if (!metadata.enabled) continue;

            // Check if any dependency is disabled
            for (const auto& [dep_mod_id, _] : metadata.dependencies) {
                auto it = discovered_mods_.find(dep_mod_id);
                if (it != discovered_mods_.end() && !it->second.enabled) {
                    // Dependency is disabled, cascade disable this mod
                    metadata.enabled = false;
                    changed = true;
                    break;
                }
            }
        }
    }
}

// NEW: Check incompatibilities
void ModRegistry::check_incompatibilities() {
    std::vector<std::string> mods_to_disable;

    for (auto& [mod_id, metadata] : discovered_mods_) {
        if (!metadata.enabled) continue;

        // Check each incompatibility
        for (const auto& [incompat_mod_id, incompat_spec] : metadata.incompatible_mods) {
            // Check if incompatible mod exists and is enabled
            auto it = discovered_mods_.find(incompat_mod_id);
            if (it != discovered_mods_.end() && it->second.enabled) {
                // Check if version is incompatible
                if (incompat_spec.is_incompatible_with(it->second.version)) {
                    // Conflict detected - disable this mod
                    mods_to_disable.push_back(mod_id);
                    break;
                }
            }
        }
    }

    // Disable conflicting mods
    for (const auto& mod_id : mods_to_disable) {
        discovered_mods_[mod_id].enabled = false;
    }
}

// VersionRange implementation
bool VersionRange::is_satisfied_by(const std::string& version) const {
    if (any_version) {
        return true;
    }

    // Simple string comparison (Phase 3 will add semantic versioning)
    // For now, just check min/max bounds with string comparison
    // Note: This is not semantically correct but serves as placeholder

    if (!min_version.empty() && version < min_version) {
        return false;
    }

    if (!max_version.empty() && version > max_version) {
        return false;
    }

    return true;
}

// IncompatibilitySpec implementation
bool IncompatibilitySpec::is_incompatible_with(const std::string& version) const {
    switch (type) {
        case COMPLETE:
            // All versions are incompatible
            return true;

        case VERSION_RANGE:
            // Check if version is in incompatible range
            return version_range.is_satisfied_by(version);

        case SPECIFIC_VERSIONS:
            // Check if version matches any specific version
            for (const auto& v : specific_versions) {
                if (version == v) {
                    return true;
                }
            }
            return false;

        default:
            return false;
    }
}

} // namespace APFramework