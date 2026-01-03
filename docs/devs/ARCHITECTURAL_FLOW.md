# APFramework - System Architectural Flow

This document provides a comprehensive overview of the APFramework system architecture and complete data flow from initialization through runtime operation.

## Table of Contents

1. [System Overview](#system-overview)
2. [Component Architecture](#component-architecture)
3. [Complete System Flow](#complete-system-flow)
4. [Thread Model](#thread-model)
5. [Message Flow Diagrams](#message-flow-diagrams)
6. [Configuration System](#configuration-system)
7. [State Machine](#state-machine)

---

## System Overview

APFramework is an **Inter-Process Communication (IPC) based** Archipelago integration framework for Palworld. It replaces the previous submodule architecture with a clean separation between the core framework and game mods.

### Key Design Principles

- **Separation of Concerns**: C++ handles complex AP protocol and IPC; Lua handles UE4SS integration
- **Non-Blocking**: Three concurrent threads ensure smooth operation
- **Promise-Based Registration**: Framework discovers mods before they register, preventing race conditions
- **Dynamic Capabilities**: APCapabilities.json generated at runtime from registered mods
- **Native Lua Integration**: Proper Lua C API bindings (no FFI dependency)

### Architecture Layers

```
┌─────────────────────────────────────────────────────────────┐
│                    Palworld Game (UE4)                       │
├─────────────────────────────────────────────────────────────┤
│                         UE4SS                                │
├──────────────────┬────────────────────────┬─────────────────┤
│  APFramework Mod │    Game-Specific Mods  │  Other Mods     │
│  (main.lua)      │    (e.g., Palworld)    │                 │
│        ↓         │           ↓            │                 │
│ framework_wrapper│      APClient Lib      │                 │
│        ↓         │        (Lua/C++)       │                 │
└────────┼─────────┴──────────┼─────────────┴─────────────────┘
         │                    │
         └─────────┬──────────┘
                   │ IPC (Named Pipes)
         ┌─────────┴──────────┐
         │                    │
┌────────▼────────────────────▼────────┐
│      APFrameworkCore.dll (C++)       │
│  ┌──────────────────────────────┐   │
│  │     FrameworkCore            │   │
│  │  (Component Orchestrator)    │   │
│  └─────────┬────────────────────┘   │
│            │                         │
│  ┌─────────┼─────────────────────┐  │
│  │         │  Core Components    │  │
│  │  ┌──────▼──────┐              │  │
│  │  │ IPCServer   │              │  │
│  │  └─────────────┘              │  │
│  │  ┌─────────────┐              │  │
│  │  │APClientWrap │              │  │
│  │  └─────────────┘              │  │
│  │  ┌─────────────┐              │  │
│  │  │ModRegistry  │              │  │
│  │  └─────────────┘              │  │
│  │  ┌─────────────┐              │  │
│  │  │MsgRouter    │              │  │
│  │  └─────────────┘              │  │
│  │  ┌─────────────┐              │  │
│  │  │PollingThread│              │  │
│  │  └─────────────┘              │  │
│  │  ┌─────────────┐              │  │
│  │  │CapGenerator │              │  │
│  │  └─────────────┘              │  │
│  │  ┌─────────────┐              │  │
│  │  │ConfigMgr    │              │  │
│  │  └─────────────┘              │  │
│  └──────────────────────────────┘  │
└──────────────┬──────────────────────┘
               │ WebSocket
               │
      ┌────────▼────────┐
      │  AP Server      │
      │ (archipelago.gg)│
      └─────────────────┘
```

---

## Component Architecture

### C++ Framework Core Components

The framework core consists of 7 major components orchestrated by [FrameworkCore](../../src/framework_core/include/framework_core.h):

#### 1. FrameworkCore (Orchestrator)

**Location**: [framework_core.h](../../src/framework_core/include/framework_core.h) / [framework_core.cpp](../../src/framework_core/src/framework_core.cpp)

**Purpose**: Main orchestrator that coordinates all components and manages the framework lifecycle.

**Key Responsibilities**:
- IPC server lifecycle management (`start_ipc()`, `stop_ipc()`)
- AP connection management (`connect_ap()`, `disconnect_ap()`)
- Mod discovery and registration coordination (`discover_mods()`, `all_mods_registered()`)
- Polling thread control (`start_polling()`, `stop_polling()`)
- Configuration management (`load_config()`, `get_config()`)
- IPC message routing and handling (`handle_mod_registration()`, etc.)

**State Management**:
- Tracks IPC server state
- Tracks AP connection state
- Tracks registration completion state

#### 2. APClientWrapper

**Location**: [ap_client.h](../../src/framework_core/include/ap_client.h) / [ap_client.cpp](../../src/framework_core/src/ap_client.cpp)

**Purpose**: Wraps the apclientpp library for Archipelago server communication.

**Key Features**:
- WebSocket-based connection to AP server
- Continuous polling for incoming messages
- Thread-safe message queuing (`MessageQueue<APMessage>`)
- Location checks and status updates
- Event callbacks (items received, locations checked, etc.)

**Message Types**:
- `ItemReceived` - Player received an item
- `LocationChecked` - A location was checked
- `SlotConnected` - Successfully connected to slot
- `Disconnected` - Disconnected from server
- `RoomInfo` - Room information received
- `DataPackage` - Data package received

**Implementation Pattern**:
```cpp
struct APClientImpl {
    std::unique_ptr<::APClient> client;  // Opaque pointer to avoid namespace conflicts
    std::string uuid;
    std::string game;
    std::string current_uri;
};
```

**Thread Safety**: All public methods are thread-safe using internal mutexes.

#### 3. IPCServer

**Location**: [ipc_server.h](../../src/framework_core/include/ipc_server.h) / [ipc_server.cpp](../../src/framework_core/src/ipc_server.cpp)

**Purpose**: Windows Named Pipes-based IPC server for mod communication.

**Key Features**:
- Listens on named pipe (default: `\\.\pipe\APFramework_default`)
- Per-mod message queues (thread-safe)
- Multiple simultaneous mod connections
- Message broadcasting to all mods
- Dedicated server thread

**IPC Message Format**:
```cpp
struct IPCMessage {
    std::string type;       // "register", "location_check", "connect", etc.
    std::string mod_id;     // Sender/receiver mod ID
    std::string data_json;  // Payload as JSON string
};
```

**Message Types** (Mod → Framework):
- `register` - Mod registration with capabilities
- `location_check` - Check a location
- `connect` - Request AP connection
- `status_update` - Update player status

**Message Types** (Framework → Mod):
- `item_received` - Item delivered to player
- `location_checked` - Location was checked
- `registration_complete` - All mods registered
- `connection_status` - AP connection status change

**Connection Lifecycle**:
1. Mod connects to named pipe
2. Mod registers with capabilities
3. Framework routes messages to/from mod
4. Mod disconnects or framework shuts down

#### 4. MessageRouter

**Location**: [message_router.h](../../src/framework_core/include/message_router.h) / [message_router.cpp](../../src/framework_core/src/message_router.cpp)

**Purpose**: Routes AP messages to appropriate mods based on item/location ownership.

**Routing Tables**:
- `item_to_mod_`: Maps item IDs → mod_id
- `location_to_mod_`: Maps location IDs → mod_id

**Routing Logic**:
- **Targeted Routing**: `ItemReceived`, `LocationChecked` (uses routing tables)
- **Broadcast Routing**: `SlotConnected`, `Disconnected`, `RoomInfo`, `DataPackage` (all mods)

**Table Population**:
Routing tables are populated during mod registration from the capabilities data:
```cpp
void MessageRouter::register_mod_capabilities(
    const std::string& mod_id,
    const std::vector<int64_t>& items,
    const std::vector<int64_t>& locations
);
```

**Example Flow**:
```
AP Server sends ItemReceived(item_id=100042)
    ↓
MessageRouter looks up item_to_mod_[100042] → "PalworldMod"
    ↓
IPCServer sends to "PalworldMod" only
```

#### 5. ModRegistry

**Location**: [mod_registry.h](../../src/framework_core/include/mod_registry.h) / [mod_registry.cpp](../../src/framework_core/src/mod_registry.cpp)

**Purpose**: Promise-based mod discovery and registration tracking.

**Two-Phase System**:

**Phase 1: Discovery**
- Scans for `ap_config.json` files in mod directories
- Parses `mod_id` from each config
- Adds to `discovered_mods_` set
- Sets expectation for registration

**Phase 2: Registration**
- Waits for mods to register via IPC
- Validates mod_id matches discovered
- Adds to `registered_mods_` set
- Stores capabilities

**ModCapabilities Structure**:
```cpp
struct ModCapabilities {
    std::string mod_id;
    std::vector<int64_t> items;       // Item IDs this mod handles
    std::vector<int64_t> locations;   // Location IDs this mod handles
    std::vector<std::string> regions; // Region names
};
```

**Key Methods**:
- `discover_mod(mod_id)` - Add to expected mods
- `register_mod(mod_id, caps)` - Register mod with capabilities
- `all_mods_registered()` - Check if all discovered mods have registered
- `get_registered_mods()` - Get list of registered mods
- `get_mod_capabilities(mod_id)` - Get capabilities for specific mod

**Promise Pattern**:
```
discover_mod("ModA")  →  registered_mods_.count("ModA") == 0
discover_mod("ModB")  →  registered_mods_.count("ModB") == 0
                         all_mods_registered() → false
    ↓
register_mod("ModA")  →  registered_mods_.insert("ModA")
                         all_mods_registered() → false
    ↓
register_mod("ModB")  →  registered_mods_.insert("ModB")
                         all_mods_registered() → true ✓
```

#### 6. PollingThread

**Location**: [polling_thread.h](../../src/framework_core/include/polling_thread.h) / [polling_thread.cpp](../../src/framework_core/src/polling_thread.cpp)

**Purpose**: Background thread for continuous AP server polling.

**Operation**:
- Polls at configurable intervals (default: 16ms ≈ 60fps)
- Calls `ap_client_->poll()` continuously
- Retrieves messages via `ap_client_->get_messages()`
- Routes via `message_router_->route_ap_message()`
- Non-blocking for main thread

**Polling Loop**:
```cpp
void PollingThread::polling_loop() {
    while (running_) {
        ap_client_->poll();

        auto messages = ap_client_->get_messages();
        for (const auto& msg : messages) {
            message_router_->route_ap_message(msg);
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(polling_interval_ms_)
        );
    }
}
```

**Thread Control**:
- `start()` - Spawns background thread
- `stop()` - Signals thread to stop and joins
- `is_running()` - Check if thread is active
- `set_interval(ms)` - Adjust polling frequency

#### 7. CapabilitiesGenerator

**Location**: [capabilities_generator.h](../../src/framework_core/include/capabilities_generator.h) / [capabilities_generator.cpp](../../src/framework_core/src/capabilities_generator.cpp)

**Purpose**: Generates `APCapabilities.json` from registered mod data.

**Structured Types**:
```cpp
struct ItemDefinition {
    int64_t id;
    std::string name;
    std::string classification;  // "filler", "useful", "progression", "trap"
    std::string mod_id;
};

struct LocationDefinition {
    int64_t id;
    std::string name;
    std::string region;
    std::string mod_id;
};

struct RegionDefinition {
    std::string name;
    std::vector<std::string> connects_to;
    std::vector<int64_t> locations;
    std::string mod_id;
};
```

**JSON Output Format**:
```json
{
  "items": [
    {
      "id": 100000,
      "name": "Super Pickaxe",
      "classification": "useful",
      "mod_id": "PalworldMod"
    }
  ],
  "locations": [
    {
      "id": 200000,
      "name": "Chest in Cave",
      "region": "Starting Area",
      "mod_id": "PalworldMod"
    }
  ],
  "regions": [
    {
      "name": "Starting Area",
      "connects_to": ["Mountain Path"],
      "locations": [200000],
      "mod_id": "PalworldMod"
    }
  ]
}
```

**Generation Process**:
1. Iterate through all registered mods
2. Merge items, locations, and regions
3. Validate uniqueness (no ID collisions)
4. Generate JSON structure
5. Write to file

#### 8. ConfigManager

**Location**: [config_manager.h](../../src/framework_core/include/config_manager.h) / [config_manager.cpp](../../src/framework_core/src/config_manager.cpp)

**Purpose**: Configuration and connection profile management.

**Configuration Structure**:
```cpp
struct ConnectionProfile {
    std::string name;
    std::string server;
    int port;
    std::string slot_name;
    std::string password;
    bool autoconnect;
};

struct FrameworkConfig {
    ConnectionProfile active_profile;
    std::map<std::string, ConnectionProfile> saved_profiles;
    int polling_interval_ms;
    bool enable_logging;
    std::string log_level;
    std::map<std::string, std::string> mod_overrides;
};
```

**Key Methods**:
- `load_from_file(path)` - Load config from JSON
- `save_to_file(path)` - Save config to JSON
- `get_active_profile()` - Get current connection profile
- `set_active_profile(name)` - Switch to different profile
- `add_profile(profile)` - Add new connection profile

#### 9. Logger

**Location**: [logger.h](../../src/framework_core/include/logger.h) / [logger.cpp](../../src/framework_core/src/logger.cpp)

**Purpose**: Thread-safe file logging system.

**Features**:
- Singleton pattern (`Logger::instance()`)
- Timestamped entries (YYYY-MM-DD HH:MM:SS.mmm)
- Log levels: DEBUG, INFO, WARNING, ERROR
- Flushes after every write (crash-safe)
- Convenience macros: `LOG_INFO()`, `LOG_ERROR()`, etc.

**Usage**:
```cpp
// Initialize (from Lua)
Logger::instance().init("path/to/log.txt");

// Log messages (from C++)
LOG_INFO("Framework started");
LOG_ERROR("Connection failed: {}", error_msg);
LOG_DEBUG("Received {} messages", count);
```

**Thread Safety**: Mutex-protected for concurrent logging from multiple threads.

### Lua Integration Layer

#### Native Lua C API Bindings

**Location**: [lua_bindings.h](../../src/framework_core/include/lua_bindings.h) / [lua_bindings.cpp](../../src/framework_core/src/lua_bindings.cpp)

**Critical Design**: UE4SS uses **Lua 5.4** (NOT LuaJIT), so FFI is unavailable. The framework uses native Lua C API bindings.

**Module Entry Point**:
```cpp
extern "C" __declspec(dllexport) int luaopen_APFrameworkCore(lua_State* L);
```

**Binding Pattern**:
- Userdata with metatables for OOP interface
- `__gc` metamethod for automatic garbage collection
- `__index` metamethod for method dispatch
- Module table with `create()` and `init_logger()` functions

**Lua API**:
```lua
local APFrameworkCore = require("APFrameworkCore")

-- Initialize logger (must be first)
APFrameworkCore.init_logger("path/to/log.txt")

-- Create framework instance
local handle = APFrameworkCore.create("pipe_name")

-- Call methods
handle:start_ipc()
handle:discover_mods("directory")
handle:connect_ap(server, port, slot, password)
handle:start_polling()
-- ... etc
```

**Bound Methods**:
- `create(pipe_name)` - Create FrameworkCore instance
- `init_logger(path)` - Initialize logging system
- `start_ipc()` - Start IPC server
- `stop_ipc()` - Stop IPC server
- `discover_mods(directory)` - Scan for mods
- `all_mods_registered()` - Check registration status
- `generate_capabilities()` - Generate APCapabilities.json
- `connect_ap(server, port, slot, password)` - Connect to AP
- `disconnect_ap()` - Disconnect from AP
- `start_polling()` - Start polling thread
- `stop_polling()` - Stop polling thread
- `load_config(path)` - Load configuration

#### Lua Scripts

##### main.lua

**Location**: [APFramework/Scripts/main.lua](../../APFramework/Scripts/main.lua)

**Purpose**: Entry point for the UE4SS mod. Implements the framework lifecycle state machine.

**State Machine** (see [State Machine](#state-machine) section for details):
```
INIT → START_IPC → DISCOVER → WAIT_REG → CONNECT → RUNNING
```

**Key Features**:
- Driven by UE4SS `Tick` event (every frame)
- Updates state machine every 60 ticks (~60fps)
- Verbose logging at each stage
- Registration timeout handling (default: 180s)
- Automatic capability generation
- Autoconnect support

**Hooks**:
- `RegisterCustomEvent("Tick")` - Drives lifecycle state machine
- `RegisterHook("/Script/Engine.GameInstance:ReceiveShutdown")` - Cleanup

##### framework_wrapper.lua

**Location**: [APFramework/Scripts/framework_wrapper.lua](../../APFramework/Scripts/framework_wrapper.lua)

**Purpose**: Lua wrapper class around the native C module.

**Features**:
- Loads native module: `local core = require("APFrameworkCore")`
- Provides convenient OOP interface
- Handles logger initialization
- Manages framework lifecycle
- Direct passthrough to C++ methods

**Class Structure**:
```lua
FrameworkWrapper = {}
FrameworkWrapper.__index = FrameworkWrapper

function FrameworkWrapper:new(pipe_name)
    local instance = setmetatable({}, FrameworkWrapper)
    instance.handle = core.create(pipe_name)
    return instance
end

function FrameworkWrapper:start_ipc()
    return self.handle:start_ipc()
end
-- ... etc
```

##### config.lua

**Location**: [APFramework/Scripts/config.lua](../../APFramework/Scripts/config.lua)

**Purpose**: Simple configuration parser for `framework_config.json`.

**Parsed Fields**:
- `server` - AP server address
- `port` - AP server port
- `slot_name` - Player slot name
- `password` - Connection password
- `autoconnect` - Auto-connect on startup (boolean)
- `registration_timeout` - Timeout for mod registration (seconds)

**Implementation**: Uses basic regex parsing (no JSON library dependency).

---

## Complete System Flow

This section describes the complete data flow from game startup through runtime operation.

### Phase 1: Framework Initialization

**Trigger**: Game starts, UE4SS loads mods

```
1. Game Starts
   │
   ↓
2. UE4SS loads APFramework mod
   │
   ↓
3. main.lua executes
   │
   ├─→ Set LOG_PATH = "APFramework/Logs/framework.log"
   ├─→ Set PIPE_NAME = "APFramework_default"
   ├─→ Set CONFIG_PATH = "APFramework/framework_config.json"
   ├─→ Set MODS_DIRECTORY = "UE4SS/Mods"
   │
   ↓
4. Initialize logger
   │
   └─→ FrameworkWrapper.init_logger(LOG_PATH)
       │
       └─→ APFrameworkCore.init_logger(LOG_PATH)
           │
           └─→ Logger::instance().init("APFramework/Logs/framework.log")
               │
               └─→ Opens log file, writes initial entry
   │
   ↓
5. Create framework instance
   │
   └─→ framework = FrameworkWrapper:new(PIPE_NAME)
       │
       └─→ handle = APFrameworkCore.create("APFramework_default")
           │
           └─→ new FrameworkCore("APFramework_default")
               │
               ├─→ Create APClientWrapper("uuid", "Palworld")
               ├─→ Create IPCServer("APFramework_default")
               ├─→ Create MessageRouter(ap_client_, ipc_server_)
               ├─→ Create ModRegistry()
               ├─→ Create PollingThread(ap_client_, message_router_)
               ├─→ Create CapabilitiesGenerator()
               ├─→ Create ConfigManager()
               └─→ LOG: "FrameworkCore created"
   │
   ↓
6. Load configuration
   │
   └─→ framework:load_config(CONFIG_PATH)
       │
       └─→ handle:load_config("APFramework/framework_config.json")
           │
           └─→ ConfigManager::load_from_file("framework_config.json")
               │
               ├─→ Parse JSON file
               ├─→ Extract server, port, slot_name, password
               ├─→ Extract autoconnect, polling_interval_ms
               └─→ LOG: "Config loaded: server={}, port={}, autoconnect={}"
   │
   ↓
7. Register event hooks
   │
   ├─→ RegisterCustomEvent("Tick", on_tick_callback)
   └─→ RegisterHook("ReceiveShutdown", on_shutdown_callback)
   │
   ↓
8. Set initial state = "INIT"
   │
   └─→ LOG: "Framework initialized, entering state machine"
```

**State**: Framework initialized, waiting for first tick event.

### Phase 2: IPC Server & Mod Discovery

**Trigger**: State machine enters `START_IPC` state (first tick)

```
9. State: INIT → START_IPC
   │
   ↓
10. Start IPC server
    │
    └─→ framework:start_ipc()
        │
        └─→ handle:start_ipc()
            │
            └─→ IPCServer::start()
                │
                ├─→ Create named pipe: "\\.\pipe\APFramework_default"
                ├─→ Spawn server thread
                │   │
                │   └─→ Server thread runs ipc_server_thread()
                │       │
                │       └─→ Loop: accept connections, read messages
                │
                └─→ LOG: "IPC server started on pipe: APFramework_default"
    │
    ↓
11. State: START_IPC → DISCOVER
    │
    └─→ LOG: "Transitioning to DISCOVER state"
    │
    ↓
12. Discover mods
    │
    └─→ framework:discover_mods(MODS_DIRECTORY)
        │
        └─→ handle:discover_mods("UE4SS/Mods")
            │
            └─→ FrameworkCore::discover_mods("UE4SS/Mods")
                │
                ├─→ Scan for files: "UE4SS/Mods/*/ap_config.json"
                │   │
                │   ├─→ Found: "UE4SS/Mods/PalworldMod/ap_config.json"
                │   ├─→ Found: "UE4SS/Mods/AnotherMod/ap_config.json"
                │   └─→ ...
                │
                ├─→ For each ap_config.json:
                │   │
                │   ├─→ Parse JSON
                │   ├─→ Extract mod_id
                │   └─→ ModRegistry::discover_mod(mod_id)
                │       │
                │       └─→ discovered_mods_.insert(mod_id)
                │
                └─→ LOG: "Discovered {} mods: {}", count, mod_ids
    │
    ↓
13. State: DISCOVER → WAIT_REG
    │
    ├─→ Start registration timer
    └─→ LOG: "Waiting for {} mods to register (timeout: 180s)"
```

**State**: IPC server running, mods can now connect.

### Phase 3: Mod Registration

**Trigger**: Mods connect to IPC pipe and send registration messages

```
14. Mod connects to IPC pipe
    │
    └─→ Mod opens: "\\.\pipe\APFramework_default"
        │
        ├─→ IPC server thread accepts connection
        └─→ LOG: "Mod connected to IPC pipe"
    │
    ↓
15. Mod sends registration message
    │
    └─→ IPCMessage {
            type: "register",
            mod_id: "PalworldMod",
            data_json: {
                "items": [
                    {"id": 100000, "name": "Super Pickaxe", "classification": "useful"},
                    {"id": 100001, "name": "Fast Boots", "classification": "progression"}
                ],
                "locations": [
                    {"id": 200000, "name": "Chest in Cave", "region": "Starting Area"},
                    {"id": 200001, "name": "Boss Reward", "region": "Mountain Path"}
                ],
                "regions": [
                    {"name": "Starting Area", "connects_to": ["Mountain Path"]},
                    {"name": "Mountain Path", "connects_to": ["Starting Area", "Peak"]}
                ]
            }
        }
    │
    ↓
16. IPC server receives message
    │
    └─→ IPCServer::ipc_server_thread()
        │
        ├─→ Read message from pipe
        ├─→ Deserialize to IPCMessage
        └─→ Push to main message queue
    │
    ↓
17. Framework processes registration
    │
    └─→ FrameworkCore::handle_mod_registration(msg)
        │
        ├─→ Parse mod_id and capabilities from data_json
        │   │
        │   ├─→ Extract items: [{id, name, classification}...]
        │   ├─→ Extract locations: [{id, name, region}...]
        │   └─→ Extract regions: [{name, connects_to}...]
        │
        ├─→ Register in CapabilitiesGenerator
        │   │
        │   └─→ CapabilitiesGenerator::register_mod(mod_id, items, locations, regions)
        │       │
        │       ├─→ Store item definitions
        │       ├─→ Store location definitions
        │       ├─→ Store region definitions
        │       └─→ LOG: "Registered {} items, {} locations for {}",
        │                 item_count, location_count, mod_id
        │
        ├─→ Register in ModRegistry
        │   │
        │   └─→ ModRegistry::register_mod(mod_id, capabilities)
        │       │
        │       ├─→ Validate mod_id in discovered_mods_
        │       ├─→ Add to registered_mods_
        │       ├─→ Store capabilities
        │       └─→ LOG: "Mod registered: {} ({}/{} total)",
        │                 mod_id, registered_count, discovered_count
        │
        ├─→ Register in IPCServer
        │   │
        │   └─→ IPCServer::register_mod(mod_id, connection)
        │       │
        │       ├─→ Create message queue for mod
        │       └─→ LOG: "IPC connection established for {}", mod_id
        │
        └─→ Update MessageRouter routing tables
            │
            └─→ MessageRouter::register_mod_capabilities(mod_id, items, locations)
                │
                ├─→ For each item_id:
                │   └─→ item_to_mod_[item_id] = mod_id
                │
                ├─→ For each location_id:
                │   └─→ location_to_mod_[location_id] = mod_id
                │
                └─→ LOG: "Routing tables updated for {}: {} items, {} locations",
                          mod_id, item_count, location_count
    │
    ↓
18. Check if all mods registered
    │
    └─→ If ModRegistry::all_mods_registered():
        │
        ├─→ State: WAIT_REG → GENERATE_CAPABILITIES (internal)
        └─→ LOG: "All mods registered!"
```

**State**: Waiting for remaining mods or all registered.

**Note**: Steps 14-18 repeat for each mod that registers.

### Phase 4: Registration Complete

**Trigger**: All discovered mods have registered

```
19. All mods registered
    │
    ↓
20. Generate APCapabilities.json
    │
    └─→ framework:generate_capabilities()
        │
        └─→ handle:generate_capabilities()
            │
            └─→ CapabilitiesGenerator::generate_json()
                │
                ├─→ Merge all mod items into single array
                ├─→ Merge all mod locations into single array
                ├─→ Merge all mod regions into single array
                ├─→ Validate no ID collisions
                ├─→ Generate JSON structure
                ├─→ Write to "APCapabilities.json"
                └─→ LOG: "Generated APCapabilities.json: {} items, {} locations, {} regions",
                          item_count, location_count, region_count
    │
    ↓
21. Broadcast registration complete
    │
    └─→ IPCServer::broadcast_message({
            type: "registration_complete",
            mod_id: "",
            data_json: "{}"
        })
        │
        └─→ For each registered mod:
            │
            └─→ Send "registration_complete" message
                │
                └─→ LOG: "Sent registration_complete to {}", mod_id
    │
    ↓
22. Mods receive notification
    │
    └─→ Mods know they can now:
        ├─→ Request AP connection
        ├─→ Send location checks
        └─→ Expect item deliveries
    │
    ↓
23. State: WAIT_REG → CONNECT
    │
    └─→ LOG: "Transitioning to CONNECT state"
```

**State**: All mods registered, capabilities generated, ready to connect to AP.

### Phase 5: AP Connection

**Trigger**: Autoconnect enabled OR manual connection request

```
24. Connection trigger
    │
    ├─→ Case A: Autoconnect enabled
    │   │
    │   └─→ Config: autoconnect = true
    │       │
    │       └─→ Framework automatically initiates connection
    │
    └─→ Case B: Manual connection request
        │
        └─→ Mod sends IPC message: {type: "connect", ...}
            │
            └─→ Framework processes connection request
    │
    ↓
25. Connect to AP
    │
    └─→ framework:connect_ap(server, port, slot, password)
        │
        └─→ handle:connect_ap("archipelago.gg", 38281, "Player1", "")
            │
            └─→ APClientWrapper::connect(server, port, slot, password)
                │
                ├─→ Create URI: "ws://archipelago.gg:38281"
                ├─→ Create new APClient instance
                │   │
                │   └─→ client = std::make_unique<::APClient>(uuid, game, uri)
                │
                ├─→ Set callbacks
                │   │
                │   ├─→ OnSlotConnected → queue_message(SlotConnected)
                │   ├─→ OnItemReceived → queue_message(ItemReceived)
                │   ├─→ OnLocationChecked → queue_message(LocationChecked)
                │   ├─→ OnDisconnected → queue_message(Disconnected)
                │   ├─→ OnRoomInfo → queue_message(RoomInfo)
                │   └─→ OnDataPackage → queue_message(DataPackage)
                │
                ├─→ client->connect()
                │   │
                │   ├─→ WebSocket connection initiated
                │   ├─→ Send Connect packet to AP server
                │   ├─→ Authenticate with slot_name and password
                │   └─→ Wait for Connected response
                │
                └─→ LOG: "Connected to AP: {}:{} as {}", server, port, slot
    │
    ↓
26. Connection established
    │
    ├─→ OnSlotConnected callback fires
    ├─→ APMessage(SlotConnected) queued
    └─→ LOG: "Slot connected successfully"
    │
    ↓
27. Broadcast connection status to mods
    │
    └─→ IPCServer::broadcast_message({
            type: "connection_status",
            mod_id: "",
            data_json: '{"status": "connected", "slot": "Player1"}'
        })
        │
        └─→ All mods receive connection_status
            │
            └─→ LOG: "Sent connection_status to all mods"
    │
    ↓
28. Start polling thread
    │
    └─→ framework:start_polling()
        │
        └─→ handle:start_polling()
            │
            └─→ PollingThread::start()
                │
                ├─→ Spawn background thread
                │   │
                │   └─→ polling_loop() begins
                │       │
                │       └─→ Continuous polling (see Phase 6)
                │
                └─→ LOG: "Polling thread started (interval: 16ms)"
    │
    ↓
29. State: CONNECT → RUNNING
    │
    └─→ LOG: "Framework fully operational"
```

**State**: Connected to AP, polling thread running, ready for runtime operation.

### Phase 6: Runtime Operation

**Trigger**: Polling thread running continuously

```
30. Polling loop (background thread)
    │
    └─→ PollingThread::polling_loop()
        │
        └─→ while (running_):
            │
            ├─→ Step 1: Poll AP client
            │   │
            │   └─→ ap_client_->poll()
            │       │
            │       ├─→ Process WebSocket messages
            │       ├─→ Fire callbacks (OnItemReceived, etc.)
            │       └─→ Queue APMessage objects
            │
            ├─→ Step 2: Retrieve messages
            │   │
            │   └─→ messages = ap_client_->get_messages()
            │       │
            │       └─→ Thread-safe dequeue from message_queue_
            │
            ├─→ Step 3: Route each message
            │   │
            │   └─→ For each msg in messages:
            │       │
            │       └─→ message_router_->route_ap_message(msg)
            │
            └─→ Step 4: Sleep
                │
                └─→ std::this_thread::sleep_for(16ms)
    │
    ↓
31. Message routing
    │
    └─→ MessageRouter::route_ap_message(msg)
        │
        ├─→ Case: msg.type == ItemReceived
        │   │
        │   ├─→ Look up: mod_id = item_to_mod_[msg.item_id]
        │   ├─→ If found:
        │   │   │
        │   │   └─→ IPCServer::send_to_mod(mod_id, {
        │   │           type: "item_received",
        │   │           mod_id: mod_id,
        │   │           data_json: '{"item_id": 100042, "player": 1, ...}'
        │   │       })
        │   │       │
        │   │       └─→ LOG: "Routed ItemReceived(100042) to {}", mod_id
        │   │
        │   └─→ Else:
        │       │
        │       └─→ LOG_WARNING: "No mod registered for item_id: {}", msg.item_id
        │
        ├─→ Case: msg.type == LocationChecked
        │   │
        │   ├─→ Look up: mod_id = location_to_mod_[msg.location_id]
        │   └─→ (Similar to ItemReceived)
        │
        └─→ Case: msg.type == SlotConnected/Disconnected/RoomInfo/DataPackage
            │
            └─→ Broadcast to all mods
                │
                └─→ IPCServer::broadcast_message({
                        type: "slot_connected",  // or other type
                        mod_id: "",
                        data_json: '{ ... }'
                    })
                    │
                    └─→ LOG: "Broadcast {} to all mods", msg.type
    │
    ↓
32. Mod receives item
    │
    └─→ Mod's IPC client reads message from pipe
        │
        ├─→ Parse: {type: "item_received", data_json: {...}}
        ├─→ Extract: item_id, item_name, player_slot
        ├─→ Process: Grant item to player in game
        └─→ LOG: "Received item: {} (id: {})", item_name, item_id
    │
    ↓
33. Mod checks location
    │
    └─→ Mod detects: Player opened chest, defeated boss, etc.
        │
        └─→ Mod sends IPC message:
            │
            └─→ {
                    type: "location_check",
                    mod_id: "PalworldMod",
                    data_json: '{"location_id": 200042}'
                }
    │
    ↓
34. Framework processes location check
    │
    └─→ FrameworkCore receives IPC message
        │
        └─→ handle_location_check(msg)
            │
            └─→ APClientWrapper::check_location(location_id)
                │
                ├─→ client->LocationChecks([location_id])
                ├─→ Send to AP server via WebSocket
                └─→ LOG: "Sent location check: {}", location_id
    │
    ↓
35. AP server processes check
    │
    ├─→ Server validates location
    ├─→ Server sends LocationChecked confirmation
    └─→ Server may send ItemReceived to other players
    │
    ↓
36. Framework receives confirmation
    │
    └─→ OnLocationChecked callback fires
        │
        └─→ APMessage(LocationChecked, location_id) queued
            │
            └─→ Routed back to mod (Step 31)
                │
                └─→ Mod receives confirmation
                    │
                    └─→ Mod updates UI: "Location checked!"
    │
    ↓
37. Loop continues indefinitely
    │
    └─→ Steps 30-36 repeat until shutdown
```

**State**: Runtime operation - continuous polling, routing, and message processing.

### Phase 7: Shutdown

**Trigger**: Game closes or user requests shutdown

```
38. Game closes
    │
    ↓
39. Shutdown hook triggered
    │
    └─→ RegisterHook("ReceiveShutdown") callback fires
        │
        └─→ on_shutdown()
            │
            └─→ LOG: "Shutdown initiated"
    │
    ↓
40. Stop polling
    │
    └─→ framework:stop_polling()
        │
        └─→ PollingThread::stop()
            │
            ├─→ Set running_ = false
            ├─→ Join polling thread
            └─→ LOG: "Polling thread stopped"
    │
    ↓
41. Disconnect from AP
    │
    └─→ framework:disconnect_ap()
        │
        └─→ APClientWrapper::disconnect()
            │
            ├─→ Send Disconnect packet to AP server
            ├─→ Close WebSocket connection
            ├─→ Reset client instance
            └─→ LOG: "Disconnected from AP"
    │
    ↓
42. Stop IPC server
    │
    └─→ framework:stop_ipc()
        │
        └─→ IPCServer::stop()
            │
            ├─→ Set running_ = false
            ├─→ Close named pipe
            ├─→ Join server thread
            └─→ LOG: "IPC server stopped"
    │
    ↓
43. Cleanup
    │
    └─→ framework = nil (Lua garbage collection)
        │
        └─→ __gc metamethod fires
            │
            └─→ delete FrameworkCore
                │
                ├─→ Delete all components
                ├─→ Free resources
                └─→ LOG: "FrameworkCore destroyed"
    │
    ↓
44. Close logger
    │
    └─→ Logger::instance().close()
        │
        ├─→ Flush remaining logs
        ├─→ Close log file
        └─→ Final entry: "Framework shutdown complete"
```

**State**: Framework fully shut down, all resources released.

---

## Thread Model

APFramework uses three concurrent threads to ensure non-blocking operation:

### Thread 1: Main Thread (Lua)

**Purpose**: Runs the UE4SS mod and Lua state machine.

**Responsibilities**:
- Execute Lua scripts (main.lua, framework_wrapper.lua, config.lua)
- Drive state machine via Tick events
- Handle user input and UI
- Call C++ framework methods via Lua bindings

**Frequency**: Every game frame (~60fps)

**Blocking**: Must not block; all C++ calls are non-blocking

**Communication**:
- **To C++**: Direct Lua C API function calls
- **From C++**: Poll-based state queries (`all_mods_registered()`, etc.)

### Thread 2: IPC Server Thread

**Purpose**: Handle IPC connections and message passing with mods.

**Responsibilities**:
- Accept connections on named pipe
- Read messages from connected mods
- Write messages to mod-specific queues
- Maintain per-mod connection state

**Lifecycle**:
- **Start**: `IPCServer::start()` spawns thread
- **Run**: Continuous loop accepting connections and reading messages
- **Stop**: `IPCServer::stop()` signals shutdown and joins thread

**Thread Function**:
```cpp
void IPCServer::ipc_server_thread() {
    while (running_) {
        // 1. Accept new connections
        HANDLE pipe = accept_connection();

        // 2. Read messages from connected mods
        for (auto& [mod_id, connection] : connections_) {
            if (message_available(connection)) {
                IPCMessage msg = read_message(connection);
                push_to_main_queue(msg);
            }
        }

        // 3. Write queued messages to mods
        for (auto& [mod_id, queue] : mod_queues_) {
            while (!queue.empty()) {
                IPCMessage msg = queue.pop();
                write_message(connections_[mod_id], msg);
            }
        }
    }
}
```

**Thread Safety**:
- `mod_queues_`: Per-mod `MessageQueue<IPCMessage>` (mutex-protected)
- `connections_`: Mutex-protected map of mod connections
- `main_queue_`: Thread-safe queue for messages to main thread

### Thread 3: Polling Thread

**Purpose**: Continuously poll the AP server for incoming messages.

**Responsibilities**:
- Call `ap_client_->poll()` at regular intervals
- Retrieve queued AP messages
- Route messages via `message_router_`

**Lifecycle**:
- **Start**: `PollingThread::start()` spawns thread
- **Run**: Continuous loop polling at 16ms intervals (~60fps)
- **Stop**: `PollingThread::stop()` signals shutdown and joins thread

**Thread Function**:
```cpp
void PollingThread::polling_loop() {
    while (running_) {
        // 1. Poll AP client (processes WebSocket messages)
        ap_client_->poll();

        // 2. Retrieve queued messages
        auto messages = ap_client_->get_messages();

        // 3. Route each message
        for (const auto& msg : messages) {
            message_router_->route_ap_message(msg);
        }

        // 4. Sleep until next poll
        std::this_thread::sleep_for(
            std::chrono::milliseconds(polling_interval_ms_)
        );
    }
}
```

**Thread Safety**:
- `ap_client_->message_queue_`: `MessageQueue<APMessage>` (mutex-protected)
- `message_router_`: Thread-safe routing (reads from immutable tables)
- `ipc_server_->mod_queues_`: Thread-safe queues for outgoing messages

### Thread Synchronization

**Data Structures**:

```cpp
template <typename T>
class MessageQueue {
public:
    void push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
        cv_.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty(); });
        T item = queue_.front();
        queue_.pop();
        return item;
    }

    std::vector<T> pop_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<T> items;
        while (!queue_.empty()) {
            items.push_back(queue_.front());
            queue_.pop();
        }
        return items;
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};
```

**Thread Communication**:

```
Main Thread (Lua)
    ↓ (function calls)
FrameworkCore
    ↓ (method calls)
IPC Server Thread ←→ MessageQueue ←→ Polling Thread
    ↓                                      ↓
Named Pipe                            WebSocket
    ↓                                      ↓
Mods                                  AP Server
```

**Synchronization Points**:

1. **Main → IPC Server**:
   - Main calls `send_to_mod()` → pushes to mod_queue (mutex)
   - IPC thread pops from mod_queue and writes to pipe

2. **IPC Server → Main**:
   - IPC thread reads from pipe → pushes to main_queue (mutex)
   - Main polls main_queue for mod messages

3. **Polling → Message Router**:
   - Polling thread pops from ap_client message_queue (mutex)
   - Calls message_router (reads immutable routing tables)

4. **Message Router → IPC Server**:
   - Router calls ipc_server->send_to_mod() → pushes to mod_queue (mutex)
   - IPC thread pops and sends to mod

**Lock Hierarchy**: No nested locks → no deadlock potential.

---

## Message Flow Diagrams

### Mod Registration Flow

```
Mod                 IPC Server        FrameworkCore    ModRegistry    CapGen      MsgRouter
 │                       │                  │              │            │            │
 │ Connect to pipe       │                  │              │            │            │
 ├──────────────────────>│                  │              │            │            │
 │                       │                  │              │            │            │
 │ Send "register"       │                  │              │            │            │
 ├──────────────────────>│                  │              │            │            │
 │                       │                  │              │            │            │
 │                       │ Push to queue    │              │            │            │
 │                       ├─────────────────>│              │            │            │
 │                       │                  │              │            │            │
 │                       │                  │ Register mod │            │            │
 │                       │                  ├─────────────>│            │            │
 │                       │                  │              │            │            │
 │                       │                  │              │ Add caps   │            │
 │                       │                  │              ├───────────>│            │
 │                       │                  │              │            │            │
 │                       │                  │              │            │ Update     │
 │                       │                  │              │            │ routing    │
 │                       │                  │              │            │ tables     │
 │                       │                  ├──────────────┴────────────┴───────────>│
 │                       │                  │                                        │
 │                       │                  │ All registered?                        │
 │                       │                  │<──────────────┐                        │
 │                       │                  │               │                        │
 │                       │                  │ Yes: Generate caps                     │
 │                       │                  ├───────────────────────────>│            │
 │                       │                  │                            │            │
 │                       │                  │              Write APCapabilities.json │
 │                       │                  │<───────────────────────────┤            │
 │                       │                  │                            │            │
 │                       │ Broadcast        │                            │            │
 │                       │ "reg_complete"   │                            │            │
 │                       │<─────────────────┤                            │            │
 │                       │                  │                            │            │
 │ Receive "reg_complete"│                  │                            │            │
 │<──────────────────────┤                  │                            │            │
 │                       │                  │                            │            │
```

### Item Received Flow

```
AP Server       APClient    PollingThread   MsgRouter    IPCServer    Mod
    │               │             │             │            │          │
    │ ItemReceived  │             │             │            │          │
    │ (WebSocket)   │             │             │            │          │
    ├──────────────>│             │             │            │          │
    │               │             │             │            │          │
    │               │ Poll()      │             │            │          │
    │               │<────────────┤             │            │          │
    │               │             │             │            │          │
    │               │ Callback    │             │            │          │
    │               │ fires       │             │            │          │
    │               ├─────────┐   │             │            │          │
    │               │         │   │             │            │          │
    │               │ Queue   │   │             │            │          │
    │               │ APMsg   │   │             │            │          │
    │               │<────────┘   │             │            │          │
    │               │             │             │            │          │
    │               │ get_msgs()  │             │            │          │
    │               │<────────────┤             │            │          │
    │               │             │             │            │          │
    │               │ [APMsg]     │             │            │          │
    │               ├────────────>│             │            │          │
    │               │             │             │            │          │
    │               │             │ Route       │            │          │
    │               │             ├────────────>│            │          │
    │               │             │             │            │          │
    │               │             │             │ Lookup     │          │
    │               │             │             │ item→mod   │          │
    │               │             │             ├────┐       │          │
    │               │             │             │    │       │          │
    │               │             │             │<───┘       │          │
    │               │             │             │            │          │
    │               │             │             │ Send to    │          │
    │               │             │             │ mod        │          │
    │               │             │             ├───────────>│          │
    │               │             │             │            │          │
    │               │             │             │            │ IPC msg  │
    │               │             │             │            ├─────────>│
    │               │             │             │            │          │
    │               │             │             │            │          │ Process
    │               │             │             │            │          │ item
    │               │             │             │            │          ├────┐
    │               │             │             │            │          │    │
    │               │             │             │            │          │<───┘
```

### Location Check Flow

```
Mod        IPCServer    FrameworkCore    APClient    AP Server
 │              │              │             │            │
 │ Player       │              │             │            │
 │ triggers     │              │             │            │
 │ location     │              │             │            │
 ├────┐         │              │             │            │
 │    │         │              │             │            │
 │<───┘         │              │             │            │
 │              │              │             │            │
 │ Send "location_check"       │             │            │
 ├─────────────>│              │             │            │
 │              │              │             │            │
 │              │ Push to queue│             │            │
 │              ├─────────────>│             │            │
 │              │              │             │            │
 │              │              │ check_loc() │            │
 │              │              ├────────────>│            │
 │              │              │             │            │
 │              │              │             │ LocationChecks
 │              │              │             │ (WebSocket)│
 │              │              │             ├───────────>│
 │              │              │             │            │
 │              │              │             │            │ Validate
 │              │              │             │            ├────┐
 │              │              │             │            │    │
 │              │              │             │            │<───┘
 │              │              │             │            │
 │              │              │             │ LocChecked│
 │              │              │             │ response   │
 │              │              │             │<───────────┤
 │              │              │             │            │
 │              │              │             │ Callback   │
 │              │              │             │ fires      │
 │              │              │             ├────┐       │
 │              │              │             │    │       │
 │              │              │             │<───┘       │
 │              │              │             │            │
 │              │              │             │ Queue msg  │
 │              │              │             ├────┐       │
 │              │              │             │    │       │
 │              │              │             │<───┘       │
 │              │              │             │            │
 │              │             [Polling thread routes back to mod via MsgRouter]
 │              │              │             │            │
 │ Receive "location_checked"  │             │            │
 │<─────────────┤              │             │            │
 │              │              │             │            │
 │ Update UI    │              │             │            │
 ├────┐         │              │             │            │
 │    │         │              │             │            │
 │<───┘         │              │             │            │
```

---

## Configuration System

### Configuration File Structure

**File**: [APFramework/framework_config.json](../../APFramework/framework_config.json)

```json
{
  "server": "archipelago.gg",
  "port": 38281,
  "slot_name": "Player1",
  "password": "",
  "autoconnect": false,
  "registration_timeout": 180
}
```

### Configuration Loading Flow

```
framework_config.json
         │
         ↓ (read file)
   config.lua
         │
         ↓ (parse with regex)
   Config table
    {
      server: "archipelago.gg",
      port: 38281,
      slot_name: "Player1",
      password: "",
      autoconnect: false,
      registration_timeout: 180
    }
         │
         ↓ (pass to C++)
   ConfigManager::load_from_file()
         │
         ↓ (parse JSON)
   FrameworkConfig struct
    {
      active_profile: {
        name: "default",
        server: "archipelago.gg",
        port: 38281,
        slot_name: "Player1",
        password: "",
        autoconnect: false
      },
      polling_interval_ms: 16,
      enable_logging: true,
      log_level: "INFO"
    }
         │
         ↓ (used by)
   FrameworkCore
    ├─→ APClientWrapper (server, port, slot, password)
    ├─→ PollingThread (polling_interval_ms)
    └─→ Logger (enable_logging, log_level)
```

### Mod Configuration Structure

**File**: `UE4SS/Mods/ModName/ap_config.json`

```json
{
  "mod_id": "PalworldMod",
  "items": [
    {
      "id": 100000,
      "name": "Super Pickaxe",
      "classification": "useful"
    },
    {
      "id": 100001,
      "name": "Fast Boots",
      "classification": "progression"
    }
  ],
  "locations": [
    {
      "id": 200000,
      "name": "Chest in Cave",
      "region": "Starting Area"
    },
    {
      "id": 200001,
      "name": "Boss Reward",
      "region": "Mountain Path"
    }
  ],
  "regions": [
    {
      "name": "Starting Area",
      "connects_to": ["Mountain Path"]
    },
    {
      "name": "Mountain Path",
      "connects_to": ["Starting Area", "Peak"]
    }
  ]
}
```

### Generated Capabilities Structure

**File**: `APCapabilities.json` (generated at runtime)

```json
{
  "items": [
    {
      "id": 100000,
      "name": "Super Pickaxe",
      "classification": "useful",
      "mod_id": "PalworldMod"
    },
    {
      "id": 110000,
      "name": "Magic Sword",
      "classification": "progression",
      "mod_id": "AnotherMod"
    }
  ],
  "locations": [
    {
      "id": 200000,
      "name": "Chest in Cave",
      "region": "Starting Area",
      "mod_id": "PalworldMod"
    },
    {
      "id": 210000,
      "name": "Secret Room",
      "region": "Castle",
      "mod_id": "AnotherMod"
    }
  ],
  "regions": [
    {
      "name": "Starting Area",
      "connects_to": ["Mountain Path"],
      "locations": [200000],
      "mod_id": "PalworldMod"
    },
    {
      "name": "Castle",
      "connects_to": ["Starting Area"],
      "locations": [210000],
      "mod_id": "AnotherMod"
    }
  ]
}
```

**Generation Logic**:
1. Iterate through all registered mods
2. Merge items from all mods into single array
3. Merge locations from all mods into single array
4. Merge regions from all mods into single array
5. Validate no ID collisions (items, locations must have unique IDs)
6. Write to JSON file

---

## State Machine

### States

The framework lifecycle is driven by a state machine implemented in [main.lua](../../APFramework/Scripts/main.lua:60):

```lua
States:
  INIT          -- Initial state, framework created
  START_IPC     -- Starting IPC server
  DISCOVER      -- Discovering mods
  WAIT_REG      -- Waiting for mod registration
  CONNECT       -- Connecting to AP server
  RUNNING       -- Fully operational
```

### State Transitions

```
┌──────┐
│ INIT │
└───┬──┘
    │ (first tick)
    ↓
┌────────────┐
│ START_IPC  │─────┐ start_ipc() fails
└─────┬──────┘     │
      │            ↓
      │ (success) [ERROR STATE]
      ↓
┌──────────┐
│ DISCOVER │──────┐ discover_mods() fails
└────┬─────┘      │
     │            ↓
     │ (success) [ERROR STATE]
     ↓
┌──────────┐
│ WAIT_REG │──────┐ timeout (180s default)
└────┬─────┘      │
     │            ↓
     │ (all regs) [TIMEOUT ERROR]
     │            (can retry or abort)
     ↓
┌─────────┐
│ CONNECT │───────┐ autoconnect=false
└────┬────┘       │ (wait for manual)
     │            │
     │            ↓
     │         [WAITING]
     │  (manual connect request)
     │            │
     │<───────────┘
     │
     │ (autoconnect=true OR manual)
     ↓
┌─────────┐
│ RUNNING │
└────┬────┘
     │
     │ (continuous polling)
     │
     ↓
   [LOOP]
     ↑
     │
     └─ (until shutdown)
```

### State Details

#### INIT

**Entry Actions**:
- Load configuration
- Initialize logger
- Create framework instance

**Tick Actions**:
- None (immediate transition)

**Exit Condition**:
- Immediate (first tick)

**Next State**: `START_IPC`

#### START_IPC

**Entry Actions**:
- Call `framework:start_ipc()`
- Start IPC server thread
- Create named pipe

**Tick Actions**:
- Check if IPC server started successfully

**Exit Condition**:
- IPC server running

**Next State**: `DISCOVER`

**Error Handling**:
- If start_ipc() fails: Log error, remain in state, retry next tick

#### DISCOVER

**Entry Actions**:
- Call `framework:discover_mods(MODS_DIRECTORY)`
- Scan for ap_config.json files
- Add discovered mods to registry

**Tick Actions**:
- Check if discovery complete

**Exit Condition**:
- All mods discovered

**Next State**: `WAIT_REG`

**Error Handling**:
- If no mods found: Log warning, skip to CONNECT state
- If discovery fails: Log error, retry next tick

#### WAIT_REG

**Entry Actions**:
- Start registration timer
- Log: "Waiting for X mods to register"

**Tick Actions**:
- Call `framework:all_mods_registered()`
- Check registration timer
- Log progress every 5 seconds

**Exit Condition**:
- All discovered mods registered: Generate capabilities, broadcast, go to CONNECT
- Timeout exceeded: Log error, go to CONNECT anyway (partial registration)

**Next State**: `CONNECT`

**Timeout Handling**:
```lua
if time_in_state > registration_timeout then
    LOG_WARNING("Registration timeout! Only {}/{} mods registered",
                registered_count, discovered_count)
    -- Generate capabilities with partial registration
    framework:generate_capabilities()
    -- Continue to CONNECT
    state = "CONNECT"
end
```

#### CONNECT

**Entry Actions**:
- Check autoconnect setting
- If autoconnect: Call `framework:connect_ap()`
- If not autoconnect: Wait for manual request

**Tick Actions**:
- If autoconnect: Check if connected
- If manual mode: Poll for IPC "connect" message

**Exit Condition**:
- Connected to AP server
- Polling thread started

**Next State**: `RUNNING`

**Error Handling**:
- Connection failed: Log error, remain in state, allow retry

#### RUNNING

**Entry Actions**:
- Log: "Framework fully operational"
- Start polling thread

**Tick Actions**:
- Process IPC messages from mods
- Monitor connection status
- Handle mod requests (location checks, status updates)

**Exit Condition**:
- None (runs until shutdown)

**Next State**: N/A (terminal state)

**Error Handling**:
- If disconnected: Log warning, attempt reconnect
- If critical error: Log error, transition to ERROR state

### State Machine Driver

**Tick Handler**:
```lua
local tick_counter = 0
local UPDATE_INTERVAL = 60  -- Update every 60 ticks (~1 second @ 60fps)

function on_tick()
    tick_counter = tick_counter + 1

    if tick_counter >= UPDATE_INTERVAL then
        tick_counter = 0
        update_state_machine()
    end
end

function update_state_machine()
    if state == "INIT" then
        handle_init_state()
    elseif state == "START_IPC" then
        handle_start_ipc_state()
    elseif state == "DISCOVER" then
        handle_discover_state()
    elseif state == "WAIT_REG" then
        handle_wait_reg_state()
    elseif state == "CONNECT" then
        handle_connect_state()
    elseif state == "RUNNING" then
        handle_running_state()
    end
end
```

---

## Summary

APFramework is a production-ready IPC-based Archipelago integration framework with:

✅ **Clean separation of concerns** - C++ handles complex protocol and threading, Lua handles UE4SS integration

✅ **Non-blocking architecture** - Three concurrent threads (main, IPC server, polling) ensure smooth operation

✅ **Promise-based registration** - Framework discovers mods before they register, preventing race conditions

✅ **Dynamic capabilities** - APCapabilities.json generated at runtime from mod data

✅ **Native Lua integration** - Proper Lua C API bindings with automatic memory management

✅ **Robust state machine** - Comprehensive lifecycle management with error handling and timeouts

✅ **Thread-safe design** - Mutex-protected message queues and careful synchronization

✅ **Comprehensive logging** - Detailed logging at every stage for debugging and monitoring

The framework is fully implemented and ready for deployment and testing with real game mods.