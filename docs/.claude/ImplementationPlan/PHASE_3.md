# Phase 3: Lua Framework Wrapper

## Overview

Phase 3 implements the main **APFramework UE4SS mod** - a Lua mod that wraps APFrameworkCore.dll and manages the framework lifecycle.

This is the **only required mod** for the system to work. It:
- Loads APFrameworkCore.dll via LuaJIT FFI
- Starts the IPC server
- Performs mod auto-discovery
- Connects to the Archipelago server
- Manages the framework lifecycle

## Goals

- Single UE4SS Lua mod that users install
- Loads and manages APFrameworkCore.dll
- Provides configuration UI (via ImGui or console commands)
- Handles framework lifecycle automatically
- Minimal user interaction required

## Architecture

```
APFramework/ (UE4SS Mod)
├── Scripts/
│   ├── main.lua              (Entry point, registers hooks)
│   ├── framework_wrapper.lua (FFI wrapper for APFrameworkCore.dll)
│   ├── config_ui.lua         (Configuration UI - optional)
│   └── utils.lua             (Helpers)
├── APFrameworkCore.dll       (Copied from Phase 1 build)
└── enabled.txt               (UE4SS mod enablement)
```

## Key Components

### 1. main.lua - Entry Point

```lua
-- APFramework main entry point
local FrameworkWrapper = require("framework_wrapper")
local ConfigUI = require("config_ui")

local framework = nil
local mods_directory = "ue4ss/Mods"

-- Initialize framework
RegisterHook("/Script/Engine.GameInstance:Shutdown", function()
    if framework then
        framework:shutdown()
    end
end)

-- Start framework on game start
RegisterHook("/Script/Engine.PlayerController:ServerAcknowledgePossession", function()
    if not framework then
        framework = FrameworkWrapper:new("APFramework_default")
        framework:load_config("APFramework/framework_config.json")
        framework:start_ipc()
        framework:discover_mods(mods_directory)

        -- Wait for registration in background
        -- (handled by framework lifecycle)
    end
end)

-- Polling loop
RegisterHook("/Script/Engine.PlayerController:PlayerTick", function()
    if framework then
        -- Framework handles polling internally via background thread
        -- Just need to occasionally check status or handle UI
        ConfigUI:update()
    end
end)
```

### 2. framework_wrapper.lua - FFI Wrapper

```lua
local ffi = require("ffi")

-- Load FFI definitions from ffi_bindings.h
ffi.cdef[[
    // Opaque handle
    typedef void* FrameworkHandle;

    // Lifecycle
    FrameworkHandle framework_core_create(const char* pipe_name);
    void framework_core_destroy(FrameworkHandle handle);

    // IPC
    void framework_core_start_ipc(FrameworkHandle handle);
    void framework_core_stop_ipc(FrameworkHandle handle);

    // Mod discovery
    void framework_core_discover_mods(FrameworkHandle handle, const char* mods_directory);
    bool framework_core_all_mods_registered(FrameworkHandle handle);
    char* framework_core_get_pending_registrations(FrameworkHandle handle);

    // Capabilities
    char* framework_core_generate_capabilities(FrameworkHandle handle);

    // AP Connection
    bool framework_core_connect_ap(FrameworkHandle handle,
                                    const char* server, int port,
                                    const char* slot_name, const char* password);
    void framework_core_disconnect_ap(FrameworkHandle handle);
    bool framework_core_is_connected(FrameworkHandle handle);

    // Polling
    void framework_core_start_polling(FrameworkHandle handle);
    void framework_core_stop_polling(FrameworkHandle handle);

    // Configuration
    bool framework_core_load_config(FrameworkHandle handle, const char* config_path);
    bool framework_core_save_config(FrameworkHandle handle, const char* config_path);
    char* framework_core_get_active_profile_json(FrameworkHandle handle);
    bool framework_core_set_active_profile(FrameworkHandle handle, const char* profile_json);

    // Utility
    void framework_core_free_string(char* str);
]]

-- Load DLL
local dll_path = "APFramework/APFrameworkCore.dll"
local core = ffi.load(dll_path)

-- Wrapper class
local FrameworkWrapper = {}

function FrameworkWrapper:new(pipe_name)
    local obj = {
        handle = core.framework_core_create(pipe_name),
        pipe_name = pipe_name
    }
    setmetatable(obj, self)
    self.__index = self
    return obj
end

function FrameworkWrapper:shutdown()
    if self.handle then
        core.framework_core_stop_polling(self.handle)
        core.framework_core_disconnect_ap(self.handle)
        core.framework_core_stop_ipc(self.handle)
        core.framework_core_destroy(self.handle)
        self.handle = nil
    end
end

function FrameworkWrapper:load_config(config_path)
    return core.framework_core_load_config(self.handle, config_path)
end

function FrameworkWrapper:save_config(config_path)
    return core.framework_core_save_config(self.handle, config_path)
end

function FrameworkWrapper:start_ipc()
    core.framework_core_start_ipc(self.handle)
end

function FrameworkWrapper:discover_mods(mods_directory)
    core.framework_core_discover_mods(self.handle, mods_directory)
end

function FrameworkWrapper:all_mods_registered()
    return core.framework_core_all_mods_registered(self.handle)
end

function FrameworkWrapper:connect_ap(server, port, slot_name, password)
    return core.framework_core_connect_ap(self.handle, server, port, slot_name, password)
end

function FrameworkWrapper:is_connected()
    return core.framework_core_is_connected(self.handle)
end

-- Helper: Get string from C and free it
function FrameworkWrapper:get_and_free_string(c_str)
    if c_str == nil then return nil end
    local str = ffi.string(c_str)
    core.framework_core_free_string(c_str)
    return str
end

return FrameworkWrapper
```

