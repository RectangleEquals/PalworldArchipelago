#include "ap_client.h"
#include "ap_logger.h"
#include <sstream>

// Windows headers define ERROR as a macro, which conflicts with LogLevel::ERROR
#ifdef ERROR
#undef ERROR
#endif

namespace APFramework {

APClient::APClient(APLogger* logger)
    : logger_(logger)
{
}

APClient::~APClient() {
    disconnect();
}

void APClient::connect_async(
    const std::string& server,
    int port,
    const std::string& slot_name,
    const std::string& password,
    std::chrono::milliseconds timeout,
    std::function<void(bool, const std::string&)> callback
) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (connection_in_progress_) {
        if (callback) {
            callback(false, "Connection already in progress");
        }
        return;
    }

    connection_callback_ = callback;
    connection_timeout_ = timeout;
    connection_in_progress_ = true;
    connection_completed_ = false;
    connection_start_time_ = std::chrono::steady_clock::now();

    // Build URI
    std::string uri = server + ":" + std::to_string(port);

    // Create apclientpp instance
    // UUID: Generate a simple UUID (in production, this should be persistent per installation)
    std::string uuid = "APFramework-" + slot_name;
    std::string game = "Palworld"; // TODO: Get from APConfig

    try {
        ap_client_ = std::make_unique<::APClient>(uuid, game, uri);

        // Set up callbacks
        ap_client_->set_socket_connected_handler([this]() { on_socket_connected(); });
        ap_client_->set_socket_error_handler([this](const std::string& err) { on_socket_error(err); });
        ap_client_->set_socket_disconnected_handler([this]() { on_socket_disconnected(); });
        ap_client_->set_slot_connected_handler([this](const nlohmann::json& data) { on_slot_connected(data); });
        ap_client_->set_slot_refused_handler([this](const std::list<std::string>& errors) { on_slot_refused(errors); });
        ap_client_->set_room_info_handler([this]() { on_room_info(); });
        ap_client_->set_items_received_handler([this](const std::list<::APClient::NetworkItem>& items) { on_items_received(items); });
        ap_client_->set_location_info_handler([this](const std::list<::APClient::NetworkItem>& items) { on_location_info(items); });
        ap_client_->set_print_json_handler([this](const nlohmann::json& data) { on_print_json(data); });
        ap_client_->set_room_update_handler([this]() { on_room_update(); });

        // Note: apclientpp connects automatically on first poll()
        // We'll track connection state through callbacks

        if (logger_) {
            logger_->log(LogLevel::INFO, "APClient", "Initiating connection to " + uri);
        }

    } catch (const std::exception& e) {
        connection_in_progress_ = false;
        if (callback) {
            callback(false, std::string("Failed to create APClient: ") + e.what());
        }
        if (logger_) {
            logger_->log(LogLevel::ERROR, "APClient", std::string("Connection failed: ") + e.what());
        }
    }
}

void APClient::disconnect() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (ap_client_) {
        ap_client_->reset();
        ap_client_.reset();
    }

    connection_in_progress_ = false;
    connection_completed_ = false;
    connection_callback_ = nullptr;

    if (logger_) {
        logger_->log(LogLevel::INFO, "APClient", "Disconnected from AP server");
    }
}

void APClient::poll() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (!ap_client_) {
        return;
    }

    // Check connection timeout
    if (connection_in_progress_ && !connection_completed_) {
        check_connection_timeout();
    }

    // Poll apclientpp (processes WebSocket events)
    ap_client_->poll();
}

std::vector<APMessage> APClient::get_messages() {
    std::lock_guard<std::mutex> lock(message_queue_mutex_);

    std::vector<APMessage> messages = std::move(message_queue_);
    message_queue_.clear();

    return messages;
}

bool APClient::is_connected() const {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (!ap_client_) {
        return false;
    }

    return ap_client_->get_state() == ::APClient::State::SLOT_CONNECTED;
}

::APClient::State APClient::get_state() const {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (!ap_client_) {
        return ::APClient::State::DISCONNECTED;
    }

    return ap_client_->get_state();
}

VoidResult APClient::send_location_checks(const std::vector<int64_t>& locations) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (!ap_client_) {
        return VoidResult::failure(ErrorCode::CONNECTION_ERROR, "Not connected to AP server");
    }

    if (!is_connected()) {
        return VoidResult::failure(ErrorCode::CONNECTION_ERROR, "Not in SLOT_CONNECTED state");
    }

    std::list<int64_t> location_list(locations.begin(), locations.end());

    if (!ap_client_->LocationChecks(location_list)) {
        return VoidResult::failure(ErrorCode::INTERNAL_ERROR, "Failed to send LocationChecks");
    }

    if (logger_) {
        logger_->log(LogLevel::DEBUG, "APClient", "Sent " + std::to_string(locations.size()) + " location checks");
    }

    return VoidResult::success();
}

