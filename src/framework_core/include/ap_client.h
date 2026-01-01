#pragma once
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <cstdint>

namespace APFramework {

// Opaque pointer to apclientpp implementation
struct APClientImpl;

/**
 * @brief Message structure for AP events
 *
 * Represents events received from the Archipelago server, such as
 * items received, locations checked, connection status changes, etc.
 */
struct APMessage {
    enum Type {
        ItemReceived,      // Player received an item
        LocationChecked,   // A location was checked
        SlotConnected,     // Successfully connected to slot
        Disconnected,      // Disconnected from server
        RoomInfo,          // Room information received
        DataPackage        // Data package received
    };

    Type type;
    int64_t item_id;
    int64_t location_id;
    int player_slot;
    std::string data_json; // Full message as JSON for flexibility
};

/**
 * @brief Wrapper around apclientpp for Archipelago server communication
 *
 * Provides a simplified interface to the apclientpp library, handling:
 * - Connection management to AP servers
 * - Continuous polling for incoming messages
 * - Sending location checks and status updates
 * - Converting apclientpp events to APMessage format
 *
 * This wrapper isolates the rest of the framework from apclientpp
 * implementation details and provides thread-safe message queuing.
 */
class APClientWrapper {
public:
    APClientWrapper(const std::string& uuid, const std::string& game);
    ~APClientWrapper();

    // Connection management
    bool connect(const std::string& server, int port,
                const std::string& slot_name, const std::string& password);
    void disconnect();
    bool is_connected() const;
    int get_state() const;

    // Polling - call continuously in polling thread
    void poll();

    // Get pending messages (thread-safe)
    std::vector<APMessage> get_messages();

    // Commands to send to server
    void check_location(int64_t location_id);
    void status_update(int status);

private:
    std::unique_ptr<APClientImpl> impl_;
    std::vector<APMessage> pending_messages_;
    mutable std::mutex messages_mutex_;

    // Callbacks from apclientpp (registered during construction)
    void on_slot_connected(const std::string& data);
    void on_slot_refused(const std::string& reason);
    void on_items_received(const std::string& data);
    void on_location_checked(const std::string& data);
};

} // namespace APFramework
