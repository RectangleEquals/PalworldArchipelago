#include "ap_message_router.h"
#include "ap_client.h"  // For APMessage
#include "ap_ipc_server.h"
#include "ap_logger.h"
#include <sstream>

namespace APFramework {

APMessageRouter::APMessageRouter(APLogger* logger)
    : logger_(logger), ipc_server_(nullptr) {
}

APMessageRouter::~APMessageRouter() {
    clear();
}

void APMessageRouter::set_ipc_server(APIPCServer* ipc_server) {
    ipc_server_ = ipc_server;
}

void APMessageRouter::build_subscriptions(const nlohmann::json& capabilities) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    // Clear existing subscriptions
    item_subscriptions_.clear();
    location_subscriptions_.clear();
    item_name_to_mod_.clear();
    location_name_to_mod_.clear();

    if (!capabilities.contains("items") || !capabilities.contains("locations")) {
        logger_->log(LogLevel::LOG_WARN, "APMessageRouter", "Capabilities missing items or locations");
        return;
    }

    // Build item subscriptions
    const auto& items = capabilities["items"];
    if (items.is_array()) {
        for (const auto& item : items) {
            if (!item.contains("id") || !item.contains("_mod_id")) {
                continue;
            }

            int64_t item_id = item["id"].get<int64_t>();
            std::string mod_id = item["_mod_id"].get<std::string>();

            // Add to ID-based subscription map
            item_subscriptions_[item_id].push_back(mod_id);

            // Add to name-based lookup (if name exists)
            if (item.contains("name")) {
                std::string item_name = item["name"].get<std::string>();
                item_name_to_mod_[item_name] = mod_id;
            }
        }
    }

    // Build location subscriptions
    const auto& locations = capabilities["locations"];
    if (locations.is_array()) {
        for (const auto& location : locations) {
            if (!location.contains("id") || !location.contains("_mod_id")) {
                continue;
            }

            int64_t location_id = location["id"].get<int64_t>();
            std::string mod_id = location["_mod_id"].get<std::string>();

            // Add to ID-based subscription map
            location_subscriptions_[location_id].push_back(mod_id);

            // Add to name-based lookup (if name exists)
            if (location.contains("name")) {
                std::string location_name = location["name"].get<std::string>();
                location_name_to_mod_[location_name] = mod_id;
            }
        }
    }

    std::ostringstream oss;
    oss << "Built subscriptions: " << item_subscriptions_.size() << " items, "
        << location_subscriptions_.size() << " locations";
    logger_->log(LogLevel::LOG_INFO, "APMessageRouter", oss.str());
}

void APMessageRouter::route_ap_message(const APMessage& message) {
    if (!ipc_server_) {
        logger_->log(LogLevel::LOG_ERROR, "APMessageRouter", "Cannot route message: IPC server not set");
        return;
    }

    std::ostringstream oss;
    oss << "Routing AP message: " << message.type;
    logger_->log(LogLevel::LOG_DEBUG, "APMessageRouter", oss.str());

    // Route based on message type
    if (message.type == "ReceivedItems") {
        route_received_items(message.payload);
    } else if (message.type == "LocationInfo") {
        route_location_info(message.payload);
    } else if (message.type == "PrintJSON") {
        route_print_json(message.payload);
    } else if (message.type == "RoomUpdate") {
        route_room_update(message.payload);
    } else if (message.type == "Connected") {
        route_connected(message.payload);
    } else if (message.type == "ConnectionRefused") {
        route_connection_refused(message.payload);
    } else {
        logger_->log(LogLevel::LOG_WARN, "APMessageRouter", "Unknown AP message type: " + message.type);
    }
}

