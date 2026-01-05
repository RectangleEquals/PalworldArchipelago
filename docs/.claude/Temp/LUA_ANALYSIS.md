# APFramework Lua Codebase Analysis

## Overview

The APFramework Lua codebase consists of 5 main files that work together to provide a bridge between UE4SS (Unreal Engine 4 Scripting System) and the Archipelago multiworld randomizer. The architecture uses a **dual-role design** where the framework acts as both an IPC server and client.

---

## File-by-File Analysis

### 1. `main.lua` - Framework Entry Point & Lifecycle Manager

**Purpose**: Orchestrates the entire framework lifecycle from initialization through normal operation.

**Key Components**:

#### Constants (Lines 11-16)
```lua
PIPE_NAME = "APFramework_default"  -- Named pipe for IPC
MODS_DIRECTORY = "ue4ss\\Mods"     -- Where to discover AP-enabled mods
CONFIG_PATH = "..."                 -- Framework configuration file
LOG_PATH = "..."                    -- Lua-side log file
FRAMEWORK_MOD_ID = "archipelago.palworld.framework"  -- Special mod ID for priority client
```

#### State Machine (Lines 25-37, 271-319)
The framework uses a **lifecycle state machine** with these states:

1. **INIT** (Lines 276-281)
   - Loads configuration from JSON
   - Creates C++ FrameworkCore instance
   - Initializes logging
   - **Transition**: → START_IPC when initialized

2. **START_IPC** (Lines 283-290)
   - Starts the IPC server (C++ component)
   - Attempts to register framework as **priority client**
   - **Transition**: → DISCOVER

3. **DISCOVER** (Lines 292-302)
   - Retries priority client registration if it failed
   - Discovers AP-enabled mods in the Mods directory
   - **Transition**: → WAIT_REG after discovery initiated

4. **WAIT_REG** (Lines 304-308)
   - Waits for all discovered mods to register via IPC
   - Times out after `registration_timeout` seconds (default 180s)
   - Generates capabilities JSON from registered mods
   - **Transition**: → CONNECT after all mods registered

5. **CONNECT** (Lines 310-314)
   - Connects to Archipelago server using apclientpp
   - Starts polling thread
   - **Transition**: → RUNNING

6. **RUNNING** (Line 316-318)
   - Normal operation
   - Framework client is polled every frame (line 335)

#### Priority Client Concept (Lines 110-161)

**Why it exists**: The framework needs to receive its own messages for debugging/logging purposes.

**What it does**:
- Creates an `APClient` instance using the special mod ID `"archipelago.palworld.framework"`
- Connects to its own IPC server (dual-role: server + client)
- Registers empty capabilities (framework provides no items/locations)
- Sets up callbacks to receive:
  - Item received events
  - Location checked events
  - Connection status changes
  - **C++ framework logs** (via `on_log` callback - line 143-147)

**Current Issue**: Priority client connection fails due to timing - the client tries to connect before the IPC server's `ConnectNamedPipe()` is ready. However, this doesn't break functionality since:
- It retries in the DISCOVER phase (line 294-296)
- The rest of the framework works without it
- Its primary purpose is receiving logs, which is optional

#### Frame Update Hook (Lines 325-336)

```lua
RegisterCustomEvent("Tick", function(deltaTime)
    if lifecycle ~= "RUNNING" then
        lifecycle_count = lifecycle_count + 1
        if lifecycle_count % 60 == 0 then  -- Only update every 60 frames
            update_lifecycle()
        end
    end

    poll_framework_client()  -- Always poll, even in RUNNING
end)
```

**Why every 60 frames**: Prevents console spam during startup. At 60 FPS, this is ~1 update per second.

**Why poll always runs**: The framework client needs to receive messages continuously once connected.

---

### 2. `framework_wrapper.lua` - C++ API Bridge

**Purpose**: Thin Lua wrapper around the C++ `APFrameworkCore.dll` native module.

**Architecture**:

```
Lua Code → framework_wrapper.lua → APFrameworkCore.dll (Lua C bindings) → C++ FrameworkCore
```

#### Key Functions:

**Initialization** (Lines 12-23):
```lua
FrameworkWrapper.init_logger(log_path)  -- Static function, must be called BEFORE creating framework
FrameworkWrapper:new(pipe_name)         -- Creates C++ FrameworkCore instance
```
**Why logger is separate**: The C++ logger must be initialized before any framework operations.

