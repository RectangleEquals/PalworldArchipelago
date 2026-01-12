#include "ap_mod_registry.h"
#include "ap_logger.h"
#include <fstream>
#include <algorithm>

namespace APFramework {

APModRegistry::APModRegistry(APLogger* logger)
    : logger_(logger)
{
}

APModRegistry::~APModRegistry() {
}

VoidResult APModRegistry::discover_mods(const std::filesystem::path& mods_dir) {
    std::lock_guard<std::mutex> lock(mods_mutex_);

    if (!std::filesystem::exists(mods_dir)) {
        return VoidResult::failure(ErrorCode::VALIDATION_ERROR,
            "Mods directory does not exist: " + mods_dir.string());
    }

    if (logger_) {
        logger_->log(LogLevel::LOG_INFO, "APModRegistry", "Discovering mods in: " + mods_dir.string());
    }

    size_t discovered_count = 0;

    // Iterate through subdirectories in Mods/
    for (const auto& entry : std::filesystem::directory_iterator(mods_dir)) {
        if (!entry.is_directory()) {
            continue;
        }

        // Look for AP_Config.json in each mod directory
        auto config_path = entry.path() / "AP_Config.json";
        if (!std::filesystem::exists(config_path)) {
            continue;
        }

        // Load mod config
        auto result = load_mod_config(config_path);
        if (result.is_error()) {
            if (logger_) {
                logger_->log(LogLevel::LOG_WARN, "APModRegistry",
                    "Failed to load config for " + entry.path().filename().string() + ": " + result.error_message);
            }
            continue;
        }

        ModInfo mod_info = result.value;

        // Check if mod already exists
        auto it = mods_.find(mod_info.mod_id);
        if (it != mods_.end()) {
            if (logger_) {
                logger_->log(LogLevel::LOG_WARN, "APModRegistry",
                    "Duplicate mod_id discovered: " + mod_info.mod_id);
            }
            continue;
        }

        // Add to registry as discovered
        ModEntry entry_data;
        entry_data.info = mod_info;
        entry_data.discovered = true;
        entry_data.registered = false;

        mods_[mod_info.mod_id] = entry_data;
        discovered_count++;

        if (logger_) {
            std::string priority_str = mod_info.is_priority ? " (priority)" : "";
            logger_->log(LogLevel::LOG_INFO, "APModRegistry",
                "Discovered mod: " + mod_info.mod_id + priority_str);
        }
    }

    if (logger_) {
        logger_->log(LogLevel::LOG_INFO, "APModRegistry",
            "Discovery complete: " + std::to_string(discovered_count) + " mods found");
    }

    return VoidResult::success();
}

VoidResult APModRegistry::register_mod(const std::string& mod_id,
                                        const nlohmann::json& capabilities,
                                        bool is_priority) {
    std::lock_guard<std::mutex> lock(mods_mutex_);

    auto it = mods_.find(mod_id);

    // If mod was not discovered, add it now (late discovery)
    if (it == mods_.end()) {
        if (logger_) {
            logger_->log(LogLevel::LOG_WARN, "APModRegistry",
                "Registering mod that was not discovered: " + mod_id);
        }

        ModEntry entry;
        entry.info.mod_id = mod_id;
        entry.info.name = mod_id;  // Use mod_id as name if not discovered
        entry.info.version = "unknown";
        entry.info.capabilities = capabilities;
        entry.info.is_priority = is_priority;
        entry.discovered = false;
        entry.registered = true;

        mods_[mod_id] = entry;

        if (logger_) {
            logger_->log(LogLevel::LOG_INFO, "APModRegistry",
                "Registered mod (late discovery): " + mod_id);
        }

        return VoidResult::success();
    }

    // Check if already registered
    if (it->second.registered) {
        return VoidResult::failure(ErrorCode::VALIDATION_ERROR,
            "Mod already registered: " + mod_id);
    }

    // Update registration
    it->second.info.capabilities = capabilities;
    it->second.info.is_priority = is_priority;
    it->second.registered = true;

    if (logger_) {
        std::string priority_str = is_priority ? " (priority)" : "";
        logger_->log(LogLevel::LOG_INFO, "APModRegistry",
            "Registered mod: " + mod_id + priority_str);
    }

    return VoidResult::success();
}

std::vector<ModInfo> APModRegistry::get_registered_mods() const {
    std::lock_guard<std::mutex> lock(mods_mutex_);

    std::vector<ModInfo> result;
    for (const auto& [mod_id, entry] : mods_) {
        if (entry.registered) {
            result.push_back(entry.info);
        }
    }

    return result;
}

std::vector<ModInfo> APModRegistry::get_priority_mods() const {
    std::lock_guard<std::mutex> lock(mods_mutex_);

    std::vector<ModInfo> result;
    for (const auto& [mod_id, entry] : mods_) {
        if (entry.registered && entry.info.is_priority) {
            result.push_back(entry.info);
        }
    }

    return result;
}

std::vector<ModInfo> APModRegistry::get_regular_mods() const {
    std::lock_guard<std::mutex> lock(mods_mutex_);

    std::vector<ModInfo> result;
    for (const auto& [mod_id, entry] : mods_) {
        if (entry.registered && !entry.info.is_priority) {
            result.push_back(entry.info);
        }
    }

    return result;
}

Result<ModInfo> APModRegistry::get_mod_info(const std::string& mod_id) const {
    std::lock_guard<std::mutex> lock(mods_mutex_);

    auto it = mods_.find(mod_id);
    if (it == mods_.end()) {
        return Result<ModInfo>::failure(ErrorCode::VALIDATION_ERROR,
            "Mod not found: " + mod_id);
    }

    return Result<ModInfo>::success(it->second.info);
}

bool APModRegistry::is_mod_registered(const std::string& mod_id) const {
    std::lock_guard<std::mutex> lock(mods_mutex_);

    auto it = mods_.find(mod_id);
    return it != mods_.end() && it->second.registered;
}

bool APModRegistry::all_priority_mods_registered() const {
    std::lock_guard<std::mutex> lock(mods_mutex_);

    for (const auto& [mod_id, entry] : mods_) {
        if (entry.discovered && entry.info.is_priority && !entry.registered) {
            return false;
        }
    }

    return true;
}

bool APModRegistry::all_mods_registered() const {
    std::lock_guard<std::mutex> lock(mods_mutex_);

    for (const auto& [mod_id, entry] : mods_) {
        if (entry.discovered && !entry.registered) {
            return false;
        }
    }

    return true;
}

size_t APModRegistry::get_discovered_count() const {
    std::lock_guard<std::mutex> lock(mods_mutex_);

    size_t count = 0;
    for (const auto& [mod_id, entry] : mods_) {
        if (entry.discovered) {
            count++;
        }
    }

    return count;
}

size_t APModRegistry::get_registered_count() const {
    std::lock_guard<std::mutex> lock(mods_mutex_);

    size_t count = 0;
    for (const auto& [mod_id, entry] : mods_) {
        if (entry.registered) {
            count++;
        }
    }

    return count;
}

size_t APModRegistry::get_priority_count() const {
    std::lock_guard<std::mutex> lock(mods_mutex_);

    size_t count = 0;
    for (const auto& [mod_id, entry] : mods_) {
        if (entry.registered && entry.info.is_priority) {
            count++;
        }
    }

    return count;
}

size_t APModRegistry::get_discovered_priority_count() const {
    std::lock_guard<std::mutex> lock(mods_mutex_);

    size_t count = 0;
    for (const auto& [mod_id, entry] : mods_) {
        if (entry.discovered && entry.info.is_priority) {
            count++;
        }
    }

    return count;
}

void APModRegistry::clear() {
    std::lock_guard<std::mutex> lock(mods_mutex_);

    mods_.clear();

    if (logger_) {
        logger_->log(LogLevel::LOG_INFO, "APModRegistry", "Registry cleared");
    }
}

bool APModRegistry::is_priority_pattern(const std::string& mod_id) const {
    // Priority client pattern: archipelago.<game>.*
    // Example: archipelago.palworld.framework
    return mod_id.rfind("archipelago.", 0) == 0;
}

Result<ModInfo> APModRegistry::load_mod_config(const std::filesystem::path& config_path) {
    // Open and parse AP_Config.json
    std::ifstream file(config_path);
    if (!file.is_open()) {
        return Result<ModInfo>::failure(ErrorCode::CONFIG_ERROR,
            "Failed to open config file: " + config_path.string());
    }

    nlohmann::json config;
    try {
        file >> config;
    } catch (const nlohmann::json::exception& e) {
        return Result<ModInfo>::failure(ErrorCode::CONFIG_ERROR,
            "Failed to parse JSON: " + std::string(e.what()));
    }

    // Validate required fields
    if (!config.contains("mod_id")) {
        return Result<ModInfo>::failure(ErrorCode::CONFIG_ERROR,
            "Missing required field: mod_id");
    }

    ModInfo info;
    info.mod_id = config["mod_id"].get<std::string>();
    info.name = config.value("mod_name", info.mod_id);
    info.version = config.value("version", "1.0.0");
    info.description = config.value("description", "");
    info.config_path = config_path;

    // Check if priority client
    info.is_priority = is_priority_pattern(info.mod_id);

    // Load capabilities (only for non-priority mods)
    if (!info.is_priority && config.contains("capabilities")) {
        info.capabilities = config["capabilities"];
    } else {
        info.capabilities = nlohmann::json::object();
    }

    // Load incompatible mods list (optional)
    if (config.contains("incompatible")) {
        for (const auto& incomp : config["incompatible"]) {
            IncompatibleEntry entry;
            entry.id = incomp["id"].get<std::string>();
            if (incomp.contains("versions")) {
                entry.versions = incomp["versions"].get<std::string>();
            }
            info.incompatible.push_back(entry);
        }
    }

    return Result<ModInfo>::success(info);
}

} // namespace APFramework