### 3. config_ui.lua - Configuration UI (Optional)

```lua
-- Optional ImGui-based configuration UI
local ConfigUI = {}

local show_config_window = false

function ConfigUI:update()
    if not ImGui then return end

    if ImGui.Begin("APFramework Configuration", show_config_window) then
        -- Profile selection
        ImGui.Text("Connection Profile:")
        -- ... profile dropdown ...

        -- Server settings
        ImGui.InputText("Server", server_buffer)
        ImGui.InputInt("Port", port_buffer)
        ImGui.InputText("Slot Name", slot_buffer)
        ImGui.InputText("Password", password_buffer, ImGui.InputTextFlags_Password)

        -- Autoconnect
        ImGui.Checkbox("Auto-connect on start", autoconnect_flag)

        -- Connect button
        if ImGui.Button("Connect") then
            framework:connect_ap(server, port, slot_name, password)
        end

        -- Status
        local connected = framework:is_connected()
        ImGui.Text("Status: " .. (connected and "Connected" or "Disconnected"))

        ImGui.End()
    end
end

-- Console command to toggle UI
RegisterConsoleCommand("ap_config", function()
    show_config_window = not show_config_window
end)

return ConfigUI
```

## Lifecycle Flow

```
Game Start
    ↓
1. Load APFrameworkCore.dll via FFI
    ↓
2. Create FrameworkCore instance
    ↓
3. Load config (auto-loads default profile)
    ↓
4. Start IPC server
    ↓
5. Discover mods (scan for ap_config.json)
    ↓
6. Wait for mods to register (2-3 min timeout)
    ↓
7. All mods registered → Generate APCapabilities.json
    ↓
8. Broadcast "registration_complete" to all mods
    ↓
9. Check autoconnect in config
    ├─ If enabled: Connect to AP server
    └─ If disabled: Wait for manual connection request
    ↓
10. Start polling thread (runs in background)
    ↓
Runtime - Messages flow automatically
    ↓
Game Shutdown
    ↓
11. Stop polling
12. Disconnect from AP
13. Stop IPC
14. Destroy framework
```

## Configuration File Format

**framework_config.json**:
```json
{
  "active_profile": {
    "name": "default",
    "server": "archipelago.gg",
    "port": 38281,
    "slot_name": "",
    "password": "",
    "autoconnect": false
  },
  "saved_profiles": {
    "localhost": {
      "name": "localhost",
      "server": "localhost",
      "port": 38281,
      "slot_name": "Player1",
      "password": "",
      "autoconnect": true
    }
  },
  "polling_interval_ms": 16,
  "enable_logging": true,
  "log_level": "info"
}
```

## Auto-Discovery

The framework scans for `ap_config.json` files in all UE4SS mod directories:

**Scan locations**:
```
ue4ss/Mods/*/ap_config.json
```

**Example ap_config.json** (in a mod):
```json
{
  "mod_id": "MyPalworldMod",
  "items": [
    {"id": 100000, "name": "Super Pickaxe", "classification": "useful"}
  ],
  "locations": [
    {"id": 200000, "name": "Chest in Cave", "region": "Starting Area"}
  ],
  "regions": [
    {"name": "Starting Area", "connects_to": ["Mountain Path"]}
  ]
}
```