**IPC Management** (Lines 53-59):
```lua
:start_ipc()  -- Starts IPC server thread (spawns server_loop in ipc_server.cpp)
:stop_ipc()   -- Stops IPC server
```

**C++ Counterpart**: `framework_core/src/ipc_server.cpp`
- `start()` spawns thread running `server_loop()`
- `server_loop()` creates named pipe and calls `ConnectNamedPipe()` to wait for clients

**Mod Discovery** (Lines 62-72):
```lua
:discover_mods(mods_directory)    -- Scans directory for enabled.txt files
:all_mods_registered()            -- Checks if all discovered mods have registered
:get_pending_registrations()      -- Returns list of mods that haven't registered
```

**C++ Counterpart**: `framework_core/src/mod_registry.cpp`
- Scans for `enabled.txt` files
- Maintains list of expected mod IDs
- Tracks which mods have sent registration messages via IPC

**Capabilities** (Lines 75-77):
```lua
:generate_capabilities()  -- Returns JSON string of combined mod capabilities
```

**C++ Counterpart**: `framework_core/src/capabilities_generator.cpp`
- Aggregates capabilities from all registered mods
- Formats as Archipelago-compatible JSON

**AP Connection** (Lines 80-99):
```lua
:connect_ap(server, port, slot_name, password)  -- Initiates connection to AP server
:disconnect_ap()                                 -- Disconnects
:is_connected()                                  -- Checks AP connection status
:start_polling()                                 -- Starts polling thread for apclientpp
:stop_polling()                                  -- Stops polling thread
```

**C++ Counterpart**: `framework_core/src/ap_client.cpp`
- Uses `apclientpp` library to communicate with AP server
- Polling thread calls `ap_client.poll()` at ~60fps
- Handles callbacks: `room_info`, `slot_connected`, `items_received`, etc.

---

### 3. `ap_client.lua` - IPC Client Wrapper

**Purpose**: Thin wrapper around the C++ `APClientLib.dll` for mods to communicate with the framework.

**Architecture**:

```
Game Mod Lua → ap_client.lua → APClientLib.dll (Lua C bindings) → C++ IPCClient → Named Pipe → Framework IPC Server
```

#### Important: APClientLib ≠ AP Server Client

**Common Misconception**: APClientLib is **NOT** for connecting to the Archipelago server.

**Reality**: APClientLib is an **IPC client library** that connects to the framework's local IPC server.

```
+----------------+
| Game Mod       |
+----------------+
        ↓ (uses ap_client.lua)
+----------------+
| APClientLib    | ← IPC client (connects to local named pipe)
+----------------+
        ↓ (via named pipe "APFramework_default")
+----------------+
| FrameworkCore  | ← IPC server + AP client
+----------------+
        ↓ (uses apclientpp)
+----------------+
| AP Server      | ← Actual Archipelago server (network connection)
+----------------+
```

#### Key Functions:

**Constructor** (Lines 10-51):
```lua
APClient:new(mod_id)
```

**Flow**:
1. Creates C++ `IPCClient` instance (line 13)
2. IPCClient auto-connects to pipe "APFramework_default" (in `ap_client_lib.cpp:ap_client_create()`)
3. Sets `connected = false` (line 14)
4. Sets up Lua→C callback bridges (lines 26-48)
5. **Does NOT check initial connection status** ← **BUG**

**Current Bug**: Line 14 sets `connected = false`, but never updates it until first `poll()`. This causes the priority client registration to fail in `main.lua:119`.

**Fix Applied**: I added `obj.connected = obj.client:is_connected()` after line 49 (in game directory, not yet in source).

**Callbacks** (Lines 26-48):

The wrapper bridges between Lua user callbacks and C++ callbacks:

```lua
obj.client:set_item_received_callback(function(item_id, location_id, player_slot)
    if obj.on_item_received then  -- User's Lua callback
        obj.on_item_received(item_id, location_id, player_slot)
    end
end)
```

**C++ Flow**:
1. Framework receives message from AP server
2. Framework sends IPC message to mod's pipe connection
3. IPCClient reads message and calls C callback
4. C callback invokes Lua callback (via LUA_REGISTRYINDEX ref)
5. Lua callback invokes user's callback

