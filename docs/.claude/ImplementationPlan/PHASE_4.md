# Phase 4: Example Mods & Testing

## Overview

Phase 4 creates complete example mods demonstrating how to integrate with APFramework, plus comprehensive testing.

## Goals

- Provide working examples for mod developers
- Demonstrate different mod types (Lua, C++, BP Logic)
- Validate the entire system works end-to-end
- Create documentation and tutorials

## Example Mods

### 1. Example Lua Mod - "APTestLua"

**Purpose**: Simple Lua mod demonstrating basic AP integration

**Features**:
- Registers items and locations
- Handles item received events
- Checks locations when player actions occur
- Uses `ap_client.lua` from Phase 2

**Structure**:
```
APTestLua/
├── Scripts/
│   └── main.lua
├── ap_config.json
└── enabled.txt
```

**main.lua**:
```lua
local APClient = require("ap_client")

local client = APClient:new("APTestLua")
local items_given = {}

-- Callbacks
client.on_item_received = function(item_id, location_id, player_slot)
    print("[APTestLua] Received item: " .. item_id)

    -- Example: Give pickaxe to player
    if item_id == 100000 and not items_given[item_id] then
        print("Giving Super Pickaxe to player!")
        -- UE4SS code to spawn item in player inventory
        items_given[item_id] = true
    end
end

client.on_location_checked = function(location_id)
    print("[APTestLua] Location checked: " .. location_id)
end

client.on_connection_status = function(connected, slot_name)
    if connected then
        print("[APTestLua] Connected to AP as: " .. slot_name)
    else
        print("[APTestLua] Disconnected from AP")
    end
end

client.on_registration_complete = function()
    print("[APTestLua] All mods registered! Can now connect to AP.")
end

-- Register capabilities
client:register({
    items = {
        {id = 100000, name = "Super Pickaxe", classification = "useful"}
    },
    locations = {
        {id = 200000, name = "Chest in Cave", region = "Starting Area"}
    },
    regions = {
        {name = "Starting Area", connects_to = {}}
    }
})

-- Example: Check location when player opens specific chest
RegisterHook("/Script/Pal.PalPlayerCharacter:OnOpenChest", function(self, ChestID)
    if ChestID == 42 then  -- Specific chest
        print("[APTestLua] Checking location: Chest in Cave")
        client:check_location(200000)
    end
end)

-- Poll every tick
RegisterHook("/Script/Engine.PlayerController:PlayerTick", function()
    client:poll()
end)
```

**ap_config.json**:
```json
{
  "mod_id": "APTestLua",
  "items": [
    {"id": 100000, "name": "Super Pickaxe", "classification": "useful"}
  ],
  "locations": [
    {"id": 200000, "name": "Chest in Cave", "region": "Starting Area"}
  ],
  "regions": [
    {"name": "Starting Area", "connects_to": []}
  ]
}
```

### 2. Example C++ Mod - "APTestCpp"

**Purpose**: C++ mod demonstrating `APClientLib.dll` usage

**Features**:
- Same functionality as Lua example
- Shows C++ API usage
- Links against APClientLib.dll

**Structure**:
```
APTestCpp/
├── src/
│   └── main.cpp
├── include/
│   └── ap_client_lib.h  (from Phase 2)
├── APClientLib.dll      (from Phase 2)
├── ap_config.json
├── CMakeLists.txt
└── enabled.txt
```

