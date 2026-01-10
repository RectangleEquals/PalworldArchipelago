#include "ap_capabilities_generator.h"
#include "ap_logger.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <set>
#include <map>

// Windows CryptoAPI for SHA256
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")
#endif

namespace APFramework {

APCapabilitiesGenerator::APCapabilitiesGenerator(APLogger* logger)
    : logger_(logger)
{
}

APCapabilitiesGenerator::~APCapabilitiesGenerator() {
}

Result<nlohmann::json> APCapabilitiesGenerator::generate_capabilities(
    const std::vector<ModInfo>& mods,
    const std::string& game_name) {

    if (logger_) {
        logger_->log(LogLevel::LOG_INFO, "APCapabilitiesGenerator",
            "Generating capabilities for " + std::to_string(mods.size()) + " mods");
    }

    nlohmann::json capabilities = nlohmann::json::object();

    // Add game name
    capabilities["game"] = game_name;

    // Aggregate items
    capabilities["items"] = aggregate_items(mods);

    // Aggregate locations
    capabilities["locations"] = aggregate_locations(mods);

    // Aggregate regions
    capabilities["regions"] = aggregate_regions(mods);

    // Aggregate options
    capabilities["options"] = aggregate_options(mods);

    if (logger_) {
        logger_->log(LogLevel::LOG_INFO, "APCapabilitiesGenerator", "Capability generation complete");
    }

    return Result<nlohmann::json>::success(capabilities);
}

std::string APCapabilitiesGenerator::compute_checksum(const nlohmann::json& capabilities) {
    // Serialize JSON to string (compact format for consistency)
    std::string json_str = capabilities.dump();

    // Compute SHA256
    return compute_sha256(json_str);
}

std::vector<ConflictInfo> APCapabilitiesGenerator::detect_conflicts(const nlohmann::json& capabilities) {
    std::vector<ConflictInfo> conflicts;

    if (logger_) {
        logger_->log(LogLevel::LOG_INFO, "APCapabilitiesGenerator", "Detecting conflicts...");
    }

    // Detect duplicate item IDs
    auto item_id_conflicts = detect_duplicate_item_ids(capabilities);
    conflicts.insert(conflicts.end(), item_id_conflicts.begin(), item_id_conflicts.end());

    // Detect duplicate location IDs
    auto loc_id_conflicts = detect_duplicate_location_ids(capabilities);
    conflicts.insert(conflicts.end(), loc_id_conflicts.begin(), loc_id_conflicts.end());

    // Detect duplicate item names
    auto item_name_conflicts = detect_duplicate_item_names(capabilities);
    conflicts.insert(conflicts.end(), item_name_conflicts.begin(), item_name_conflicts.end());

    // Detect duplicate location names
    auto loc_name_conflicts = detect_duplicate_location_names(capabilities);
    conflicts.insert(conflicts.end(), loc_name_conflicts.begin(), loc_name_conflicts.end());

    // Detect invalid region references
    auto region_conflicts = detect_invalid_regions(capabilities);
    conflicts.insert(conflicts.end(), region_conflicts.begin(), region_conflicts.end());

    if (logger_) {
        logger_->log(LogLevel::LOG_INFO, "APCapabilitiesGenerator",
            "Conflict detection complete: " + std::to_string(conflicts.size()) + " conflicts found");
    }

    return conflicts;
}

VoidResult APCapabilitiesGenerator::save_to_file(const nlohmann::json& capabilities,
                                                  const std::filesystem::path& output_path) {
    try {
        std::ofstream file(output_path);
        if (!file.is_open()) {
            return VoidResult::failure(ErrorCode::CONFIG_ERROR,
                "Failed to open output file: " + output_path.string());
        }

        // Write formatted JSON
        file << capabilities.dump(2);  // indent with 2 spaces
        file.close();

        if (logger_) {
            logger_->log(LogLevel::LOG_INFO, "APCapabilitiesGenerator",
                "Capabilities saved to: " + output_path.string());
        }

        return VoidResult::success();
    } catch (const std::exception& e) {
        return VoidResult::failure(ErrorCode::CONFIG_ERROR,
            "Failed to save capabilities: " + std::string(e.what()));
    }
}

Result<nlohmann::json> APCapabilitiesGenerator::load_from_file(const std::filesystem::path& input_path) {
    try {
        std::ifstream file(input_path);
        if (!file.is_open()) {
            return Result<nlohmann::json>::failure(ErrorCode::CONFIG_ERROR,
                "Failed to open input file: " + input_path.string());
        }

        nlohmann::json capabilities;
        file >> capabilities;

        if (logger_) {
            logger_->log(LogLevel::LOG_INFO, "APCapabilitiesGenerator",
                "Capabilities loaded from: " + input_path.string());
        }

        return Result<nlohmann::json>::success(capabilities);
    } catch (const nlohmann::json::exception& e) {
        return Result<nlohmann::json>::failure(ErrorCode::CONFIG_ERROR,
            "Failed to parse JSON: " + std::string(e.what()));
    }
}

// Private helper methods

nlohmann::json APCapabilitiesGenerator::aggregate_items(const std::vector<ModInfo>& mods) {
    nlohmann::json items = nlohmann::json::array();

    for (const auto& mod : mods) {
        if (!mod.capabilities.contains("items")) {
            continue;
        }

        const auto& mod_items = mod.capabilities["items"];
        if (mod_items.is_array()) {
            for (const auto& item : mod_items) {
                nlohmann::json item_with_mod = item;
                item_with_mod["_mod_id"] = mod.mod_id;  // Track ownership
                items.push_back(item_with_mod);
            }
        }
    }

    return items;
}

nlohmann::json APCapabilitiesGenerator::aggregate_locations(const std::vector<ModInfo>& mods) {
    nlohmann::json locations = nlohmann::json::array();

    for (const auto& mod : mods) {
        if (!mod.capabilities.contains("locations")) {
            continue;
        }

        const auto& mod_locations = mod.capabilities["locations"];
        if (mod_locations.is_array()) {
            for (const auto& location : mod_locations) {
                nlohmann::json loc_with_mod = location;
                loc_with_mod["_mod_id"] = mod.mod_id;  // Track ownership
                locations.push_back(loc_with_mod);
            }
        }
    }

    return locations;
}

nlohmann::json APCapabilitiesGenerator::aggregate_regions(const std::vector<ModInfo>& mods) {
    nlohmann::json regions = nlohmann::json::array();

    for (const auto& mod : mods) {
        if (!mod.capabilities.contains("regions")) {
            continue;
        }

        const auto& mod_regions = mod.capabilities["regions"];
        if (mod_regions.is_array()) {
            for (const auto& region : mod_regions) {
                nlohmann::json region_with_mod = region;
                region_with_mod["_mod_id"] = mod.mod_id;  // Track ownership
                regions.push_back(region_with_mod);
            }
        }
    }

    return regions;
}

nlohmann::json APCapabilitiesGenerator::aggregate_options(const std::vector<ModInfo>& mods) {
    nlohmann::json options = nlohmann::json::array();

    for (const auto& mod : mods) {
        if (!mod.capabilities.contains("options")) {
            continue;
        }

        const auto& mod_options = mod.capabilities["options"];
        if (mod_options.is_array()) {
            for (const auto& option : mod_options) {
                nlohmann::json opt_with_mod = option;
                opt_with_mod["_mod_id"] = mod.mod_id;  // Track ownership
                options.push_back(opt_with_mod);
            }
        }
    }

    return options;
}

std::vector<ConflictInfo> APCapabilitiesGenerator::detect_duplicate_item_ids(const nlohmann::json& capabilities) {
    std::vector<ConflictInfo> conflicts;

    if (!capabilities.contains("items") || !capabilities["items"].is_array()) {
        return conflicts;
    }

    std::map<int64_t, std::vector<std::string>> item_id_to_mods;

    for (const auto& item : capabilities["items"]) {
        if (item.contains("id") && item.contains("_mod_id")) {
            int64_t id = item["id"].get<int64_t>();
            std::string mod_id = item["_mod_id"].get<std::string>();
            item_id_to_mods[id].push_back(mod_id);
        }
    }

    // Check for duplicates
    for (const auto& [id, mods] : item_id_to_mods) {
        if (mods.size() > 1) {
            ConflictInfo conflict;
            conflict.type = ConflictType::DUPLICATE_ITEM_ID;
            conflict.description = "Duplicate item ID found";
            conflict.involved_mods = mods;
            conflict.details = "item_id: " + std::to_string(id);
            conflicts.push_back(conflict);
        }
    }

    return conflicts;
}

std::vector<ConflictInfo> APCapabilitiesGenerator::detect_duplicate_location_ids(const nlohmann::json& capabilities) {
    std::vector<ConflictInfo> conflicts;

    if (!capabilities.contains("locations") || !capabilities["locations"].is_array()) {
        return conflicts;
    }

    std::map<int64_t, std::vector<std::string>> loc_id_to_mods;

    for (const auto& location : capabilities["locations"]) {
        if (location.contains("id") && location.contains("_mod_id")) {
            int64_t id = location["id"].get<int64_t>();
            std::string mod_id = location["_mod_id"].get<std::string>();
            loc_id_to_mods[id].push_back(mod_id);
        }
    }

    // Check for duplicates
    for (const auto& [id, mods] : loc_id_to_mods) {
        if (mods.size() > 1) {
            ConflictInfo conflict;
            conflict.type = ConflictType::DUPLICATE_LOCATION_ID;
            conflict.description = "Duplicate location ID found";
            conflict.involved_mods = mods;
            conflict.details = "location_id: " + std::to_string(id);
            conflicts.push_back(conflict);
        }
    }

    return conflicts;
}

std::vector<ConflictInfo> APCapabilitiesGenerator::detect_duplicate_item_names(const nlohmann::json& capabilities) {
    std::vector<ConflictInfo> conflicts;

    if (!capabilities.contains("items") || !capabilities["items"].is_array()) {
        return conflicts;
    }

    std::map<std::string, std::vector<std::string>> name_to_mods;

    for (const auto& item : capabilities["items"]) {
        if (item.contains("name") && item.contains("_mod_id")) {
            std::string name = item["name"].get<std::string>();
            std::string mod_id = item["_mod_id"].get<std::string>();
            name_to_mods[name].push_back(mod_id);
        }
    }

    // Check for duplicates
    for (const auto& [name, mods] : name_to_mods) {
        if (mods.size() > 1) {
            ConflictInfo conflict;
            conflict.type = ConflictType::DUPLICATE_ITEM_NAME;
            conflict.description = "Duplicate item name found";
            conflict.involved_mods = mods;
            conflict.details = "item_name: " + name;
            conflicts.push_back(conflict);
        }
    }

    return conflicts;
}

std::vector<ConflictInfo> APCapabilitiesGenerator::detect_duplicate_location_names(const nlohmann::json& capabilities) {
    std::vector<ConflictInfo> conflicts;

    if (!capabilities.contains("locations") || !capabilities["locations"].is_array()) {
        return conflicts;
    }

    std::map<std::string, std::vector<std::string>> name_to_mods;

    for (const auto& location : capabilities["locations"]) {
        if (location.contains("name") && location.contains("_mod_id")) {
            std::string name = location["name"].get<std::string>();
            std::string mod_id = location["_mod_id"].get<std::string>();
            name_to_mods[name].push_back(mod_id);
        }
    }

    // Check for duplicates
    for (const auto& [name, mods] : name_to_mods) {
        if (mods.size() > 1) {
            ConflictInfo conflict;
            conflict.type = ConflictType::DUPLICATE_LOCATION_NAME;
            conflict.description = "Duplicate location name found";
            conflict.involved_mods = mods;
            conflict.details = "location_name: " + name;
            conflicts.push_back(conflict);
        }
    }

    return conflicts;
}

std::vector<ConflictInfo> APCapabilitiesGenerator::detect_invalid_regions(const nlohmann::json& capabilities) {
    std::vector<ConflictInfo> conflicts;

    if (!capabilities.contains("regions") || !capabilities["regions"].is_array()) {
        return conflicts;
    }

    if (!capabilities.contains("locations") || !capabilities["locations"].is_array()) {
        return conflicts;
    }

    // Build set of valid region names
    std::set<std::string> valid_regions;
    for (const auto& region : capabilities["regions"]) {
        if (region.contains("name")) {
            valid_regions.insert(region["name"].get<std::string>());
        }
    }

    // Check location region references
    for (const auto& location : capabilities["locations"]) {
        if (location.contains("region") && location.contains("_mod_id")) {
            std::string region = location["region"].get<std::string>();
            if (valid_regions.find(region) == valid_regions.end()) {
                ConflictInfo conflict;
                conflict.type = ConflictType::INVALID_REGION_REFERENCE;
                conflict.description = "Location references non-existent region";
                conflict.involved_mods.push_back(location["_mod_id"].get<std::string>());
                conflict.details = "location: " + location["name"].get<std::string>() +
                                   ", invalid_region: " + region;
                conflicts.push_back(conflict);
            }
        }
    }

    return conflicts;
}

std::string APCapabilitiesGenerator::compute_sha256(const std::string& data) {
#ifdef _WIN32
    // Use Windows CryptoAPI
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string result;

    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            if (CryptHashData(hHash, (BYTE*)data.c_str(), (DWORD)data.size(), 0)) {
                DWORD hashLen = 0;
                DWORD hashLenSize = sizeof(DWORD);
                if (CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashLen, &hashLenSize, 0)) {
                    std::vector<BYTE> hash(hashLen);
                    if (CryptGetHashParam(hHash, HP_HASHVAL, hash.data(), &hashLen, 0)) {
                        // Convert to hex string
                        std::ostringstream oss;
                        for (BYTE b : hash) {
                            oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
                        }
                        result = oss.str();
                    }
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }

    return result;
#else
    // Fallback: simple hash (not cryptographic!) for non-Windows platforms
    // In production, this should use OpenSSL or similar
    size_t hash = 0;
    for (char c : data) {
        hash = hash * 31 + c;
    }
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
#endif
}

} // namespace APFramework