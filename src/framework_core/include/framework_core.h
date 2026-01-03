#pragma once
#include <memory>
#include <string>
#include "ap_client.h"
#include "ipc_server.h"
#include "message_router.h"
#include "mod_registry.h"
#include "polling_thread.h"
#include "config_manager.h"
#include "capabilities_generator.h"
#include "logger.h"

namespace APFramework {

/**
 * @brief Main framework core class that coordinates all components
 *
 * FrameworkCore is the central orchestrator that manages:
 * - IPC server for mod communication
 * - AP client for Archipelago server connection
 * - Message routing between AP and mods
 * - Mod discovery and registration
 * - Background polling thread
 * - Configuration management
 *
 * This class is instantiated once per framework instance and provides
 * the implementation for all FFI bindings.
 *
 * Lifecycle:
 * 1. Create: Initialize all components
 * 2. Start IPC: Begin listening for mod connections
 * 3. Discover mods: Scan for ap_config.json files
 * 4. Wait for registration: Mods connect and register
 * 5. Connect to AP: Once ready, connect to AP server
 * 6. Start polling: Begin background AP polling
 * 7. Runtime: Route messages between AP and mods
 * 8. Shutdown: Stop polling, disconnect, cleanup
 */
class FrameworkCore {
public:
    explicit FrameworkCore(const std::string& pipe_name);
    ~FrameworkCore();

    // IPC Server Management
    void start_ipc();
    void stop_ipc();
    bool is_ipc_running() const;

    // AP Client Management
    bool connect_ap(const std::string& server, int port,
                   const std::string& slot, const std::string& password);
    void disconnect_ap();
    int get_ap_state() const;
    bool is_ap_connected() const;

    // Mod Discovery and Registration
    void discover_mods(const std::string& mods_directory);
    bool all_mods_registered() const;
    std::string generate_capabilities() const;

    // Polling Thread Management
    void start_polling();
    void stop_polling();
    bool is_polling() const;

    // Configuration Management
    bool load_config(const std::string& config_path);
    bool save_config(const std::string& config_path);
    ConfigManager& get_config_manager();

private:
    // Core components
    std::unique_ptr<APClientWrapper> ap_client_;
    std::unique_ptr<IPCServer> ipc_server_;
    std::unique_ptr<MessageRouter> message_router_;
    std::unique_ptr<ModRegistry> mod_registry_;
    std::unique_ptr<PollingThread> polling_thread_;
    std::unique_ptr<ConfigManager> config_manager_;
    std::unique_ptr<CapabilitiesGenerator> capabilities_generator_;
    std::unique_ptr<Logger> logger_;

    std::string pipe_name_;

    // IPC message handler - called when mods send messages
    void handle_ipc_message(const IPCMessage& msg);

    // Handle specific message types from mods
    void handle_mod_registration(const std::string& mod_id, const std::string& data_json);
    void handle_location_check(const std::string& mod_id, const std::string& data_json);
    void handle_connection_request(const std::string& mod_id, const std::string& data_json);
    void handle_status_update(const std::string& mod_id, const std::string& data_json);

    // Notify all mods that registration is complete
    void notify_registration_complete();

    // Send registration error to a mod
    void send_registration_error(const std::string& mod_id, const std::string& reason);
};

} // namespace APFramework
