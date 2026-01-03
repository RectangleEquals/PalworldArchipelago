#include "framework_core.h"
#include <nlohmann/json.hpp>

namespace APFramework {

using json = nlohmann::json;

FrameworkCore::FrameworkCore(const std::string& pipe_name)
    : pipe_name_(pipe_name) {

    // Initialize all components
    config_manager_ = std::make_unique<ConfigManager>();
    capabilities_generator_ = std::make_unique<CapabilitiesGenerator>();
    mod_registry_ = std::make_unique<ModRegistry>();

    // Create logger and setup IPC routing
    logger_ = std::make_unique<Logger>();
    logger_->set_mode(LogMode::FRAMEWORK_ONLY);  // Default mode
    logger_->set_verbosity(LogLevel::INFO);      // Default verbosity

    // Create AP client with default UUID and game name
    ap_client_ = std::make_unique<APClientWrapper>("APFramework", "Palworld");

    // Create IPC server
    ipc_server_ = std::make_unique<IPCServer>(pipe_name);

    // Setup logger callback to forward logs via IPC
    logger_->set_log_callback([this](LogLevel level, const std::string& component, const std::string& message) {
        // Create log message
        IPCMessage log_msg;
        log_msg.type = "log";
        log_msg.mod_id = "APFramework";

        json data;
        data["level"] = Logger::level_to_string(level);
        data["component"] = component;
        data["message"] = message;
        log_msg.data_json = data.dump();

        // Send to all connected mods
        ipc_server_->send_to_all_mods(log_msg);
    });

    // Create message router (depends on IPC server)
    message_router_ = std::make_unique<MessageRouter>(ipc_server_.get());

    // Create polling thread (depends on AP client and message router)
    polling_thread_ = std::make_unique<PollingThread>(ap_client_.get(), message_router_.get());

    // Register IPC message handler
    ipc_server_->set_message_handler([this](const IPCMessage& msg) {
        handle_ipc_message(msg);
    });

    // Log initialization
    logger_->info("FrameworkCore", "APFrameworkCore initialized with pipe: " + pipe_name);
}

FrameworkCore::~FrameworkCore() {
    stop_polling();
    disconnect_ap();
    stop_ipc();
}

// IPC Server Management
void FrameworkCore::start_ipc() {
    logger_->info("IPCServer", "Starting IPC server on pipe: " + pipe_name_);
    ipc_server_->start();
    logger_->info("IPCServer", "IPC server started successfully");
}

void FrameworkCore::stop_ipc() {
    logger_->info("IPCServer", "Stopping IPC server");
    ipc_server_->stop();
}

bool FrameworkCore::is_ipc_running() const {
    return ipc_server_->is_running();
}

// AP Client Management
bool FrameworkCore::connect_ap(const std::string& server, int port,
                               const std::string& slot, const std::string& password) {
    logger_->info("APClient", "Connecting to AP server: " + server + ":" + std::to_string(port) + " as " + slot);
    bool result = ap_client_->connect(server, port, slot, password);
    if (result) {
        logger_->info("APClient", "AP connection initiated successfully");
    } else {
        logger_->error("APClient", "AP connection failed");
    }
    return result;
}

void FrameworkCore::disconnect_ap() {
    logger_->info("APClient", "Disconnecting from AP server");
    ap_client_->disconnect();
}

int FrameworkCore::get_ap_state() const {
    return ap_client_->get_state();
}

bool FrameworkCore::is_ap_connected() const {
    return ap_client_->is_connected();
}

// Mod Discovery and Registration
void FrameworkCore::discover_mods(const std::string& mods_directory) {
    logger_->info("ModRegistry", "Discovering mods in directory: " + mods_directory);
    mod_registry_->discover_mods(mods_directory);
    auto discovered = mod_registry_->get_discovered_mods();
    logger_->info("ModRegistry", "Discovered " + std::to_string(discovered.size()) + " enabled mods");
}

bool FrameworkCore::all_mods_registered() const {
    return mod_registry_->all_discovered_mods_registered();
}

std::string FrameworkCore::generate_capabilities() const {
    logger_->info("CapabilitiesGenerator", "Generating capabilities JSON");
    std::string result = capabilities_generator_->generate_json();
    logger_->info("CapabilitiesGenerator", "Generated " + std::to_string(result.length()) + " bytes of capabilities data");
    return result;
}

// Polling Thread Management
void FrameworkCore::start_polling() {
    logger_->info("PollingThread", "Starting AP polling thread");
    polling_thread_->start();
}

void FrameworkCore::stop_polling() {
    polling_thread_->stop();
}

bool FrameworkCore::is_polling() const {
    return polling_thread_->is_running();
}

// Configuration Management
bool FrameworkCore::load_config(const std::string& config_path) {
    bool result = config_manager_->load_config(config_path);

    if (result) {
        // Apply logging configuration
        auto config = config_manager_->get_config();

        // Set log mode and verbosity
        logger_->set_mode(Logger::string_to_mode(config.log_mode));
        logger_->set_verbosity(Logger::string_to_level(config.log_level));

        // Enable/disable file logging based on config
        if (config.log_to_file) {
            bool file_enabled = logger_->enable_file_logging(config.log_file_path);
            if (file_enabled) {
                logger_->info("FrameworkCore", "File logging enabled: " + config.log_file_path);
            } else {
                logger_->warning("FrameworkCore", "Failed to enable file logging: " + config.log_file_path);
            }
        } else {
            logger_->disable_file_logging();
        }
    }

    return result;
}

bool FrameworkCore::save_config(const std::string& config_path) {
    return config_manager_->save_config(config_path);
}

ConfigManager& FrameworkCore::get_config_manager() {
    return *config_manager_;
}

// IPC Message Handlers
void FrameworkCore::handle_ipc_message(const IPCMessage& msg) {
    if (msg.type == "register") {
        handle_mod_registration(msg.mod_id, msg.data_json);
    }
    else if (msg.type == "location_check") {
        handle_location_check(msg.mod_id, msg.data_json);
    }
    else if (msg.type == "connect") {
        handle_connection_request(msg.mod_id, msg.data_json);
    }
    else if (msg.type == "status_update") {
        handle_status_update(msg.mod_id, msg.data_json);
    }
}

void FrameworkCore::handle_mod_registration(const std::string& mod_id, const std::string& data_json) {
    try {
        logger_->info("ModRegistry", "Registration request from mod: " + mod_id);

        // Special handling for framework mod (allow self-registration)
        if (mod_id == "archipelago.palworld.framework") {
            // Framework mod is registering as a priority client
            // Register in IPC server but skip capabilities merging
            logger_->info("ModRegistry", "Framework mod self-registration (priority client)");
            ipc_server_->register_mod(mod_id);

            // Send registration_complete message back to framework mod
            IPCMessage response;
            response.type = "registration_complete";
            response.mod_id = mod_id;
            response.data_json = "{}";
            ipc_server_->send_to_mod(mod_id, response);

            return;
        }

        // Check if mod is discovered
        ModMetadata* metadata = mod_registry_->get_mod_metadata(mod_id);
        if (!metadata) {
            // Mod not discovered during initialization
            logger_->warning("ModRegistry", "Registration rejected: " + mod_id + " was not discovered");
            send_registration_error(mod_id, "Mod not discovered during initialization");
            return;
        }

        // Check if mod is enabled (dependency/incompatibility check)
        if (!metadata->enabled) {
            // Mod was disabled due to dependency or incompatibility issues
            logger_->warning("ModRegistry", "Registration rejected: " + mod_id + " is disabled");
            send_registration_error(mod_id, "Mod is disabled due to dependency or incompatibility issues");
            return;
        }

        // Normal mod registration
        // Register mod in capabilities generator (which parses the full JSON)
        capabilities_generator_->register_mod_from_config(mod_id, data_json);

        // Parse for ModRegistry (which needs simpler data)
        json data = json::parse(data_json);
        ModCapabilities caps;
        caps.mod_id = mod_id;

        // Parse items array
        if (data.contains("items") && data["items"].is_array()) {
            for (const auto& item : data["items"]) {
                int64_t item_id = 0;
                if (item.is_number_integer()) {
                    item_id = item.get<int64_t>();
                } else if (item.is_object() && item.contains("id")) {
                    item_id = item["id"].get<int64_t>();
                }
                if (item_id != 0) {
                    caps.items.push_back(item_id);
                }
            }
        }

        // Parse locations array
        if (data.contains("locations") && data["locations"].is_array()) {
            for (const auto& location : data["locations"]) {
                int64_t location_id = 0;
                if (location.is_number_integer()) {
                    location_id = location.get<int64_t>();
                } else if (location.is_object() && location.contains("id")) {
                    location_id = location["id"].get<int64_t>();
                }
                if (location_id != 0) {
                    caps.locations.push_back(location_id);
                }
            }
        }

        // Parse regions array
        if (data.contains("regions") && data["regions"].is_array()) {
            for (const auto& region : data["regions"]) {
                if (region.is_string()) {
                    caps.regions.push_back(region.get<std::string>());
                } else if (region.is_object() && region.contains("name")) {
                    caps.regions.push_back(region["name"].get<std::string>());
                }
            }
        }

        // Register mod in registry
        mod_registry_->register_mod(mod_id, caps);

        // Register mod in IPC server
        ipc_server_->register_mod(mod_id);

        // Update message router with this mod's capabilities
        message_router_->register_mod_capabilities(mod_id, caps.items, caps.locations);

        logger_->info("ModRegistry", "Successfully registered mod: " + mod_id +
                     " (Items: " + std::to_string(caps.items.size()) +
                     ", Locations: " + std::to_string(caps.locations.size()) +
                     ", Regions: " + std::to_string(caps.regions.size()) + ")");

        // Check if all mods have registered
        if (all_mods_registered()) {
            logger_->info("ModRegistry", "All discovered mods have registered");
            notify_registration_complete();

            // If autoconnect enabled, connect now
            auto profile = config_manager_->get_active_profile();
            if (profile.autoconnect) {
                logger_->info("FrameworkCore", "Autoconnect enabled - initiating connection");
                connect_ap(profile.server, profile.port,
                          profile.slot_name, profile.password);
            }
        }

    } catch (const std::exception&) {
        // Invalid registration data - ignore
    }
}

void FrameworkCore::handle_location_check(const std::string& mod_id, const std::string& data_json) {
    try {
        json data = json::parse(data_json);

        if (data.contains("location_id") && data["location_id"].is_number_integer()) {
            int64_t location_id = data["location_id"];
            logger_->info("APClient", "Location check from " + mod_id + ": location_id=" + std::to_string(location_id));
            ap_client_->check_location(location_id);
        }

    } catch (const std::exception&) {
        // Invalid data - ignore
    }
}

void FrameworkCore::handle_connection_request(const std::string& mod_id, const std::string& data_json) {
    try {
        json data = json::parse(data_json);

        std::string server = data.value("server", "");
        int port = data.value("port", 38281);
        std::string slot = data.value("slot", "");
        std::string password = data.value("password", "");

        if (!server.empty() && !slot.empty()) {
            logger_->info("FrameworkCore", "Connection request from " + mod_id + " to " + server + ":" + std::to_string(port));
            connect_ap(server, port, slot, password);
        }

    } catch (const std::exception&) {
        // Invalid data - ignore
    }
}

void FrameworkCore::handle_status_update(const std::string& mod_id, const std::string& data_json) {
    try {
        json data = json::parse(data_json);

        if (data.contains("status") && data["status"].is_number_integer()) {
            int status = data["status"];
            logger_->info("APClient", "Status update from " + mod_id + ": status=" + std::to_string(status));
            ap_client_->status_update(status);
        }

    } catch (const std::exception&) {
        // Invalid data - ignore
    }
}

void FrameworkCore::notify_registration_complete() {
    // Generate APCapabilities.json
    std::string caps_json = capabilities_generator_->generate_json();

    // TODO: Save to file (will need path configuration)
    // For now, just generate it - the Lua wrapper can request it

    size_t registered_count = mod_registry_->get_all_registered_mods().size();
    logger_->info("FrameworkCore", "Notifying " + std::to_string(registered_count) +
                 " registered mods that registration is complete");

    // Send registration_complete message to all mods
    IPCMessage msg;
    msg.type = "registration_complete";
    msg.mod_id = "APFramework";

    json data;
    data["registered_count"] = registered_count;
    data["capabilities_ready"] = true;
    msg.data_json = data.dump();

    ipc_server_->send_to_all_mods(msg);
}

void FrameworkCore::send_registration_error(const std::string& mod_id, const std::string& reason) {
    IPCMessage error_msg;
    error_msg.type = "registration_error";
    error_msg.mod_id = mod_id;

    json data;
    data["error"] = reason;
    error_msg.data_json = data.dump();

    ipc_server_->send_to_mod(mod_id, error_msg);
}

} // namespace APFramework