void APMessageRouter::route_received_items(const nlohmann::json& payload) {
    // ReceivedItems format: {"index": 0, "items": [{item: 123, location: 456, player: 1, flags: 0}, ...]}
    if (!payload.contains("items") || !payload["items"].is_array()) {
        logger_->log(LogLevel::LOG_ERROR, "APMessageRouter", "ReceivedItems missing items array");
        return;
    }

    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    // Route each item to its owning mod
    for (const auto& item_data : payload["items"]) {
        if (!item_data.contains("item")) {
            continue;
        }

        int64_t item_id = item_data["item"].get<int64_t>();

        // Find mods that own this item
        auto it = item_subscriptions_.find(item_id);
        if (it != item_subscriptions_.end()) {
            for (const std::string& mod_id : it->second) {
                // Create IPC message for this mod
                IPCMessage ipc_msg;
                ipc_msg.type = "ap_message";
                ipc_msg.from_mod_id = "framework";
                ipc_msg.to_mod_id = mod_id;
                ipc_msg.data = {
                    {"command", "ITEM_RECEIVED"},
                    {"item_id", item_id},
                    {"location_id", item_data.value("location", -1)},
                    {"player", item_data.value("player", -1)},
                    {"flags", item_data.value("flags", 0)}
                };

                send_to_mod(mod_id, ipc_msg);
            }
        } else {
            std::ostringstream oss;
            oss << "No mod owns item ID " << item_id;
            logger_->log(LogLevel::LOG_WARN, "APMessageRouter", oss.str());
        }
    }

    // Also include the index in case mods need it for state tracking
    if (payload.contains("index")) {
        // Broadcast index update to all mods so they can track their progress
        IPCMessage index_msg;
        index_msg.type = "ap_message";
        index_msg.from_mod_id = "framework";
        index_msg.data = {
            {"command", "ITEMS_INDEX_UPDATE"},
            {"index", payload["index"]}
        };
        broadcast_to_all(index_msg);
    }
}

void APMessageRouter::route_location_info(const nlohmann::json& payload) {
    // LocationInfo format: {"locations": [{item: 123, location: 456, player: 1, flags: 0}, ...]}
    if (!payload.contains("locations") || !payload["locations"].is_array()) {
        logger_->log(LogLevel::LOG_ERROR, "APMessageRouter", "LocationInfo missing locations array");
        return;
    }

    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    // Route each location info to its owning mod
    for (const auto& location_data : payload["locations"]) {
        if (!location_data.contains("location")) {
            continue;
        }

        int64_t location_id = location_data["location"].get<int64_t>();

        // Find mods that own this location
        auto it = location_subscriptions_.find(location_id);
        if (it != location_subscriptions_.end()) {
            for (const std::string& mod_id : it->second) {
                // Create IPC message for this mod
                IPCMessage ipc_msg;
                ipc_msg.type = "ap_message";
                ipc_msg.from_mod_id = "framework";
                ipc_msg.to_mod_id = mod_id;
                ipc_msg.data = {
                    {"command", "LOCATION_INFO"},
                    {"location_id", location_id},
                    {"item_id", location_data.value("item", -1)},
                    {"player", location_data.value("player", -1)},
                    {"flags", location_data.value("flags", 0)}
                };

                send_to_mod(mod_id, ipc_msg);
            }
        }
    }
}

void APMessageRouter::route_print_json(const nlohmann::json& payload) {
    // PrintJSON is a broadcast message - send to all connected mods
    IPCMessage ipc_msg;
    ipc_msg.type = "ap_message";
    ipc_msg.from_mod_id = "framework";
    ipc_msg.data = {
        {"command", "PRINT_MESSAGE"},
        {"message", payload}
    };

    broadcast_to_all(ipc_msg);
}

void APMessageRouter::route_room_update(const nlohmann::json& payload) {
    // RoomUpdate is a broadcast message - send to all connected mods
    IPCMessage ipc_msg;
    ipc_msg.type = "ap_message";
    ipc_msg.from_mod_id = "framework";
    ipc_msg.data = {
        {"command", "ROOM_UPDATE"},
        {"update", payload}
    };

    broadcast_to_all(ipc_msg);
}

void APMessageRouter::route_connected(const nlohmann::json& payload) {
    // Connected is a broadcast message - send to all connected mods
    IPCMessage ipc_msg;
    ipc_msg.type = "ap_message";
    ipc_msg.from_mod_id = "framework";
    ipc_msg.data = {
        {"command", "CONNECTED"},
        {"slot_data", payload}
    };

    broadcast_to_all(ipc_msg);
}

void APMessageRouter::route_connection_refused(const nlohmann::json& payload) {
    // ConnectionRefused is a broadcast message - send to all connected mods
    IPCMessage ipc_msg;
    ipc_msg.type = "ap_message";
    ipc_msg.from_mod_id = "framework";
    ipc_msg.data = {
        {"command", "CONNECTION_REFUSED"},
        {"errors", payload}
    };

    broadcast_to_all(ipc_msg);
}