**Poll Function** (Lines 64-69):
```lua
function APClient:poll()
    self.client:poll()  -- C++ polls for IPC messages
    self.connected = self.client:is_connected()  -- Update connection status
end
```

**Why poll is needed**: Named pipes are message-based. The client must actively check for incoming messages.

**C++ Counterpart**: `client_lib/src/ap_client_lib.cpp:ap_client_poll()`
- Calls `IPCClient::poll_messages()`
- Reads from named pipe using `PeekNamedPipe()` + `ReadFile()`
- Dispatches messages to registered callbacks

**Registration** (Lines 53-62):
```lua
function APClient:register(capabilities)
```

Sends a registration message to the framework with the mod's capabilities (items, locations, regions).

**C++ Flow**:
1. Encodes capabilities to JSON (line 55)
2. Sends IPC message of type "register" (calls `ap_client_register()`)
3. Framework's IPC server receives it in `handle_client()` (ipc_server.cpp)
4. Framework adds mod to registry
5. Framework sends "registration_complete" response
6. Client receives it and calls `on_registration_complete` callback

---

### 4. `config.lua` - Configuration Manager

**Purpose**: Loads and saves framework configuration from JSON.

**Schema**:
```json
{
    "server": "archipelago.gg",      // AP server hostname
    "port": 38281,                    // AP server port
    "slot_name": "P1",                // Player slot name
    "password": "",                   // Slot password
    "autoconnect": false,             // Connect automatically on startup?
    "registration_timeout": 180,      // Max wait time for mod registration (seconds)
    "log_mode": "framework_only",     // "minimal", "framework_only", "all"
    "log_verbosity": "info"           // "debug", "info", "warning", "error"
}
```

**Uses lunajson** for robust JSON parsing instead of manual string manipulation.

**Boolean Handling** (Lines 49-52):
```lua
if config_data.autoconnect ~= nil then
    self.autoconnect = config_data.autoconnect
end
```
**Why explicit nil check**: Distinguishes between `false` (explicit) and missing field (use default).

---

### 5. `lunajson.lua` - JSON Parser

**Purpose**: Third-party pure-Lua JSON encoder/decoder.

**Why not use C library**:
- Portability - works in any Lua environment
- No need to build/distribute additional binaries
- lunajson is well-tested and robust

**Usage**:
```lua
local json = require("lunajson")
local data = json.decode(json_string)
local json_string = json.encode(lua_table)
```

---

## Execution Flow

### Startup Sequence

1. **UE4SS loads mod** (on game start)
   - Executes `main.lua`
   - Prints "[APFramework] Loading APFramework v2.0.0..."

2. **Module Loading** (Lines 6-8)
   - `require("framework_wrapper")` → Loads C++ bindings
   - `require("config")` → Loads config manager
   - `require("ap_client")` → Loads IPC client wrapper

3. **Logger Initialization** (Line 22)
   - Calls `FrameworkWrapper.init_logger()` BEFORE creating framework
   - C++ creates log file and enables file output

4. **Hook Registration** (Lines 325-341)
   - Registers `Tick` event (runs every frame)
   - Registers `ReceiveShutdown` event (runs on game close)

5. **Tick-Driven State Machine**
   - Every 60 frames (~1 second at 60 FPS), calls `update_lifecycle()`
   - Progresses through states: INIT → START_IPC → DISCOVER → WAIT_REG → CONNECT → RUNNING

### First Tick (Lifecycle: INIT)

1. **initialize_framework()** (Lines 55-97)
   - Creates `Config` instance and loads from JSON
   - Creates `FrameworkWrapper` (calls C++ `framework_create()`)
   - C++ creates FrameworkCore, IPCServer, ModRegistry, etc.
   - Loads config into C++ for logging configuration
   - **Result**: `state.initialized = true`, lifecycle → START_IPC

### Second Tick (Lifecycle: START_IPC)

1. **start_ipc()** (Lines 100-107)
   - Calls `framework:start_ipc()`
   - C++ spawns server thread running `server_loop()`
   - Server thread creates named pipe "APFramework_default"
   - Server thread calls `ConnectNamedPipe()` and **blocks** waiting for client
   - **Result**: `state.ipc_started = true`

