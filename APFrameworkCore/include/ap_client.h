#pragma once
#include "ap_types.h"
#include <apclientpp/apclient.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <functional>
#include <vector>
#include <mutex>
#include <chrono>
#include <string>

namespace APFramework {

// Forward declarations
class APLogger;
class APConfig;

/**
 * APMessage - Represents a message received from the AP server
 * This will be routed to mods via APMessageRouter
 */
struct APMessage {
    std::string type;  // "ReceivedItems", "LocationInfo", "PrintJSON", "RoomUpdate", "Connected", etc.
    nlohmann::json payload;  // The actual AP packet data
};

/**
 * APClient - Wrapper around apclientpp for Archipelago server communication
 *
 * Responsibilities:
 * - Async connection with timeout
 * - Poll WebSocket for events
 * - Queue incoming AP messages for routing
 * - Send commands to AP server (LocationChecks, LocationScouts, StatusUpdate)
 * - Thread-safe message queue
 */
class APClient {
public:
    APClient(APLogger* logger);
    ~APClient();

    // Async connection with callback
    // Callback signature: void(bool success, const std::string& error_message)
    void connect_async(
        const std::string& server,
        int port,
        const std::string& slot_name,
        const std::string& password,
        std::chrono::milliseconds timeout,
        std::function<void(bool, const std::string&)> callback
    );

    // Disconnect from server
    void disconnect();

    // Poll for events (called by APPollingThread at 60fps)
    // This processes WebSocket events and queues messages
    void poll();

    // Get queued messages (thread-safe)
    // Called by APPollingThread to retrieve messages for routing
    std::vector<APMessage> get_messages();

    // Connection status
    bool is_connected() const;
    ::APClient::State get_state() const;

    // Send commands to AP server
    VoidResult send_location_checks(const std::vector<int64_t>& locations);
    VoidResult send_location_scouts(const std::vector<int64_t>& locations, int create_as_hint = 0);
    VoidResult send_status_update(::APClient::ClientStatus status);

    // Get player info
    int get_player_number() const;
    int get_team_number() const;
    const std::string& get_seed() const;
    const std::string& get_slot() const;

    // Data package queries
    std::string get_item_name(int64_t item_id, const std::string& game) const;
    std::string get_location_name(int64_t location_id, const std::string& game) const;

private:
    // Internal callback handlers for apclientpp
    void on_socket_connected();
    void on_socket_error(const std::string& error);
    void on_socket_disconnected();
    void on_slot_connected(const nlohmann::json& slot_data);
    void on_slot_refused(const std::list<std::string>& errors);
    void on_room_info();
    void on_items_received(const std::list<::APClient::NetworkItem>& items);
    void on_location_info(const std::list<::APClient::NetworkItem>& items);
    void on_print_json(const nlohmann::json& data);
    void on_room_update();

    // Connection state tracking
    void check_connection_timeout();

    APLogger* logger_;  // Non-owning
    std::unique_ptr<::APClient> ap_client_;  // apclientpp instance

    // Connection state
    std::function<void(bool, const std::string&)> connection_callback_;
    std::chrono::steady_clock::time_point connection_start_time_;
    std::chrono::milliseconds connection_timeout_{30000};
    bool connection_in_progress_{false};
    bool connection_completed_{false};

    // Message queue
    std::vector<APMessage> message_queue_;
    mutable std::mutex message_queue_mutex_;

    // Thread safety
    mutable std::mutex state_mutex_;
};

} // namespace APFramework