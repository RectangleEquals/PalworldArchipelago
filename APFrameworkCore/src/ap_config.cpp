#include "ap_config.h"
#include "ap_path_utility.h"
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace APFramework {

APConfig& APConfig::instance() {
    static APConfig instance;
    return instance;
}

bool APConfig::load(const std::filesystem::path& config_path) {
    errors_.clear();

    try {
        // Check if file exists
        if (!std::filesystem::exists(config_path)) {
            errors_.push_back("Config file not found: " + config_path.string());
            return false;
        }

        // Read file
        std::ifstream file(config_path);
        if (!file.is_open()) {
            errors_.push_back("Failed to open config file: " + config_path.string());
            return false;
        }

        // Parse JSON
        json config = json::parse(file);

        // Parse ap_server section
        if (config.contains("ap_server")) {
            auto& ap = config["ap_server"];
            if (ap.contains("server")) ap_server_.server = ap["server"];
            if (ap.contains("port")) ap_server_.port = ap["port"];
            if (ap.contains("slot_name")) ap_server_.slot_name = ap["slot_name"];
            if (ap.contains("password")) ap_server_.password = ap["password"];
            if (ap.contains("auto_reconnect")) ap_server_.auto_reconnect = ap["auto_reconnect"];
            if (ap.contains("reconnect_delay_ms")) ap_server_.reconnect_delay_ms = ap["reconnect_delay_ms"];
            if (ap.contains("connection_timeout_ms")) ap_server_.connection_timeout_ms = ap["connection_timeout_ms"];
        }

        // Parse framework section
        if (config.contains("framework")) {
            auto& fw = config["framework"];
            if (fw.contains("polling_interval_ms")) framework_.polling_interval_ms = fw["polling_interval_ms"];
            if (fw.contains("registration_timeout_ms")) framework_.registration_timeout_ms = fw["registration_timeout_ms"];
            if (fw.contains("priority_registration_timeout_ms"))
                framework_.priority_registration_timeout_ms = fw["priority_registration_timeout_ms"];
            if (fw.contains("mods_directory")) framework_.mods_directory = fw["mods_directory"];
            if (fw.contains("game_name")) framework_.game_name = fw["game_name"];
        }

        // Parse logging section
        if (config.contains("logging")) {
            auto& log = config["logging"];
            if (log.contains("enabled")) logging_.enabled = log["enabled"];
            if (log.contains("level")) logging_.level = parse_log_level(log["level"]);
            if (log.contains("file")) logging_.file = log["file"];
            if (log.contains("console")) logging_.console = log["console"];
        }

        // Parse ipc section
        if (config.contains("ipc")) {
            auto& ipc = config["ipc"];
            if (ipc.contains("endpoint")) ipc_.endpoint = ipc["endpoint"];
            if (ipc.contains("timeout_ms")) ipc_.timeout_ms = ipc["timeout_ms"];
        }

        return validate();

    } catch (const json::exception& e) {
        errors_.push_back(std::string("JSON parse error: ") + e.what());
        return false;
    } catch (const std::exception& e) {
        errors_.push_back(std::string("Config load error: ") + e.what());
        return false;
    }
}

bool APConfig::validate() const {
    errors_.clear();

    // Validate AP server settings
    validate_field(!ap_server_.server.empty(), "ap_server.server cannot be empty");
    validate_field(ap_server_.port > 0 && ap_server_.port < 65536,
                   "ap_server.port must be between 1 and 65535");
    validate_field(!ap_server_.slot_name.empty(), "ap_server.slot_name cannot be empty");
    validate_field(ap_server_.connection_timeout_ms > 0,
                   "ap_server.connection_timeout_ms must be positive");

    // Validate framework settings
    validate_field(framework_.polling_interval_ms > 0 && framework_.polling_interval_ms <= 1000,
                   "framework.polling_interval_ms must be between 1 and 1000");
    validate_field(framework_.registration_timeout_ms >= 1000,
                   "framework.registration_timeout_ms must be at least 1000ms");
    validate_field(framework_.priority_registration_timeout_ms >= 1000,
                   "framework.priority_registration_timeout_ms must be at least 1000ms");
    validate_field(!framework_.mods_directory.empty(),
                   "framework.mods_directory cannot be empty");
    validate_field(!framework_.game_name.empty(),
                   "framework.game_name cannot be empty");

    // Validate mods_directory path exists
    auto mods_path = APPathUtility::resolve_path(framework_.mods_directory);
    if (!mods_path.has_value()) {
        // If user provided an absolute path that doesn't exist, that's an error
        if (APPathUtility::is_absolute(framework_.mods_directory)) {
            errors_.push_back("framework.mods_directory path does not exist: " + framework_.mods_directory);
        } else {
            // If relative, try auto-detecting Mods folder
            auto detected_mods = APPathUtility::find_mods_folder();
            if (!detected_mods.has_value()) {
                errors_.push_back("framework.mods_directory could not be resolved: " + framework_.mods_directory +
                                " (tried resolving relative to DLL, ue4ss, and auto-detection)");
            }
        }
    }

    // Validate logging settings
    validate_field(!logging_.file.empty() || logging_.console,
                   "Must enable either file logging or console logging");

    // If logging to file, validate the directory path exists (or can be created)
    if (!logging_.file.empty() && logging_.enabled) {
        std::filesystem::path log_path(logging_.file);

        // If relative path, it will be resolved relative to DLL directory
        if (!APPathUtility::is_absolute(log_path)) {
            log_path = APPathUtility::to_absolute(log_path);
        }

        // Check if parent directory exists
        auto parent_dir = log_path.parent_path();
        if (!parent_dir.empty() && !APPathUtility::directory_exists(parent_dir)) {
            errors_.push_back("logging.file parent directory does not exist: " + parent_dir.string());
        }
    }

    // Validate IPC settings
    validate_field(!ipc_.endpoint.empty(), "ipc.endpoint cannot be empty");
    validate_field(ipc_.timeout_ms > 0, "ipc.timeout_ms must be positive");

    return errors_.empty();
}

std::vector<std::string> APConfig::get_errors() const {
    return errors_;
}

std::filesystem::path APConfig::get_mods_directory() const {
    // Try to resolve the mods directory path
    auto resolved = APPathUtility::resolve_path(framework_.mods_directory);

    if (resolved.has_value()) {
        return resolved.value();
    }

    // If can't resolve from config, try auto-detecting
    auto detected = APPathUtility::find_mods_folder();
    if (detected.has_value()) {
        return detected.value();
    }

    // Fallback: return as absolute path from DLL directory
    return APPathUtility::to_absolute(framework_.mods_directory);
}

std::filesystem::path APConfig::get_log_file_path() const {
    std::filesystem::path log_path(logging_.file);

    // If absolute, return as-is
    if (APPathUtility::is_absolute(log_path)) {
        return log_path;
    }

    // Otherwise resolve relative to APFrameworkMod root directory
    auto mods_folder = APPathUtility::find_mods_folder();
    if (mods_folder.has_value()) {
        return mods_folder.value() / "APFrameworkMod" / log_path;
    }

    // Fallback: resolve relative to DLL directory
    return APPathUtility::to_absolute(log_path);
}

LogLevel APConfig::parse_log_level(const std::string& level_str) const {
    if (level_str == "trace") return LogLevel::LOG_TRACE;
    if (level_str == "debug") return LogLevel::LOG_DEBUG;
    if (level_str == "info") return LogLevel::LOG_INFO;
    if (level_str == "warn") return LogLevel::LOG_WARN;
    if (level_str == "error") return LogLevel::LOG_ERROR;
    if (level_str == "fatal") return LogLevel::LOG_FATAL;

    errors_.push_back("Unknown log level: " + level_str + ", defaulting to INFO");
    return LogLevel::LOG_INFO;
}

void APConfig::validate_field(bool condition, const std::string& error_msg) const {
    if (!condition) {
        errors_.push_back(error_msg);
    }
}

} // namespace APFramework