VoidResult APClient::send_location_scouts(const std::vector<int64_t>& locations, int create_as_hint) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (!ap_client_) {
        return VoidResult::failure(ErrorCode::CONNECTION_ERROR, "Not connected to AP server");
    }

    if (!is_connected()) {
        return VoidResult::failure(ErrorCode::CONNECTION_ERROR, "Not in SLOT_CONNECTED state");
    }

    std::list<int64_t> location_list(locations.begin(), locations.end());

    if (!ap_client_->LocationScouts(location_list, create_as_hint)) {
        return VoidResult::failure(ErrorCode::INTERNAL_ERROR, "Failed to send LocationScouts");
    }

    if (logger_) {
        logger_->log(LogLevel::DEBUG, "APClient", "Sent " + std::to_string(locations.size()) + " location scouts");
    }

    return VoidResult::success();
}

VoidResult APClient::send_status_update(::APClient::ClientStatus status) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (!ap_client_) {
        return VoidResult::failure(ErrorCode::CONNECTION_ERROR, "Not connected to AP server");
    }

    if (!is_connected()) {
        return VoidResult::failure(ErrorCode::CONNECTION_ERROR, "Not in SLOT_CONNECTED state");
    }

    if (!ap_client_->StatusUpdate(status)) {
        return VoidResult::failure(ErrorCode::INTERNAL_ERROR, "Failed to send StatusUpdate");
    }

    if (logger_) {
        logger_->log(LogLevel::DEBUG, "APClient", "Sent status update");
    }

    return VoidResult::success();
}

int APClient::get_player_number() const {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (!ap_client_) {
        return -1;
    }

    return ap_client_->get_player_number();
}

int APClient::get_team_number() const {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (!ap_client_) {
        return -1;
    }

    return ap_client_->get_team_number();
}

const std::string& APClient::get_seed() const {
    static const std::string empty;
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (!ap_client_) {
        return empty;
    }

    return ap_client_->get_seed();
}

const std::string& APClient::get_slot() const {
    static const std::string empty;
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (!ap_client_) {
        return empty;
    }

    return ap_client_->get_slot();
}

std::string APClient::get_item_name(int64_t item_id, const std::string& game) const {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (!ap_client_) {
        return "Unknown";
    }

    return ap_client_->get_item_name(item_id, game);
}

std::string APClient::get_location_name(int64_t location_id, const std::string& game) const {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (!ap_client_) {
        return "Unknown";
    }

    return ap_client_->get_location_name(location_id, game);
}

// Internal callback handlers

void APClient::on_socket_connected() {
    if (logger_) {
        logger_->log(LogLevel::INFO, "APClient", "WebSocket connected");
    }

    // Connection not complete until slot is connected
    // apclientpp will automatically send Connect packet after RoomInfo
}

void APClient::on_socket_error(const std::string& error) {
    if (logger_) {
        logger_->log(LogLevel::ERROR, "APClient", "WebSocket error: " + error);
    }

    if (connection_in_progress_ && !connection_completed_) {
        connection_in_progress_ = false;
        if (connection_callback_) {
            connection_callback_(false, "WebSocket error: " + error);
            connection_callback_ = nullptr;
        }
    }
}

void APClient::on_socket_disconnected() {
    if (logger_) {
        logger_->log(LogLevel::WARN, "APClient", "WebSocket disconnected");
    }

    // Queue disconnection message
    {
        std::lock_guard<std::mutex> lock(message_queue_mutex_);
        APMessage msg;
        msg.type = "Disconnected";
        msg.payload = nlohmann::json::object();
        message_queue_.push_back(msg);
    }

    if (connection_in_progress_ && !connection_completed_) {
        connection_in_progress_ = false;
        if (connection_callback_) {
            connection_callback_(false, "Disconnected before slot connection");
            connection_callback_ = nullptr;
        }
    }
}

void APClient::on_slot_connected(const nlohmann::json& slot_data) {
    if (logger_) {
        logger_->log(LogLevel::INFO, "APClient", "Slot connected successfully");
    }

    // Connection complete!
    connection_completed_ = true;
    connection_in_progress_ = false;

    if (connection_callback_) {
        connection_callback_(true, "");
        connection_callback_ = nullptr;
    }

    // Queue Connected message
    {
        std::lock_guard<std::mutex> lock(message_queue_mutex_);
        APMessage msg;
        msg.type = "Connected";
        msg.payload = slot_data;
        message_queue_.push_back(msg);
    }
}

