# Phase 06: Message Routing & Polling

**Status**: ✅ Complete

---

## Overview

Implement message routing between AP server and mods based on capability subscriptions.

**Goals**:
- Route AP messages to subscribed mods
- Route IPC messages between mods and framework
- Filter messages by capability subscriptions
- Handle priority client vs regular client routing
- Console log routing to priority clients only

---

## Prerequisites

- ✅ Phase 03 complete (IPC system)
- ✅ Phase 04 complete (APClient and polling)
- ✅ Phase 05 complete (Capabilities and registry)

---

## Components

### 1. APMessageRouter - Bidirectional Routing
### 2. Subscription Management
### 3. Console Log Routing

---

## Key Implementation Points

**APMessageRouter**:
```cpp
class APMessageRouter {
public:
    // Route message from AP server to subscribed mods
    void route_ap_message(const APMessage& message);

    // Route message from mod to AP server
    void route_to_ap_server(const IPCMessage& message);

    // Route IPC message between mods (if needed)
    void route_ipc_message(const IPCMessage& message);

    // Build subscription map from capabilities
    void build_subscriptions(const nlohmann::json& capabilities);

    // Get mods subscribed to specific item/location
    std::vector<std::string> get_subscribed_mods(int64_t item_id) const;

private:
    std::map<int64_t, std::vector<std::string>> item_subscriptions_;
    std::map<int64_t, std::vector<std::string>> location_subscriptions_;
};
```

**Message Filtering**:
- ReceivedItems: Route to mods that handle those item IDs
- LocationInfo: Route to mods that handle those location IDs
- RoomUpdate, PrintJSON: Broadcast to all mods (optional subscription)

**Console Log Routing** (from ARCHITECTURE.md):
- When `logging.console = true` in config
- Logs are sent as IPC messages to **PRIORITY CLIENTS ONLY**
- Standard framework messages (lifecycle, responses) still go to relevant mods
- Only internal framework logging (filtered by log level) routed to priority clients

```cpp
// In APLogger when console mode enabled:
void APLogger::write_to_callback(LogLevel level, const std::string& message) {
    if (log_callback_) {
        log_callback_(level, message);
    }
}

// APManager sets this callback to route to priority clients:
logger.set_log_callback([this](LogLevel level, const std::string& msg) {
    IPCMessage log_msg;
    log_msg.type = "log";
    log_msg.data = {{"level", level_to_string(level)}, {"message", msg}};
    ipc_server_->broadcast_to_priority_clients(log_msg);
});
```

---

## Testing

- Test AP message routing to correct mods
- Test message filtering by subscriptions
- Test console log routing to priority clients only
- Test broadcast vs unicast routing

---

## Acceptance Criteria

- ✅ APMessageRouter routes AP messages to subscribed mods
- ✅ Subscription map built from capabilities
- ✅ Console logs routed to priority clients only (when enabled)
- ✅ Message filtering working correctly
- ✅ Integration with APPollingThread working
- ✅ Unit tests pass

---

## Next Phase

[Phase 07: Lifecycle & State Management](Phase07_LifecycleManagement.md)

---

## Notes

- APMessageRouter fully implemented with bidirectional routing
- Console log routing to priority clients will be integrated in Phase07 (APManager)
- LogLevel enum migrated to use `LOG_` prefix to avoid Windows macro conflicts
- Fixed all Result/VoidResult usage to properly use `.is_success()` and `.error_message`

---

**Last Updated**: 2026-01-10
**Status**: ✅ Complete
