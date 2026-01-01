#include "message_router.h"
#include "ipc_server.h"
#include <nlohmann/json.hpp>

namespace APFramework {

using json = nlohmann::json;

MessageRouter::MessageRouter(IPCServer* ipc_server)
    : ipc_server_(ipc_server) {
}

void MessageRouter::route_ap_message(const APMessage& msg) {
    if (!ipc_server_) {
        return;
    }

    std::lock_guard<std::mutex> lock(routing_mutex_);

    // Route based on message type
    switch (msg.type) {
        case APMessage::Type::ItemReceived: {
            // Find which mod handles this item
            auto it = item_to_mod_.find(msg.item_id);
            if (it != item_to_mod_.end()) {
                // Send to specific mod
                IPCMessage ipc_msg;
                ipc_msg.type = "item_received";
                ipc_msg.mod_id = it->second;
                ipc_msg.data_json = msg.data_json;
                ipc_server_->send_to_mod(it->second, ipc_msg);
            }
            break;
        }

        case APMessage::Type::LocationChecked: {
            // Find which mod handles this location
            auto it = location_to_mod_.find(msg.location_id);
            if (it != location_to_mod_.end()) {
                // Send to specific mod
                IPCMessage ipc_msg;
                ipc_msg.type = "location_checked";
                ipc_msg.mod_id = it->second;
                ipc_msg.data_json = msg.data_json;
                ipc_server_->send_to_mod(it->second, ipc_msg);
            }
            break;
        }

        case APMessage::Type::SlotConnected: {
            // Broadcast to all mods
            IPCMessage ipc_msg;
            ipc_msg.type = "slot_connected";
            ipc_msg.mod_id = "APServer";
            ipc_msg.data_json = msg.data_json;
            ipc_server_->send_to_all_mods(ipc_msg);
            break;
        }

        case APMessage::Type::Disconnected: {
            // Broadcast to all mods
            IPCMessage ipc_msg;
            ipc_msg.type = "disconnected";
            ipc_msg.mod_id = "APServer";
            ipc_msg.data_json = msg.data_json;
            ipc_server_->send_to_all_mods(ipc_msg);
            break;
        }

        case APMessage::Type::RoomInfo: {
            // Broadcast to all mods
            IPCMessage ipc_msg;
            ipc_msg.type = "room_info";
            ipc_msg.mod_id = "APServer";
            ipc_msg.data_json = msg.data_json;
            ipc_server_->send_to_all_mods(ipc_msg);
            break;
        }

        case APMessage::Type::DataPackage: {
            // Broadcast to all mods
            IPCMessage ipc_msg;
            ipc_msg.type = "data_package";
            ipc_msg.mod_id = "APServer";
            ipc_msg.data_json = msg.data_json;
            ipc_server_->send_to_all_mods(ipc_msg);
            break;
        }

        default:
            // Unknown message type - ignore
            break;
    }
}

void MessageRouter::register_item_handler(int64_t item_id, const std::string& mod_id) {
    std::lock_guard<std::mutex> lock(routing_mutex_);
    item_to_mod_[item_id] = mod_id;
}

void MessageRouter::register_location_handler(int64_t location_id, const std::string& mod_id) {
    std::lock_guard<std::mutex> lock(routing_mutex_);
    location_to_mod_[location_id] = mod_id;
}

void MessageRouter::register_mod_capabilities(const std::string& mod_id,
                                              const std::vector<int64_t>& item_ids,
                                              const std::vector<int64_t>& location_ids) {
    std::lock_guard<std::mutex> lock(routing_mutex_);

    // Register all items this mod handles
    for (int64_t item_id : item_ids) {
        item_to_mod_[item_id] = mod_id;
    }

    // Register all locations this mod handles
    for (int64_t location_id : location_ids) {
        location_to_mod_[location_id] = mod_id;
    }
}

void MessageRouter::load_routing_table(const std::string& capabilities_json) {
    std::lock_guard<std::mutex> lock(routing_mutex_);

    try {
        json capabilities = json::parse(capabilities_json);

        // Clear existing routing tables
        item_to_mod_.clear();
        location_to_mod_.clear();

        // Parse items array
        if (capabilities.contains("items") && capabilities["items"].is_array()) {
            for (const auto& item : capabilities["items"]) {
                if (item.contains("id") && item.contains("mod_id")) {
                    int64_t item_id = item["id"];
                    std::string mod_id = item["mod_id"];
                    item_to_mod_[item_id] = mod_id;
                }
            }
        }

        // Parse locations array
        if (capabilities.contains("locations") && capabilities["locations"].is_array()) {
            for (const auto& location : capabilities["locations"]) {
                if (location.contains("id") && location.contains("mod_id")) {
                    int64_t location_id = location["id"];
                    std::string mod_id = location["mod_id"];
                    location_to_mod_[location_id] = mod_id;
                }
            }
        }
    } catch (const std::exception&) {
        // Invalid JSON - routing tables remain cleared
    }
}

std::string MessageRouter::get_item_handler(int64_t item_id) const {
    std::lock_guard<std::mutex> lock(routing_mutex_);

    auto it = item_to_mod_.find(item_id);
    if (it != item_to_mod_.end()) {
        return it->second;
    }

    return "";
}

std::string MessageRouter::get_location_handler(int64_t location_id) const {
    std::lock_guard<std::mutex> lock(routing_mutex_);

    auto it = location_to_mod_.find(location_id);
    if (it != location_to_mod_.end()) {
        return it->second;
    }

    return "";
}

void MessageRouter::clear() {
    std::lock_guard<std::mutex> lock(routing_mutex_);
    item_to_mod_.clear();
    location_to_mod_.clear();
}

} // namespace APFramework