2. **register_framework_client()** (Lines 110-161)
   - Creates `APClient` instance with mod ID "archipelago.palworld.framework"
   - APClient's C++ constructor calls `IPCClient::connect()`
   - **RACE CONDITION**: Client tries `CreateFile()` on pipe
     - If server thread hasn't called `ConnectNamedPipe()` yet: **FAILS** with ERROR_FILE_NOT_FOUND
     - Sets `connected = false`
   - Check on line 119 fails, function returns early
   - **Result**: Priority client NOT registered, will retry next tick

3. **Lifecycle transition** → DISCOVER

### Third Tick (Lifecycle: DISCOVER)

1. **Retry framework client** (Lines 294-296)
   - Calls `register_framework_client()` again
   - Usually succeeds this time (server has had time to call ConnectNamedPipe)
   - **Problem**: `ap_client.lua:new()` doesn't set initial `connected` status
   - Still fails even though connection might succeed

2. **discover_mods()** (Lines 171-179)
   - Calls `framework:discover_mods(MODS_DIRECTORY)`
   - C++ scans for files matching `*/enabled.txt`
   - Each `enabled.txt` should contain mod ID
   - C++ adds mod IDs to "expected registrations" list
   - **Result**: `state.mods_discovered = true`, lifecycle → WAIT_REG

### Fourth+ Ticks (Lifecycle: WAIT_REG)

1. **check_registration()** (Lines 182-217)
   - Calls `framework:all_mods_registered()`
   - C++ checks if all expected mods have sent "register" IPC messages
   - If timeout expires (180s default), prints error
   - Once all registered, generates capabilities JSON
   - Combines capabilities from all mods into single JSON
   - Saves to `APCapabilities_<slot_name>.json`
   - **Result**: `state.all_registered = true`, lifecycle → CONNECT

### Fifth Tick (Lifecycle: CONNECT)

1. **connect_to_ap()** (Lines 220-260)
   - Calls `framework:connect_ap(server, port, slot_name, password)`
   - C++ creates `ws://` URI (e.g., "ws://localhost:38281")
   - C++ calls `APClient::reconnect()` (apclientpp)
   - Stores slot connection parameters
   - Sets up callbacks:
     - `room_info` → Calls `ConnectSlot()` with stored parameters
     - `slot_connected` → Connection successful
     - `items_received` → Item received from AP
     - `location_checked` → Location checked notification
   - Starts polling thread (calls `ap_client.poll()` at ~60fps)
   - **Result**: `state.connected = true`, lifecycle → RUNNING

### Ongoing (Lifecycle: RUNNING)

1. **Every Frame** (Line 335)
   - Calls `poll_framework_client()`
   - If priority client is connected, polls for IPC messages
   - Receives framework logs via `on_log` callback

2. **C++ Polling Thread** (background)
   - Calls `apclientpp::poll()` continuously
   - Processes WebSocket messages from AP server
   - Triggers callbacks that queue IPC messages to connected mods
   - Mods poll and receive messages on their next frame

---

## Data Flow: Location Check Example

1. **Game Mod** detects player obtained item
2. **Mod Lua** calls `client:check_location(location_id)`
3. **ap_client.lua** (line 72) calls `self.client:check_location(location_id)`
4. **C++ APClientLib** creates IPC message: `{"type": "check_location", "mod_id": "...", "data": {"location_id": 123}}`
5. **C++ IPCClient** writes JSON to named pipe (with `\n` delimiter)
6. **C++ IPCServer** reads message in `handle_client()` (ipc_server.cpp:168-208)
7. **C++ IPCServer** dispatches to message handler (framework_core.cpp)
8. **C++ FrameworkCore** calls `ap_client_wrapper::check_location()`
9. **C++ APClientWrapper** calls `apclientpp::LocationChecks({location_id})`
10. **apclientpp** sends WebSocket message to AP server
11. **AP Server** broadcasts location check to all players
12. **AP Server** sends items to appropriate slots
13. **apclientpp** receives item message, triggers callback
14. **C++ APClientWrapper** queues message for mod
15. **C++ FrameworkCore** sends IPC message to mod: `{"type": "item_received", ...}`
16. **C++ IPCClient** (mod side) reads message during `poll()`
17. **C++ APClientLib** calls registered callback
18. **Lua Binding** invokes Lua callback from LUA_REGISTRYINDEX
19. **ap_client.lua** calls user's `on_item_received` callback
20. **Game Mod** processes item (e.g., gives player an upgrade)

---

## Critical Issues Identified

### 1. Priority Client Connection Timing Race

