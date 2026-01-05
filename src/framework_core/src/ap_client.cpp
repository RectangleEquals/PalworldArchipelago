#include "ap_client.h"
#include "debug_log.h"
#include <apclient.hpp>
#include <nlohmann/json.hpp>

namespace APFramework {

using json = nlohmann::json;

// Define the opaque implementation structure
// APClient is from the global namespace (apclientpp library)
struct APClientImpl {
    std::unique_ptr<::APClient> client;
    std::string uuid;
    std::string game;
    std::string current_uri;

    // Pending slot connection parameters (to be used when room_info callback fires)
    std::string pending_slot_name;
    std::string pending_password;
    int pending_items_handling = 0;
    bool has_pending_slot_connection = false;

    APClientImpl(const std::string& uuid_, const std::string& game_)
        : uuid(uuid_), game(game_), current_uri("ws://localhost:38281") {
        client = std::make_unique<::APClient>(uuid, game, current_uri);
    }

    void reconnect(const std::string& server, int port) {
        // Build WebSocket URI with ws:// protocol (non-SSL)
        std::string new_uri = "ws://" + server + ":" + std::to_string(port);
        if (new_uri != current_uri) {
            // Need to recreate the client with new URI
            current_uri = new_uri;
            client = std::make_unique<::APClient>(uuid, game, current_uri);
        }
    }
};

APClientWrapper::APClientWrapper(const std::string& uuid, const std::string& game) {
    impl_ = std::make_unique<APClientImpl>(uuid, game);

    // Register socket-level callbacks
    impl_->client->set_socket_connected_handler([this]() {
        DEBUG_LOG("APClientWrapper callback: socket_connected");
    });

    impl_->client->set_socket_error_handler([this](const std::string& error) {
        DEBUG_LOG("APClientWrapper callback: socket_error - " + error);
    });

    impl_->client->set_socket_disconnected_handler([this]() {
        DEBUG_LOG("APClientWrapper callback: socket_disconnected");
    });

    impl_->client->set_room_info_handler([this]() {
        DEBUG_LOG("APClientWrapper callback: room_info");

        // If we have a pending slot connection, send it now
        if (impl_->has_pending_slot_connection) {
            DEBUG_LOG("APClientWrapper room_info: Sending pending ConnectSlot request");
            DEBUG_LOG("APClientWrapper room_info: slot_name='" + impl_->pending_slot_name +
                     "' password='" + impl_->pending_password +
                     "' items_handling=" + std::to_string(impl_->pending_items_handling));

            impl_->client->ConnectSlot(impl_->pending_slot_name, impl_->pending_password,
                                      impl_->pending_items_handling);
            impl_->has_pending_slot_connection = false;
            DEBUG_LOG("APClientWrapper room_info: ConnectSlot() called");
        }
    });

    // Register slot-level callbacks
    impl_->client->set_slot_connected_handler([this](const json& data) {
        DEBUG_LOG("APClientWrapper callback: slot_connected");
        on_slot_connected(data.dump());
    });

    impl_->client->set_slot_refused_handler([this](const std::list<std::string>& reasons) {
        DEBUG_LOG("APClientWrapper callback: slot_refused");
        json reason_json = reasons;
        on_slot_refused(reason_json.dump());
    });

    impl_->client->set_slot_disconnected_handler([this]() {
        DEBUG_LOG("APClientWrapper callback: slot_disconnected");
    });

    // Register game event callbacks
    impl_->client->set_items_received_handler([this](const std::list<APClient::NetworkItem>& items) {
        DEBUG_LOG("APClientWrapper callback: items_received count=" + std::to_string(items.size()));
        json items_json;
        for (const auto& item : items) {
            json item_obj;
            item_obj["item"] = item.item;
            item_obj["location"] = item.location;
            item_obj["player"] = item.player;
            item_obj["flags"] = item.flags;
            items_json.push_back(item_obj);
        }
        on_items_received(items_json.dump());
    });

    impl_->client->set_location_checked_handler([this](const std::list<int64_t>& locations) {
        DEBUG_LOG("APClientWrapper callback: location_checked count=" + std::to_string(locations.size()));
        json locations_json = locations;
        on_location_checked(locations_json.dump());
    });
}

APClientWrapper::~APClientWrapper() {
    disconnect();
}

bool APClientWrapper::connect(const std::string& server, int port,
                              const std::string& slot_name, const std::string& password) {
    DEBUG_LOG("APClientWrapper::connect() ENTER: server=" + server + " port=" + std::to_string(port) + " slot=" + slot_name);
    try {
        // Reconnect with new server if needed
        DEBUG_LOG("APClientWrapper::connect() calling impl_->reconnect()");
        impl_->reconnect(server, port);
        DEBUG_LOG("APClientWrapper::connect() reconnect complete");

        // Store connection parameters - ConnectSlot will be called from room_info callback
        impl_->pending_slot_name = slot_name;
        impl_->pending_password = password;
        impl_->pending_items_handling = 7;  // 7 = full item link (send + receive)
        impl_->has_pending_slot_connection = true;

        DEBUG_LOG("APClientWrapper::connect() slot connection parameters stored");
        DEBUG_LOG("APClientWrapper::connect() will send ConnectSlot when room_info is received");
        DEBUG_LOG("APClientWrapper::connect() parameters: slot_name='" + slot_name + "' password='" + password + "' items_handling=7");

        return true;
    } catch (const std::exception& e) {
        DEBUG_LOG("APClientWrapper::connect() EXCEPTION: " + std::string(e.what()));
        return false;
    }
}

void APClientWrapper::disconnect() {
    if (impl_) {
        // apclientpp doesn't have an explicit disconnect method
        // Connection is managed by destroying/recreating the client
    }
}

bool APClientWrapper::is_connected() const {
    if (!impl_) {
        return false;
    }

    APClient::State state = impl_->client->get_state();
    return state == APClient::State::SLOT_CONNECTED;
}

int APClientWrapper::get_state() const {
    if (!impl_) {
        return static_cast<int>(APClient::State::DISCONNECTED);
    }

    return static_cast<int>(impl_->client->get_state());
}

void APClientWrapper::poll() {
    if (impl_) {
        impl_->client->poll();

        // Log state periodically for debugging
        static int poll_count = 0;
        poll_count++;
        if (poll_count % 300 == 0) {  // Every ~5 seconds at 60fps
            APClient::State state = impl_->client->get_state();
            DEBUG_LOG("APClientWrapper::poll() state=" + std::to_string(static_cast<int>(state)));
        }
    }
}

std::vector<APMessage> APClientWrapper::get_messages() {
    std::lock_guard<std::mutex> lock(messages_mutex_);

    std::vector<APMessage> messages = std::move(pending_messages_);
    pending_messages_.clear();

    return messages;
}

void APClientWrapper::check_location(int64_t location_id) {
    if (impl_) {
        std::list<int64_t> locations = {location_id};
        impl_->client->LocationChecks(locations);
    }
}

void APClientWrapper::status_update(int status) {
    if (impl_) {
        APClient::ClientStatus client_status = static_cast<APClient::ClientStatus>(status);
        impl_->client->StatusUpdate(client_status);
    }
}

// Callback implementations
void APClientWrapper::on_slot_connected(const std::string& data) {
    std::lock_guard<std::mutex> lock(messages_mutex_);

    APMessage msg;
    msg.type = APMessage::Type::SlotConnected;
    msg.item_id = 0;
    msg.location_id = 0;
    msg.player_slot = 0;
    msg.data_json = data;

    pending_messages_.push_back(msg);
    DEBUG_LOG("APClientWrapper::on_slot_connected() message queued");
}

void APClientWrapper::on_slot_refused(const std::string& reason) {
    DEBUG_LOG("APClientWrapper::on_slot_refused() reason=" + reason);
    // Could add to message queue if needed
}

void APClientWrapper::on_items_received(const std::string& data) {
    std::lock_guard<std::mutex> lock(messages_mutex_);

    APMessage msg;
    msg.type = APMessage::Type::ItemReceived;
    msg.item_id = 0;
    msg.location_id = 0;
    msg.player_slot = 0;
    msg.data_json = data;

    pending_messages_.push_back(msg);
    DEBUG_LOG("APClientWrapper::on_items_received() message queued");
}

void APClientWrapper::on_location_checked(const std::string& data) {
    std::lock_guard<std::mutex> lock(messages_mutex_);

    APMessage msg;
    msg.type = APMessage::Type::LocationChecked;
    msg.item_id = 0;
    msg.location_id = 0;
    msg.player_slot = 0;
    msg.data_json = data;

    pending_messages_.push_back(msg);
    DEBUG_LOG("APClientWrapper::on_location_checked() message queued");
}

} // namespace APFramework
