#pragma once
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <cstdint>
#include "ap_client.h"

namespace APFramework {

class IPCServer; // Forward declaration

/**
 * @brief Routes AP messages to appropriate mods based on item/location ownership
 *
 * Maintains routing tables that map item IDs and location IDs to the mods
 * that handle them. When AP messages arrive (items received, locations checked),
 * the router determines which mod(s) should receive the message and dispatches
 * via the IPC server.
 *
 * Routing information is populated from:
 * - Mod registration messages (capabilities declaration)
 * - APCapabilities.json (pre-generated routing table)
 *
 * Supports:
 * - One-to-one routing (one item/location → one mod)
 * - Broadcast routing (certain messages sent to all mods)
 */
class MessageRouter {
public:
    explicit MessageRouter(IPCServer* ipc_server);

    // Route AP message to appropriate mod(s)
    void route_ap_message(const APMessage& msg);

    // Register which mod handles which items/locations
    void register_item_handler(int64_t item_id, const std::string& mod_id);
    void register_location_handler(int64_t location_id, const std::string& mod_id);

    // Bulk registration from mod capabilities
    void register_mod_capabilities(const std::string& mod_id,
                                  const std::vector<int64_t>& item_ids,
                                  const std::vector<int64_t>& location_ids);

    // Load routing table from APCapabilities.json
    void load_routing_table(const std::string& capabilities_json);

    // Query routing
    std::string get_item_handler(int64_t item_id) const;
    std::string get_location_handler(int64_t location_id) const;

    // Clear all routing tables
    void clear();

private:
    IPCServer* ipc_server_;

    // Routing tables: ID → mod_id
    std::map<int64_t, std::string> item_to_mod_;
    std::map<int64_t, std::string> location_to_mod_;

    mutable std::mutex routing_mutex_;
};

} // namespace APFramework