**Location**: `main.lua:119`, `ap_client.lua:14`

**Problem**:
- Priority client created immediately after IPC server starts (main.lua:287)
- IPC server's `ConnectNamedPipe()` might not be ready
- `ap_client.lua` sets `connected = false` and never updates until first `poll()`
- Check on line 119 fails even if connection would succeed

**Current Impact**:
- Priority client never registers successfully
- Framework logs not received in Lua
- **Does NOT break core functionality** - AP connection works fine

**Proper Fix**:
```lua
-- In ap_client.lua:new(), after line 48:
obj.connected = obj.client:is_connected()  -- Check initial status
return obj
```

Already applied to game directory, needs to be tested.

### 2. Missing Priority Client Purpose Documentation

**Issue**: Code comments don't explain WHY priority client exists.

**Actual Purpose**:
- Receive all framework logs for display in UE4SS console
- Potential future use for framework-level commands/events
- **Not essential for operation**

**Recommendation**: Either:
1. Fix the timing and document its purpose, OR
2. Remove priority client feature if only used for logs (use direct C++ logging instead)

### 3. No Error Details on Connection Failure

**Location**: `main.lua:120`

**Problem**: When connection fails, no error message from C++ layer.

**Recommendation**: Expose `ap_client_get_last_error()` in Lua bindings:
```lua
if not framework_client.connected then
    local error = framework_client:get_last_error()
    print("[APFramework] IPC Error: " .. (error or "unknown"))
end
```

---

## Architecture Summary

### Dual-Role Design

The framework plays **two roles simultaneously**:

1. **IPC Server** (via FrameworkCore)
   - Accepts connections from game mods
   - Aggregates capabilities
   - Relays messages to/from AP server

2. **IPC Client** (priority client, via APClientLib)
   - Connects to its own IPC server
   - Receives debug logs
   - Special mod ID grants elevated privileges (potential future use)

### Why This Design?

**Separation of Concerns**:
- **FrameworkCore** (C++) handles complex networking (apclientpp, WebSockets, JSON)
- **Game Mods** (Lua) use simple IPC interface (no dependencies)
- **APClientLib** (C++) provides Lua-friendly IPC client (hides pipe complexity)

**Benefits**:
- Mods don't need to link against apclientpp
- Framework can restart AP connection without reloading mods
- Single source of truth for AP connection state
- Easy to add new mods without modifying framework

**Tradeoffs**:
- Added complexity (IPC layer, dual-role)
- Timing issues (race conditions during startup)
- Debugging requires checking both Lua and C++ logs

---

## File Relationships

```
main.lua
  ├─ requires framework_wrapper.lua
  │    └─ requires APFrameworkCore.dll (C++ module)
  │         └─ Uses: ipc_server.cpp, ap_client.cpp, mod_registry.cpp, etc.
  ├─ requires config.lua
  │    └─ requires lunajson.lua
  └─ requires ap_client.lua
       └─ requires APClientLib.dll (C++ module)
            └─ Uses: ipc_client.cpp
```

### IPC Message Flow

```
Game Mod (Lua)
    ↓ (calls ap_client:check_location)
ap_client.lua
    ↓ (calls client_lib:check_location)
APClientLib.dll
    ↓ (calls IPCClient::send_message)
Named Pipe "APFramework_default"
    ↓ (Windows IPC)
FrameworkCore.dll (IPCServer::handle_client)
    ↓ (dispatches message)
APClientWrapper (apclientpp)
    ↓ (WebSocket)
Archipelago Server
```

---

## Recommendations

1. **Fix Priority Client Timing**
   - Apply `obj.connected = obj.client:is_connected()` fix to source repository
   - Rebuild and test

2. **Add Debug Logging**
   - Expose `get_last_error()` in Lua bindings
   - Log Windows error codes on pipe connection failure

3. **Document Priority Client**
   - Add comments explaining its purpose
   - Consider making it optional (config flag)

4. **Consider Removing Priority Client**
   - If only used for logs, use direct C++ logging instead
   - Simplifies architecture, removes race condition

5. **Add IPC Health Checks**
   - Periodic verification that pipe is still connected
   - Auto-reconnect on pipe failure

6. **Improve Error Messages**
   - Show specific Windows errors (ERROR_FILE_NOT_FOUND, ERROR_PIPE_BUSY, etc.)
   - Guide users to solutions (wait, check server status, etc.)