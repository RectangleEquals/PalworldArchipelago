# Phase 09: APClientLib Implementation

**Status**: ✅ Complete

---

## Overview

Implement the lightweight client library (APClientLib) for AP-enabled mods to communicate with the framework.

**Goals**:
- Complete APClientLib C++ implementation
- sol2 bindings for Lua mods
- APClient.lua high-level wrapper
- Example mod demonstrating usage
- Client library tests
- API documentation

---

## Prerequisites

- ✅ Phase 03 complete (IPC system)
- ✅ Phase 08 complete (Framework Lua bindings as reference)

---

## Components

### 1. APClientLib C++ Implementation
### 2. sol2 Bindings for APClientLib
### 3. APClient.lua Wrapper
### 4. Example Mod

---

## Key Implementation Points

**APClientLib C++ API** (`ap_client_lib.h`):
```cpp
namespace APClientLib {

class APClient {
public:
    APClient();
    ~APClient();

    // Initialize and connect to framework
    VoidResult init(const std::string& mod_id);

    // Send IPC messages
    VoidResult send_command(const std::string& cmd, const nlohmann::json& data);
    VoidResult send_to_framework(const nlohmann::json& data);

    // Receive messages from framework
    std::vector<IPCMessage> get_messages();

    // Set callback for received messages
    using MessageCallback = std::function<void(const IPCMessage&)>;
    void set_message_callback(MessageCallback callback);

    // Connection status
    bool is_connected() const;

private:
    std::unique_ptr<APIPCClient> ipc_client_;
    std::string mod_id_;
};

} // namespace APClientLib
```

**sol2 Bindings** (`lua_bindings.cpp`):
```cpp
void register_apclient_bindings(sol::state& lua) {
    lua.new_usertype<APClient>("APClient",
        sol::constructors<APClient()>(),
        "init", &APClient::init,
        "send_command", &APClient::send_command,
        "get_messages", &APClient::get_messages,
        "set_message_callback", &APClient::set_message_callback,
        "is_connected", &APClient::is_connected
    );
}
```

**APClient.lua Wrapper**:
```lua
local APClient = {}
APClient.__index = APClient

function APClient:new(mod_id)
    local obj = setmetatable({}, APClient)
    obj.client = APClientNative.new()  -- C++ APClient
    obj.mod_id = mod_id
    obj.callbacks = {}
    return obj
end

function APClient:init()
    return self.client:init(self.mod_id)
end

function APClient:send_location_check(location_id)
    return self.client:send_command("location_check", {location_id = location_id})
end

function APClient:on_received_item(callback)
    table.insert(self.callbacks, {type = "received_item", callback = callback})
end

function APClient:poll()
    local messages = self.client:get_messages()
    for _, msg in ipairs(messages) do
        self:handle_message(msg)
    end
end

function APClient:handle_message(msg)
    for _, cb in ipairs(self.callbacks) do
        if cb.type == msg.type then
            cb.callback(msg.data)
        end
    end
end

return APClient
```

**Example Mod** (`ExampleClientMod/Scripts/main.lua`):
```lua
local APClient = require("APClient")

local client = nil

local function init_ap_client()
    client = APClient:new("example.palworld.testmod")

    local success, err = pcall(function()
        client:init()
    end)

    if not success then
        print("[ExampleMod] Failed to connect to APFramework: " .. tostring(err))
        return
    end

    print("[ExampleMod] Connected to APFramework!")

    -- Register callbacks
    client:on_received_item(function(item_data)
        print("[ExampleMod] Received item: " .. item_data.item_name)
        -- Add item to player inventory via UE4SS
    end)
end

local function poll_ap_client()
    if client and client:is_connected() then
        client:poll()
    end
end

-- Initialize on game start using Tick event (most reliable method)
-- Note: RegisterInitGameStateHook does NOT exist - use RegisterCustomEvent("Tick") instead
local is_initialized = false

RegisterCustomEvent("Tick", function()
    if not is_initialized then
        init_ap_client()
        is_initialized = true
    end

    -- Poll for messages every tick (after initialization)
    poll_ap_client()
end)
```

**AP_Config.json** for Example Mod:
```json
{
  "mod_id": "example.palworld.testmod",
  "mod_name": "Example AP Test Mod",
  "version": "1.0.0",
  "capabilities": {
    "items": [
      {"id": 5000, "name": "TestItem_ExampleReward", "classification": "filler"}
    ],
    "locations": [
      {"id": 6000, "name": "TestLocation_ExampleCheck", "type": "static", "region": "TestRegion"}
    ],
    "regions": [
      {"name": "TestRegion", "entrances": []}
    ]
  }
}
```

---

## Testing

- Test APClientLib connects to framework IPC
- Test sending and receiving messages
- Test Lua bindings
- Test example mod in UE4SS
- Test multiple client mods simultaneously

---

## Acceptance Criteria

- ✅ APClientLib C++ implementation complete
- ✅ sol2 bindings working
- ✅ APClient.lua wrapper provides easy Lua API
- ✅ Example mod demonstrates full usage
- ✅ Multiple mods can connect simultaneously
- ✅ API documentation written
- ✅ Unit tests pass

---

## Deliverables

1. ✅ Complete APClientLib implementation
2. ✅ Lua bindings and wrapper
3. ✅ Example mod with AP_Config.json
4. ✅ API documentation and tutorial
5. ✅ Unit tests

---

## Next Phase

[Phase 10: Testing & Validation](Phase10_Testing.md)

---

## Notes

**Implementation Summary**:
- Complete APClientLib C++ implementation with smart pointer usage
- sol2 bindings for full Lua integration
- APClient.lua high-level wrapper with callback system
- ExampleClientMod demonstrating complete usage pattern
- AP_Config.json with example items, locations, and regions
- All code uses RAII and follows UE4SS lifecycle requirements

**Files Created/Modified**:
- `APClientLib/include/ap_client_lib.h` - APClient class definition (116 lines)
- `APClientLib/src/ap_client_lib.cpp` - Complete implementation (119 lines)
- `APClientLib/src/lua_bindings.cpp` - sol2 bindings (119 lines)
- `Mods/ExampleClientMod/Scripts/APClient.lua` - Lua wrapper (198 lines)
- `Mods/ExampleClientMod/Scripts/main.lua` - Example mod implementation (149 lines)
- `Mods/ExampleClientMod/AP_Config.json` - Example mod configuration

**Key Features**:
- Callback-based message handling (received items, location info, lifecycle changes)
- Simple location check API (`send_location_check`, `send_location_checks`)
- Custom message type handlers
- Automatic reconnection handling
- Error propagation with Lua-style nil+error returns
- Memory-safe with std::unique_ptr for APIPCClient ownership

---

**Last Updated**: 2026-01-10
**Status**: ✅ Complete
