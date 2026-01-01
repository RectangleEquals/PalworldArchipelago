#include "ap_client.h"
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

    APClientImpl(const std::string& uuid_, const std::string& game_)
        : uuid(uuid_), game(game_), current_uri("localhost:38281") {
        client = std::make_unique<::APClient>(uuid, game, current_uri);
    }

    void reconnect(const std::string& server, int port) {
        std::string new_uri = server + ":" + std::to_string(port);
        if (new_uri != current_uri) {
            // Need to recreate the client with new URI
            current_uri = new_uri;
            client = std::make_unique<::APClient>(uuid, game, current_uri);
        }
    }
};

APClientWrapper::APClientWrapper(const std::string& uuid, const std::string& game) {
    impl_ = std::make_unique<APClientImpl>(uuid, game);

    // Register callbacks
    impl_->client->set_slot_connected_handler([this](const json& data) {
        on_slot_connected(data.dump());
    });

    impl_->client->set_slot_refused_handler([this](const std::list<std::string>& reasons) {
        json reason_json = reasons;
        on_slot_refused(reason_json.dump());
    });

    impl_->client->set_items_received_handler([this](const std::list<APClient::NetworkItem>& items) {
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
        json locations_json = locations;
        on_location_checked(locations_json.dump());
    });
}

APClientWrapper::~APClientWrapper() {
    disconnect();
}

bool APClientWrapper::connect(const std::string& server, int port,
                              const std::string& slot_name, const std::string& password) {
    try {
        // Reconnect with new server if needed
        impl_->reconnect(server, port);

        // Connect to the slot
        // items_handling: 0 = no item link, 1 = send, 2 = receive, 7 = all
        impl_->client->ConnectSlot(slot_name, password, 7);  // 7 = full item link
        return true;
    } catch (const std::exception&) {
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
}

void APClientWrapper::on_slot_refused(const std::string& reason) {
    std::lock_guard<std::mutex> lock(messages_mutex_);

    APMessage msg;
    msg.type = APMessage::Type::Disconnected;
    msg.item_id = 0;
    msg.location_id = 0;
    msg.player_slot = 0;
    msg.data_json = reason;

    pending_messages_.push_back(msg);
}

void APClientWrapper::on_items_received(const std::string& data) {
    std::lock_guard<std::mutex> lock(messages_mutex_);

    try {
        json items = json::parse(data);

        for (const auto& item : items) {
            APMessage msg;
            msg.type = APMessage::Type::ItemReceived;
            msg.item_id = item.value("item", 0);
            msg.location_id = item.value("location", 0);
            msg.player_slot = item.value("player", 0);
            msg.data_json = item.dump();

            pending_messages_.push_back(msg);
        }
    } catch (const std::exception&) {
        // Invalid JSON - skip
    }
}

void APClientWrapper::on_location_checked(const std::string& data) {
    std::lock_guard<std::mutex> lock(messages_mutex_);

    try {
        json locations = json::parse(data);

        for (const auto& location : locations) {
            APMessage msg;
            msg.type = APMessage::Type::LocationChecked;
            msg.item_id = 0;
            msg.location_id = location.get<int64_t>();
            msg.player_slot = 0;
            msg.data_json = data;

            pending_messages_.push_back(msg);
        }
    } catch (const std::exception&) {
        // Invalid JSON - skip
    }
}

} // namespace APFramework
