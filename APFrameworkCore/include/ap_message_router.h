#pragma once
#include "ap_types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <mutex>

namespace APFramework {

// Forward declarations
struct APMessage;
struct IPCMessage;
class APIPCServer;
class APLogger;

/**
 * APMessageRouter - Bidirectional message routing between AP server and mods
 *
 * Responsibilities:
 * - Route AP server messages to subscribed mods based on capabilities
 * - Route IPC messages from mods to AP server
 * - Filter messages by capability subscriptions (items, locations)
 * - Handle priority vs regular client routing
 * - Broadcast vs unicast routing
 */
class APMessageRouter {
public:
    APMessageRouter(APLogger* logger);
    ~APMessageRouter();

    // Set reference to IPC server for routing messages to mods
    void set_ipc_server(APIPCServer* ipc_server);

    // Build subscription maps from aggregated capabilities
    // Should be called after capabilities generation during VALIDATING_CAPABILITIES state
    void build_subscriptions(const nlohmann::json& capabilities);

    // Route message from AP server to subscribed mods
    // Called by APPollingThread when messages are received
    void route_ap_message(const APMessage& message);

    // Route message from mod to AP server
    // Called when mod sends location checks, scouts, status updates, etc.
    // Returns the APMessage that should be sent to APClient
    Result<nlohmann::json> route_to_ap_server(const IPCMessage& ipc_message);

    // Get mods subscribed to specific item ID
    std::vector<std::string> get_mods_owning_item(int64_t item_id) const;

    // Get mods subscribed to specific location ID
    std::vector<std::string> get_mods_owning_location(int64_t location_id) const;

    // Get mod that owns a specific item by name (for lookup from AP data package)
    Result<std::string> get_mod_owning_item_name(const std::string& item_name) const;

    // Get mod that owns a specific location by name
    Result<std::string> get_mod_owning_location_name(const std::string& location_name) const;

    // Clear all subscriptions (for resync)
    void clear();

private:
    // Route specific AP message types
    void route_received_items(const nlohmann::json& payload);
    void route_location_info(const nlohmann::json& payload);
    void route_print_json(const nlohmann::json& payload);
    void route_room_update(const nlohmann::json& payload);
    void route_connected(const nlohmann::json& payload);
    void route_connection_refused(const nlohmann::json& payload);

    // Helper to send IPC message to specific mod
    void send_to_mod(const std::string& mod_id, const IPCMessage& message);

    // Helper to broadcast IPC message to all mods
    void broadcast_to_all(const IPCMessage& message);

    APLogger* logger_;  // Non-owning
    APIPCServer* ipc_server_;  // Non-owning

    // Subscription maps: item/location ID -> list of mod IDs that own it
    std::map<int64_t, std::vector<std::string>> item_subscriptions_;
    std::map<int64_t, std::vector<std::string>> location_subscriptions_;

    // Reverse lookup: item/location name -> mod ID (for AP data package lookups)
    std::map<std::string, std::string> item_name_to_mod_;
    std::map<std::string, std::string> location_name_to_mod_;

    mutable std::mutex subscriptions_mutex_;
};

} // namespace APFramework