# Phase 04: AP Client Integration

**Status**: 🟢 Complete

---

## Overview

Integrate apclientpp library and implement APClient wrapper for Archipelago server communication.

**Goals**:
- Wrap apclientpp with APClient class
- Implement async connection with timeout
- Message queue system for received AP messages
- APPollingThread with lifecycle state checking
- WebSocket communication working

---

## Prerequisites

- ✅ Phase 02 complete (APConfig, APLogger)
- ✅ apclientpp submodule initialized
- ✅ APPollingThread stub exists

---

## Components

### 1. APClient - apclientpp Wrapper
### 2. APPollingThread - 60fps Polling Loop (Configurable)
### 3. Connection Management
### 4. Message Queue System

---

## Key Implementation Points

**APClient Class**:
```cpp
class APClient {
public:
    // Async connection with callback
    void connect_async(const std::string& server, int port,
                       const std::string& slot_name, const std::string& password,
                       std::function<void(bool success, const std::string& error)> callback);

    // Poll for events (called by APPollingThread)
    void poll();

    // Get queued messages
    std::vector<APMessage> get_messages();

    // Send commands to AP server
    void send_location_checks(const std::vector<int64_t>& locations);
    void send_location_scouts(const std::vector<int64_t>& locations);
    void update_status(ClientStatus status);
    // ... other AP commands
};
```

**APPollingThread Implementation**:
```cpp
void APPollingThread::polling_loop() {
    auto last_poll = std::chrono::steady_clock::now();

    while (!should_stop_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_poll);

        // Only poll when the interval has elapsed
        if (elapsed >= poll_interval_) {
            last_poll = now;

            try {
                // Check lifecycle state - only poll when in appropriate states
                auto current_phase = ap_manager_->get_current_phase();

                // Only poll and route messages when connected to AP server
                if (current_phase == LifecyclePhase::CONNECTED_AND_SYNCING ||
                    current_phase == LifecyclePhase::RUNNING) {

                    ap_client_->poll();
                    auto messages = ap_client_->get_messages();

                    for (const auto& msg : messages) {
                        message_router_->route_ap_message(msg);
                    }
                }
                // In other states, skip polling to avoid processing messages
                // during inconsistent framework state

            } catch (const std::exception& e) {
                AP_LOG_ERROR("Polling error: " + std::string(e.what()));
            }
        }

        // Small sleep to prevent busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
```

**Connection Timeout**: Use APConfig's connection_timeout_ms (default 30s)

**Message Types from AP Server**:
- Connected
- ReceivedItems
- LocationInfo
- RoomUpdate
- PrintJSON
- DataPackage
- etc. (see apclientpp documentation)

---

## Testing

- Unit tests for APClient connection/disconnection
- Mock AP server for testing (or use apclientpp's test utilities)
- Verify polling thread respects lifecycle state
- Verify async connection timeout works

---

## Acceptance Criteria

- ✅ APClient wraps apclientpp successfully
- ✅ Async connection with timeout working
- ✅ APPollingThread polls at configured interval (default 16ms / 60fps)
- ✅ APPollingThread checks lifecycle state before polling
- ✅ Message queue system working
- ✅ Can connect to real AP server and receive messages
- ✅ Unit tests pass

---

## Next Phase

[Phase 05: Capabilities & Registry System](Phase05_CapabilitiesSystem.md)

---

**Last Updated**: 2026-01-10
**Status**: 🟢 Complete