**main.cpp**:
```cpp
#include <ap_client_lib.h>
#include <unordered_set>

static APClientHandle g_client = nullptr;
static std::unordered_set<int64_t> items_given;

// Callbacks
void on_item_received(int64_t item_id, int64_t location_id, int player_slot, void* user_data) {
    printf("[APTestCpp] Received item: %lld\n", item_id);

    if (item_id == 100001 && items_given.find(item_id) == items_given.end()) {
        printf("Giving Mega Sword to player!\n");
        // Game-specific code to give item
        items_given.insert(item_id);
    }
}

void on_location_checked(int64_t location_id, void* user_data) {
    printf("[APTestCpp] Location checked: %lld\n", location_id);
}

void on_connection_status(bool connected, const char* slot_name, void* user_data) {
    if (connected) {
        printf("[APTestCpp] Connected to AP as: %s\n", slot_name);
    } else {
        printf("[APTestCpp] Disconnected from AP\n");
    }
}

void on_registration_complete(void* user_data) {
    printf("[APTestCpp] All mods registered!\n");
}

// UE4SS C++ mod entry point
extern "C" __declspec(dllexport) void InitializeMod() {
    // Create client
    g_client = ap_client_create("APTestCpp");

    // Set callbacks
    ap_client_set_item_received_callback(g_client, on_item_received, nullptr);
    ap_client_set_location_checked_callback(g_client, on_location_checked, nullptr);
    ap_client_set_connection_status_callback(g_client, on_connection_status, nullptr);
    ap_client_set_registration_complete_callback(g_client, on_registration_complete, nullptr);

    // Register capabilities
    const char* capabilities = R"({
        "items": [{"id": 100001, "name": "Mega Sword", "classification": "progression"}],
        "locations": [{"id": 200001, "name": "Boss Chest", "region": "Boss Arena"}],
        "regions": [{"name": "Boss Arena", "connects_to": []}]
    })";
    ap_client_register(g_client, capabilities);
}

extern "C" __declspec(dllexport) void OnGameTick() {
    if (g_client) {
        ap_client_poll(g_client);
    }
}

extern "C" __declspec(dllexport) void UninitializeMod() {
    if (g_client) {
        ap_client_destroy(g_client);
        g_client = nullptr;
    }
}
```

### 3. Example BP Logic Mod - "APTestBP"

**Purpose**: Blueprint Logic mod with Lua companion

**Approach**:
- Blueprint handles game logic
- Companion Lua mod handles AP communication
- Communication via shared data or events

**Structure**:
```
APTestBP/
├── LogicMods/
│   └── MyBPLogic.pak          (Blueprint Logic mod)
├── Scripts/
│   └── bp_companion.lua       (Lua companion for IPC)
├── ap_config.json
└── enabled.txt
```

**bp_companion.lua**:
```lua
local APClient = require("ap_client")
local client = APClient:new("APTestBP")

-- BP -> Lua communication via global state or hooks
local bp_state = {
    items_to_give = {},
    locations_to_check = {}
}

client.on_item_received = function(item_id, location_id, player_slot)
    -- Add to queue for BP to process
    table.insert(bp_state.items_to_give, {
        item_id = item_id,
        location_id = location_id,
        player_slot = player_slot
    })
end

-- Register
client:register({
    items = {
        {id = 100002, name = "BP Item", classification = "filler"}
    },
    locations = {
        {id = 200002, name = "BP Location", region = "BP Region"}
    },
    regions = {
        {name = "BP Region", connects_to = {}}
    }
})

-- Expose state to BP via global
_G.APTestBP_State = bp_state

-- Poll
RegisterHook("/Script/Engine.PlayerController:PlayerTick", function()
    client:poll()

    -- BP can check _G.APTestBP_State.items_to_give
    -- and process items accordingly
end)
```

## Integration Testing

### Test Scenarios

#### 1. Framework Startup
- [ ] APFramework loads successfully
- [ ] APFrameworkCore.dll loads via FFI
- [ ] IPC server starts
- [ ] No errors in UE4SS log

#### 2. Mod Discovery
- [ ] Framework discovers all installed AP mods
- [ ] Correct mod_ids extracted from ap_config.json
- [ ] Discovery completes before registration timeout

#### 3. Mod Registration
- [ ] All discovered mods register within timeout
- [ ] Registration data correctly parsed
- [ ] Capabilities generator receives all mod data
- [ ] APCapabilities.json generated correctly

#### 4. APCapabilities.json Validation
```json
{
  "game": "Palworld",
  "version": "2.0.0",
  "items": [
    {"id": 100000, "name": "Super Pickaxe", "classification": "useful", "mod_id": "APTestLua"},
    {"id": 100001, "name": "Mega Sword", "classification": "progression", "mod_id": "APTestCpp"}
  ],
  "locations": [
    {"id": 200000, "name": "Chest in Cave", "region": "Starting Area", "mod_id": "APTestLua"},
    {"id": 200001, "name": "Boss Chest", "region": "Boss Arena", "mod_id": "APTestCpp"}
  ],
  "regions": [
    {"name": "Starting Area", "connects_to": [], "locations": [200000], "mod_id": "APTestLua"},
    {"name": "Boss Arena", "connects_to": [], "locations": [200001], "mod_id": "APTestCpp"}
  ]
}
```

