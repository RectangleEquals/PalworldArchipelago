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

    // Create AP client with default UUID and game name
    ap_client_ = std::make_unique<APClientWrapper>("APFramework", "Generic");

    // Create IPC server
    ipc_server_ = std::make_unique<IPCServer>(pipe_name);

    // Create message router (depends on IPC server)
    message_router_ = std::make_unique<MessageRouter>(ipc_server_.get());

    // Create polling thread (depends on AP client and message router)
    polling_thread_ = std::make_unique<PollingThread>(ap_client_.get(), message_router_.get());

    // Register IPC message handler
    ipc_server_->set_message_handler([this](const IPCMessage& msg) {
        handle_ipc_message(msg);
    });
}

FrameworkCore::~FrameworkCore() {
    stop_polling();
    disconnect_ap();
    stop_ipc();
}

// IPC Server Management
void FrameworkCore::start_ipc() {
    ipc_server_->start();
}

void FrameworkCore::stop_ipc() {
    ipc_server_->stop();
}

bool FrameworkCore::is_ipc_running() const {
    return ipc_server_->is_running();
}

// AP Client Management
bool FrameworkCore::connect_ap(const std::string& server, int port,
                               const std::string& slot, const std::string& password) {
    return ap_client_->connect(server, port, slot, password);
}

void FrameworkCore::disconnect_ap() {
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
    mod_registry_->discover_mods(mods_directory);
}

bool FrameworkCore::all_mods_registered() const {
    return mod_registry_->all_discovered_mods_registered();
}

std::string FrameworkCore::generate_capabilities() const {
    return mod_registry_->generate_capabilities_json();
}

// Polling Thread Management
void FrameworkCore::start_polling() {
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
    return config_manager_->load_from_file(config_path);
}

bool FrameworkCore::save_config(const std::string& config_path) {
    return config_manager_->save_to_file(config_path);
}

ConfigManager& FrameworkCore::get_config_manager() {
    return *config_manager_;
}

// IPC Message Handlers
void FrameworkCore::handle_ipc_message(const IPCMessage& msg) {
    if (msg.type == "register") {
        handle_mod_registration(msg.mod_id, msg.data_json);
    } else if (msg.type == "location_check") {
        handle_location_check(msg.mod_id, msg.data_json);
    } else if (msg.type == "connect") {
        handle_connection_request(msg.mod_id, msg.data_json);
    } else if (msg.type == "status_update") {
        handle_status_update(msg.mod_id, msg.data_json);
    }
}

void FrameworkCore::handle_mod_registration(const std::string& mod_id, const std::string& data_json) {
    try {
        json data = json::parse(data_json);

        ModCapabilities caps;
        caps.mod_id = mod_id;

        // Parse items array
        if (data.contains("items") && data["items"].is_array()) {
            for (const auto& item : data["items"]) {
                if (item.is_number_integer()) {
                    caps.items.push_back(item.get<int64_t>());
                }
            }
        }

        // Parse locations array
        if (data.contains("locations") && data["locations"].is_array()) {
            for (const auto& location : data["locations"]) {
                if (location.is_number_integer()) {
                    caps.locations.push_back(location.get<int64_t>());
                }
            }
        }

        // Parse regions array
        if (data.contains("regions") && data["regions"].is_array()) {
            for (const auto& region : data["regions"]) {
                if (region.is_string()) {
                    caps.regions.push_back(region.get<std::string>());
                }
            }
        }

        // Parse optional fields
        caps.logging_only = data.value("logging_only", false);
        caps.priority = data.value("priority", "normal");

        // Register mod in registry
        mod_registry_->register_mod(mod_id, caps);

        // Register mod in IPC server
        ipc_server_->register_mod(mod_id);

        // Update message router with this mod's capabilities
        message_router_->register_mod_capabilities(mod_id, caps.items, caps.locations);

        // Add to capabilities generator
        capabilities_generator_->add_mod_data(mod_id, data_json);

        // Check if all mods have registered
        if (all_mods_registered()) {
            notify_registration_complete();
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
            ap_client_->status_update(status);
        }

    } catch (const std::exception&) {
        // Invalid data - ignore
    }
}

void FrameworkCore::notify_registration_complete() {
    // Send registration_complete message to all mods
    IPCMessage msg;
    msg.type = "registration_complete";
    msg.mod_id = "APFramework";

    json data;
    data["registered_count"] = mod_registry_->get_all_registered_mods().size();
    msg.data_json = data.dump();

    ipc_server_->send_to_all_mods(msg);
}

} // namespace APFramework