## Special Framework Registration

The framework **registers itself as a special mod** with mod_id `"APFramework"`:

```lua
-- After starting IPC server, register self
local capabilities = {
    mod_id = "APFramework",
    items = {},      -- Framework doesn't define items
    locations = {},  -- Framework doesn't define locations
    regions = {}     -- Framework doesn't define regions
}

-- Send registration via IPC to self (for logging/status)
-- This allows the framework to receive its own IPC messages for debugging
```

## Error Handling

### Registration Timeout

If not all discovered mods register within timeout (2-3 minutes):

```lua
-- Check periodically
local timeout = 180 -- 3 minutes in seconds
local elapsed = 0

while not framework:all_mods_registered() and elapsed < timeout do
    wait(1)
    elapsed = elapsed + 1
end

if not framework:all_mods_registered() then
    local pending = framework:get_pending_registrations()
    print("ERROR: Not all mods registered within timeout!")
    print("Pending: " .. pending)
    -- Could either:
    -- A) Fail and shutdown
    -- B) Continue with partial registration (not recommended)
    framework:shutdown()
end
```

### Connection Failures

```lua
local connected = framework:connect_ap(server, port, slot_name, password)
if not connected then
    print("Failed to connect to Archipelago server")
    print("Server: " .. server .. ":" .. port)
    print("Slot: " .. slot_name)
    -- Show error to user
end
```

## Installation

**User installation steps**:
1. Download APFramework mod
2. Extract to `ue4ss/Mods/APFramework/`
3. Ensure `enabled.txt` exists in mod folder
4. (Optional) Edit `framework_config.json` to set autoconnect
5. Install AP-enabled game mods (they auto-register)
6. Launch game

**File structure after installation**:
```
Palworld/Pal/Binaries/Win64/
└── ue4ss/
    └── Mods/
        ├── APFramework/           (This framework mod)
        │   ├── Scripts/
        │   │   ├── main.lua
        │   │   ├── framework_wrapper.lua
        │   │   └── config_ui.lua
        │   ├── APFrameworkCore.dll
        │   ├── enabled.txt
        │   └── framework_config.json
        ├── MyPalworldMod/         (Example AP-enabled mod)
        │   ├── Scripts/
        │   │   └── main.lua       (Uses ap_client.lua)
        │   ├── ap_config.json     (Auto-discovered)
        │   └── enabled.txt
        └── AnotherMod/            (Another AP-enabled mod)
            ├── Scripts/
            └── ap_config.json
```

## Dependencies

- **UE4SS**: Provides Lua runtime and mod system
- **LuaJIT FFI**: For loading APFrameworkCore.dll
- **APFrameworkCore.dll**: Built in Phase 1
- **ImGui** (optional): For configuration UI

## Testing

### Manual Testing

1. Install framework mod
2. Create dummy mod with `ap_config.json`
3. Launch game
4. Verify:
   - Framework loads DLL
   - IPC server starts
   - Mods are discovered
   - Registration completes
   - (Optional) Connect to AP server

### Console Commands (for debugging)

```lua
-- Status
RegisterConsoleCommand("ap_status", function()
    print("Connected: " .. tostring(framework:is_connected()))
    print("Mods registered: " .. tostring(framework:all_mods_registered()))
end)

-- Force reconnect
RegisterConsoleCommand("ap_reconnect", function()
    framework:disconnect_ap()
    framework:connect_ap("localhost", 38281, "Player1", "")
end)

-- Reload config
RegisterConsoleCommand("ap_reload_config", function()
    framework:load_config("APFramework/framework_config.json")
end)
```

## Deliverables

1. ✅ **APFramework Lua mod** (main.lua, framework_wrapper.lua)
2. ✅ **Configuration UI** (config_ui.lua - optional)
3. ✅ **Default configuration** (framework_config.json)
4. ✅ **Installation guide**
5. ✅ **Console commands documentation**

## Next Steps

After Phase 3, proceed to Phase 4 to create **example mods** that demonstrate:
- Lua mod using `ap_client.lua`
- C++ mod using `APClientLib.dll`
- Full integration examples

## Notes

- Framework mod must be loaded before any AP-enabled game mods
- UE4SS loads mods alphabetically, so naming matters (APFramework loads first)
- Framework runs entirely in background - minimal user interaction
- Configuration UI is optional - can edit JSON directly
- Console commands useful for debugging and advanced users