Result<nlohmann::json> APMessageRouter::route_to_ap_server(const IPCMessage& ipc_message) {
    // Convert IPC message to AP protocol message
    // The IPC message should have a "command" field indicating what AP action to take

    if (!ipc_message.data.contains("command")) {
        return Result<nlohmann::json>::failure(ErrorCode::VALIDATION_ERROR,
            "IPC message missing 'command' field");
    }

    std::string command = ipc_message.data["command"];
    nlohmann::json ap_message;

    if (command == "LOCATION_CHECK") {
        // Mod is checking a location
        // Expected format: {"command": "LOCATION_CHECK", "location_ids": [123, 456, ...]}
        if (!ipc_message.data.contains("location_ids")) {
            return Result<nlohmann::json>::failure(ErrorCode::VALIDATION_ERROR,
                "LOCATION_CHECK missing location_ids");
        }
        ap_message = {
            {"cmd", "LocationChecks"},
            {"locations", ipc_message.data["location_ids"]}
        };
    } else if (command == "LOCATION_SCOUT") {
        // Mod is scouting locations
        // Expected format: {"command": "LOCATION_SCOUT", "location_ids": [123, 456, ...], "create_as_hint": 0}
        if (!ipc_message.data.contains("location_ids")) {
            return Result<nlohmann::json>::failure(ErrorCode::VALIDATION_ERROR,
                "LOCATION_SCOUT missing location_ids");
        }
        ap_message = {
            {"cmd", "LocationScouts"},
            {"locations", ipc_message.data["location_ids"]},
            {"create_as_hint", ipc_message.data.value("create_as_hint", 0)}
        };
    } else if (command == "STATUS_UPDATE") {
        // Mod is updating status
        // Expected format: {"command": "STATUS_UPDATE", "status": 30}
        if (!ipc_message.data.contains("status")) {
            return Result<nlohmann::json>::failure(ErrorCode::VALIDATION_ERROR,
                "STATUS_UPDATE missing status");
        }
        ap_message = {
            {"cmd", "StatusUpdate"},
            {"status", ipc_message.data["status"]}
        };
    } else {
        return Result<nlohmann::json>::failure(ErrorCode::VALIDATION_ERROR,
            "Unknown command: " + command);
    }

    std::ostringstream oss;
    oss << "Routing to AP server from " << ipc_message.from_mod_id << ": " << command;
    logger_->log(LogLevel::LOG_DEBUG, "APMessageRouter", oss.str());

    return Result<nlohmann::json>::success(ap_message);
}

void APMessageRouter::send_to_mod(const std::string& mod_id, const IPCMessage& message) {
    if (!ipc_server_) {
        logger_->log(LogLevel::LOG_ERROR, "APMessageRouter", "Cannot send to mod: IPC server not set");
        return;
    }

    auto result = ipc_server_->send_to_mod(mod_id, message);
    if (!result.is_success()) {
        std::ostringstream oss;
        oss << "Failed to send to mod " << mod_id << ": " << result.error_message;
        logger_->log(LogLevel::LOG_WARN, "APMessageRouter", oss.str());
    }
}

void APMessageRouter::broadcast_to_all(const IPCMessage& message) {
    if (!ipc_server_) {
        logger_->log(LogLevel::LOG_ERROR, "APMessageRouter", "Cannot broadcast: IPC server not set");
        return;
    }

    auto result = ipc_server_->broadcast(message);
    if (!result.is_success()) {
        logger_->log(LogLevel::LOG_WARN, "APMessageRouter", "Failed to broadcast: " + result.error_message);
    }
}

std::vector<std::string> APMessageRouter::get_mods_owning_item(int64_t item_id) const {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    auto it = item_subscriptions_.find(item_id);
    if (it != item_subscriptions_.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::string> APMessageRouter::get_mods_owning_location(int64_t location_id) const {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    auto it = location_subscriptions_.find(location_id);
    if (it != location_subscriptions_.end()) {
        return it->second;
    }
    return {};
}

Result<std::string> APMessageRouter::get_mod_owning_item_name(const std::string& item_name) const {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    auto it = item_name_to_mod_.find(item_name);
    if (it != item_name_to_mod_.end()) {
        return Result<std::string>::success(it->second);
    }
    return Result<std::string>::failure(ErrorCode::VALIDATION_ERROR,
        "No mod owns item name: " + item_name);
}

Result<std::string> APMessageRouter::get_mod_owning_location_name(const std::string& location_name) const {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    auto it = location_name_to_mod_.find(location_name);
    if (it != location_name_to_mod_.end()) {
        return Result<std::string>::success(it->second);
    }
    return Result<std::string>::failure(ErrorCode::VALIDATION_ERROR,
        "No mod owns location name: " + location_name);
}

void APMessageRouter::clear() {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    item_subscriptions_.clear();
    location_subscriptions_.clear();
    item_name_to_mod_.clear();
    location_name_to_mod_.clear();
    logger_->log(LogLevel::LOG_INFO, "APMessageRouter", "Cleared all subscriptions");
}

} // namespace APFramework