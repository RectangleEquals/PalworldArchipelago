#pragma once
#include "ap_ipc_client.h"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace APClientLib {

/**
 * Register APClientLib bindings with a Lua state
 *
 * @param lua The sol::state_view to register bindings with (can be an existing Lua state)
 */
void register_apclient_bindings(sol::state_view lua);

/**
 * APClient - High-level client library for AP-enabled mods
 *
 * Provides a simple API for mods to communicate with the APFramework.
 * Handles IPC connection, message routing, and provides convenience methods
 * for common operations (location checks, item receipts, etc.)
 *
 * Usage:
 *   APClient client;
 *   client.init("mymod.palworld.example");
 *   client.on_received_items([](const nlohmann::json& items) {
 *       // Handle received items
 *   });
 *   client.send_location_check(location_id);
 */
class APClient {
public:
    APClient();
    ~APClient();

    // Initialize and connect to framework
    // @param mod_id Unique mod identifier (e.g., "mymod.palworld.example")
    // @param pipe_name Optional pipe name (defaults to framework default)
    // @return VoidResult indicating success/failure
    VoidResult init(const std::string& mod_id,
                   const std::string& pipe_name = "\\\\.\\pipe\\APFramework");

    // Register mod with framework (called automatically by init)
    // @param capabilities JSON object with mod capabilities (optional for priority clients)
    // @param is_priority Whether this is a priority client
    // @return VoidResult indicating success/failure
    VoidResult register_with_framework(const nlohmann::json& capabilities,
                                      bool is_priority = false);

    // Send a location check to the framework
    // @param location_id AP location ID to check
    // @return VoidResult indicating success/failure
    VoidResult send_location_check(int64_t location_id);

    // Send multiple location checks (batched)
    // @param location_ids Vector of AP location IDs to check
    // @return VoidResult indicating success/failure
    VoidResult send_location_checks(const std::vector<int64_t>& location_ids);

    // Send a custom message to the framework or another mod
    // @param message IPCMessage to send
    // @return VoidResult indicating success/failure
    VoidResult send_message(const IPCMessage& message);

    // Send a command to the framework (priority clients only)
    // @param cmd Command name (e.g., "CONNECT", "DISCONNECT")
    // @param data Optional command data
    // @return VoidResult indicating success/failure
    VoidResult send_command(const std::string& cmd, const nlohmann::json& data = {});

    // Poll for messages from framework (call regularly, e.g., every tick)
    // This processes incoming messages and triggers registered callbacks
    void poll();

    // Get raw pending messages (alternative to callbacks)
    // @return Vector of pending IPCMessages
    std::vector<IPCMessage> get_messages();

    // Callback types
    using ReceivedItemsCallback = std::function<void(const nlohmann::json&)>;
    using LocationInfoCallback = std::function<void(const nlohmann::json&)>;
    using GenericMessageCallback = std::function<void(const IPCMessage&)>;

    // Register callback for received items from AP server
    // @param callback Function to call when items are received
    void on_received_items(ReceivedItemsCallback callback);

    // Register callback for location info updates
    // @param callback Function to call when location info is received
    void on_location_info(LocationInfoCallback callback);

    // Register callback for lifecycle phase changes
    // @param callback Function to call when framework phase changes
    void on_lifecycle_change(GenericMessageCallback callback);

    // Register callback for any message type
    // @param message_type Type of message to listen for
    // @param callback Function to call when message is received
    void on_message(const std::string& message_type, GenericMessageCallback callback);

    // Connection status
    bool is_connected() const;

    // Get mod ID
    std::string get_mod_id() const { return mod_id_; }

private:
    void handle_incoming_message(const IPCMessage& msg);

    std::unique_ptr<APIPCClient> ipc_client_;
    std::string mod_id_;

    // Callbacks
    ReceivedItemsCallback received_items_callback_;
    LocationInfoCallback location_info_callback_;
    GenericMessageCallback lifecycle_callback_;
    std::map<std::string, GenericMessageCallback> message_callbacks_;
};

} // namespace APClientLib