void APClient::on_slot_refused(const std::list<std::string>& errors) {
    std::stringstream ss;
    ss << "Slot connection refused: ";
    for (const auto& err : errors) {
        ss << err << "; ";
    }
    std::string error_msg = ss.str();

    if (logger_) {
        logger_->log(LogLevel::ERROR, "APClient", error_msg);
    }

    connection_in_progress_ = false;

    if (connection_callback_) {
        connection_callback_(false, error_msg);
        connection_callback_ = nullptr;
    }

    // Queue ConnectionRefused message
    {
        std::lock_guard<std::mutex> lock(message_queue_mutex_);
        APMessage msg;
        msg.type = "ConnectionRefused";
        nlohmann::json payload;
        payload["errors"] = std::vector<std::string>(errors.begin(), errors.end());
        msg.payload = payload;
        message_queue_.push_back(msg);
    }
}

void APClient::on_room_info() {
    if (logger_) {
        logger_->log(LogLevel::DEBUG, "APClient", "Received RoomInfo");
    }

    // Queue RoomInfo message
    {
        std::lock_guard<std::mutex> lock(message_queue_mutex_);
        APMessage msg;
        msg.type = "RoomInfo";
        msg.payload = nlohmann::json::object();
        message_queue_.push_back(msg);
    }

    // Note: apclientpp will now automatically attempt to connect to the slot
    // We need to call ConnectSlot here if not done automatically
    // For now, we'll rely on the user calling connect with slot credentials
}

void APClient::on_items_received(const std::list<::APClient::NetworkItem>& items) {
    if (logger_) {
        logger_->log(LogLevel::DEBUG, "APClient", "Received " + std::to_string(items.size()) + " items");
    }

    // Queue ReceivedItems message
    {
        std::lock_guard<std::mutex> lock(message_queue_mutex_);
        APMessage msg;
        msg.type = "ReceivedItems";

        nlohmann::json items_array = nlohmann::json::array();
        for (const auto& item : items) {
            nlohmann::json item_json;
            item_json["item"] = item.item;
            item_json["location"] = item.location;
            item_json["player"] = item.player;
            item_json["flags"] = item.flags;
            item_json["index"] = item.index;
            items_array.push_back(item_json);
        }

        msg.payload["items"] = items_array;
        message_queue_.push_back(msg);
    }
}

void APClient::on_location_info(const std::list<::APClient::NetworkItem>& items) {
    if (logger_) {
        logger_->log(LogLevel::DEBUG, "APClient", "Received LocationInfo for " + std::to_string(items.size()) + " locations");
    }

    // Queue LocationInfo message
    {
        std::lock_guard<std::mutex> lock(message_queue_mutex_);
        APMessage msg;
        msg.type = "LocationInfo";

        nlohmann::json items_array = nlohmann::json::array();
        for (const auto& item : items) {
            nlohmann::json item_json;
            item_json["item"] = item.item;
            item_json["location"] = item.location;
            item_json["player"] = item.player;
            item_json["flags"] = item.flags;
            items_array.push_back(item_json);
        }

        msg.payload["locations"] = items_array;
        message_queue_.push_back(msg);
    }
}

void APClient::on_print_json(const nlohmann::json& data) {
    // Queue PrintJSON message
    {
        std::lock_guard<std::mutex> lock(message_queue_mutex_);
        APMessage msg;
        msg.type = "PrintJSON";
        msg.payload = data;
        message_queue_.push_back(msg);
    }
}

void APClient::on_room_update() {
    if (logger_) {
        logger_->log(LogLevel::DEBUG, "APClient", "Received RoomUpdate");
    }

    // Queue RoomUpdate message
    {
        std::lock_guard<std::mutex> lock(message_queue_mutex_);
        APMessage msg;
        msg.type = "RoomUpdate";
        msg.payload = nlohmann::json::object();
        message_queue_.push_back(msg);
    }
}

void APClient::check_connection_timeout() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - connection_start_time_);

    if (elapsed >= connection_timeout_) {
        connection_in_progress_ = false;

        if (logger_) {
            logger_->log(LogLevel::ERROR, "APClient", "Connection timeout after " + std::to_string(connection_timeout_.count()) + "ms");
        }

        if (connection_callback_) {
            connection_callback_(false, "Connection timeout");
            connection_callback_ = nullptr;
        }

        // Disconnect
        if (ap_client_) {
            ap_client_->reset();
        }
    }
}

} // namespace APFramework