#### 5. AP Server Connection
- [ ] Framework connects to AP server
- [ ] Slot authentication succeeds
- [ ] Connection status broadcast to all mods
- [ ] Mods receive connection_status callback

#### 6. Item Routing
- [ ] AP server sends item to player
- [ ] Framework receives item via polling thread
- [ ] Message router routes to correct mod (by item_id)
- [ ] Mod receives item_received callback
- [ ] Item given to player in-game

#### 7. Location Checking
- [ ] Mod checks location (player action)
- [ ] IPC message sent to framework
- [ ] Framework forwards to APClient
- [ ] AP server receives location check
- [ ] Location marked as checked

#### 8. Cross-Mod Communication
- [ ] Item from ModA sent to player
- [ ] Routed correctly even though defined by ModA
- [ ] No interference with ModB

#### 9. Error Handling
- [ ] Graceful handling of mod registration timeout
- [ ] Connection failure messages
- [ ] Invalid JSON in ap_config.json
- [ ] Mod crash doesn't affect framework

#### 10. Shutdown
- [ ] Framework shuts down cleanly on game exit
- [ ] All threads stopped
- [ ] All pipes closed
- [ ] No memory leaks

### Performance Testing

**Metrics to measure**:
- IPC message latency (should be <1ms)
- Polling thread overhead (should be minimal)
- Memory usage per mod
- Framework startup time

**Tools**:
- Windows Performance Monitor
- UE4SS profiler
- Custom timing logs

### Stress Testing

**Scenarios**:
1. **Many mods**: 10+ AP-enabled mods
2. **Many items**: 1000+ items from various mods
3. **Rapid location checks**: Player spam-checking locations
4. **Long sessions**: Framework running for hours

## Documentation

### For Users

1. **Installation Guide**
   - How to install APFramework
   - How to install AP-enabled mods
   - Basic configuration

2. **Configuration Guide**
   - Editing framework_config.json
   - Connection profiles
   - Autoconnect setup

3. **Troubleshooting**
   - Common errors
   - Log file locations
   - How to report issues

### For Mod Developers

1. **Lua Mod Tutorial**
   - Step-by-step guide to create AP-enabled Lua mod
   - ap_client.lua API reference
   - Best practices

2. **C++ Mod Tutorial**
   - Setup and linking APClientLib.dll
   - API reference
   - Build configuration

3. **BP Logic Mod Tutorial**
   - Creating companion Lua mod
   - BP <-> Lua communication patterns

4. **ap_config.json Format**
   - Field descriptions
   - ID range allocation
   - Region connectivity

5. **Integration Patterns**
   - When to check locations
   - How to give items
   - Handling progression items
   - Save game integration

## Deliverables

### Code
1. ✅ **APTestLua** - Complete Lua example mod
2. ✅ **APTestCpp** - Complete C++ example mod
3. ✅ **APTestBP** - Blueprint Logic example with companion
4. ✅ **Test suite** - Integration test scripts

### Documentation
1. ✅ **User Guide** - Installation and configuration
2. ✅ **Developer Guide** - Creating AP-enabled mods
3. ✅ **API Reference** - Complete API documentation
4. ✅ **Troubleshooting Guide** - Common issues and solutions

### Testing
1. ✅ **Integration tests** - All scenarios passing
2. ✅ **Performance benchmarks** - Documented metrics
3. ✅ **Stress test results** - System limits documented

## Success Criteria

Phase 4 is complete when:
- [ ] All three example mods work end-to-end
- [ ] APCapabilities.json generated correctly
- [ ] Items route to correct mods
- [ ] Location checks reach AP server
- [ ] All integration tests pass
- [ ] Documentation complete
- [ ] Performance meets targets (<1ms IPC latency)
- [ ] No memory leaks or crashes in 1-hour test

## Next Steps After Phase 4

With examples and testing complete, the framework is ready for:
1. **Alpha release** - Limited testing with community
2. **Beta release** - Public testing
3. **Documentation polish** - Based on feedback
4. **Additional mods** - Community creates AP-enabled mods
5. **v2.0.0 Release** - Production-ready release

## Notes

- Example mods should be as simple as possible
- Focus on clarity over clever code
- Heavily comment the examples
- Provide both minimal and feature-rich examples
- Test on clean Palworld installation
- Document all assumptions and requirements