# Phase 07: Lifecycle & State Management

**Status**: 🔴 Not Started

---

## Overview

Implement APManager singleton and full lifecycle state machine orchestrating all framework components.

**Goals**:
- Complete state machine implementation
- Priority vs regular client registration flow
- Command handling (CONNECT, GENERATE, DISCONNECT, etc.)
- Resync mechanism
- Error state handling

---

## Prerequisites

- ✅ All previous phases complete (Phase 01-06)
- ✅ All components ready for orchestration

---

## Lifecycle States

From ARCHITECTURE.md:

```
UNINITIALIZED
  ↓
DISCOVERING_MODS
  ↓
AWAITING_PRIORITY_REGISTRATION (timeout: 60s)
  ↓
AWAITING_REGULAR_REGISTRATION (timeout: 180s)
  ↓
VALIDATING_CAPABILITIES
  ↓
READY_FOR_CONNECTION
  ↓
CONNECTING (on CMD_CONNECT from priority client)
  ↓
CONNECTED_AND_SYNCING
  ↓
RUNNING
  ↓
ERROR_STATE (on critical errors)
```

---

## Components

### 1. APManager - Main Orchestrator
### 2. State Machine Implementation
### 3. Command Handlers
### 4. Resync Mechanism

---

## Key Implementation Points

**APManager Class**:
```cpp
class APManager {
public:
    static APManager& instance();

    // Initialize all components
    VoidResult init(const std::string& config_path);

    // Start framework lifecycle
    VoidResult start();

    // Shutdown framework
    void shutdown();

    // Get current lifecycle phase
    LifecyclePhase get_current_phase() const;

    // Handle commands from priority clients
    VoidResult handle_command(const std::string& cmd, const nlohmann::json& data);

    // Trigger resync
    VoidResult trigger_resync();

private:
    void transition_to(LifecyclePhase new_phase);
    void enter_error_state(const std::string& error_message);

    // State machine handlers
    void handle_discovering_mods();
    void handle_awaiting_priority_registration();
    void handle_awaiting_regular_registration();
    void handle_validating_capabilities();
    void handle_connecting();
    void handle_connected_and_syncing();

    // Component references
    std::unique_ptr<APConfig> config_;
    std::unique_ptr<APLogger> logger_;
    std::unique_ptr<APClient> ap_client_;
    std::unique_ptr<APIPCServer> ipc_server_;
    std::unique_ptr<APModRegistry> mod_registry_;
    std::unique_ptr<APCapabilitiesGenerator> capabilities_generator_;
    std::unique_ptr<APMessageRouter> message_router_;
    std::unique_ptr<APPollingThread> polling_thread_;

    std::atomic<LifecyclePhase> current_phase_{LifecyclePhase::UNINITIALIZED};
};
```

**Command Handlers**:
- `CMD_CONNECT`: Transition from READY_FOR_CONNECTION → CONNECTING
- `CMD_GENERATE`: Generate capabilities file, save to disk
- `CMD_DISCONNECT`: Gracefully disconnect from AP server
- `CMD_RESYNC`: Trigger resync (re-validate capabilities, reconnect)

**Resync Mechanism**:
1. Disconnect from AP server
2. Re-scan mods directory for changes
3. Re-validate capabilities
4. Detect checksum changes
5. If mismatch → ERROR_STATE
6. If match → reconnect

**Checksum Mismatch Handling** (from ARCHITECTURE_REVIEW.md):
```cpp
if (expected_checksum != current_checksum) {
    // Ecosystem mismatch - CRITICAL ERROR
    enter_error_state("Mod ecosystem changed since generation. "
                      "Expected checksum: " + expected_checksum +
                      ", Current: " + current_checksum);

    broadcast_error_to_all_mods("ECOSYSTEM_MISMATCH",
                                "Your installed mods do not match the generated multiworld. "
                                "Please ensure the same mods are installed and restart.");
}
```

**Timeouts**:
- Priority registration: 60s (configurable)
- Regular registration: 180s (configurable)
- Connection: 30s (configurable)

---

## Testing

- Test full lifecycle from UNINITIALIZED → RUNNING
- Test timeout handling for registration phases
- Test command handling
- Test resync mechanism
- Test error state transitions
- Test checksum mismatch handling

---

## Acceptance Criteria

- ✅ APManager singleton working
- ✅ Full state machine implemented
- ✅ All state transitions working correctly
- ✅ Priority vs regular registration flow working
- ✅ Command handlers implemented
- ✅ Resync mechanism working
- ✅ Checksum mismatch → ERROR_STATE
- ✅ Timeouts enforced
- ✅ Integration tests pass

---

## Next Phase

[Phase 08: Lua Bindings & APFrameworkMod](Phase08_LuaBindings.md)

---

**Last Updated**: 2026-01-09
**Status**: 🔴 Not Started
