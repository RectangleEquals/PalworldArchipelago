# APFramework IPC Architecture

**Version**: 2.0.0 (IPC Branch)
**Last Updated**: December 31, 2024
**Status**: Design Phase

---

## Table of Contents
1. [Overview](#overview)
2. [Architecture Goals](#architecture-goals)
3. [System Components](#system-components)
4. [Communication Flow](#communication-flow)
5. [Message Protocol](#message-protocol)
6. [Mod Integration](#mod-integration)
7. [Threading Model](#threading-model)

---

## Overview

This document describes the **IPC-based architecture** for the Palworld Archipelago Framework. This is a complete redesign from the submodule architecture to support:

- **Standard UE4SS mods** (not forced submods)
- **Non-blocking operation** (no thread blocking during initialization)
- **Multiple mod types** (Lua, C++, BP Logic mods)
- **Inter-process communication** between framework and mods

### Key Innovation

The framework becomes a **service/daemon** that AP-enabled mods communicate with via IPC (Named Pipes), rather than a monolithic system that mods must be embedded within.

---

## Architecture Goals

### Problems Solved

1. **Submod Limitation** ✅
   - Mods are standard UE4SS mods in normal locations
   - No special integration required
   - Works with existing mod ecosystem

2. **Blocking Thread Issue** ✅
   - Each mod runs independently
   - Framework doesn't block other mods during initialization
   - No "subsequent mods can't load" problem

3. **Lua State Issues** ✅
   - No Lua state crossing needed
   - IPC provides clean boundary
   - Each mod has its own isolated environment

4. **Flexibility** ✅
   - Supports Lua mods, C++ mods, AND BP Logic mods
   - Same communication protocol for all mod types
   - Framework handles complexity internally

### Design Principles

- **Separation of Concerns**: Framework handles AP protocol, mods handle game integration
- **Loose Coupling**: Mods communicate via well-defined IPC protocol
- **Fault Isolation**: One mod's failure doesn't affect others
- **Developer-Friendly**: Simple API for mod developers

---

## System Components

### 1. APFramework (Core Framework)

**Type**: UE4SS Lua Mod + C++ Library
**Location**: `ue4ss/mods/APFramework/`

**Components**:
```
APFramework/
├── Scripts/
│   ├── main.lua              # Minimal Lua entry point (loads C++ core)
│   ├── APFramework.lua       # Minimal Lua wrapper
│   └── lib/
│       └── APFrameworkCore.dll    # C++ core (everything)
└── config.json               # Framework configuration
```

**Responsibilities**:
- **Lua Mod** (minimal, ~20 lines):
  - Load `APFrameworkCore.dll` via FFI
  - Pass configuration to C++ core
  - That's it! Everything else is C++

**C++ Core** (`APFrameworkCore.dll`):
- Integrate **apclientpp** (C++ library, not lua-apclientpp)
- Run background polling thread
- Host Named Pipes IPC server
- Auto-discover AP-enabled mods (`ap_config.json` scanning)
- Maintain per-mod message queues
- Route AP messages to appropriate mods
- **Pure C++** - no Lua dependencies

### 2. APClientLib (Mod Client Library)

**Type**: C++ Library (for C++ mods)
**Location**: Distributed with framework, placed in mod's `dlls/` folder

**Components**:
```
APClientLib.dll
├── IPC client (Named Pipes)
├── Message queue polling
├── C API for UE4SS C++ mods
└── Thread-safe operations
```

**Responsibilities**:
- Connect to framework's IPC server
- Send messages (location checks, status updates)
- Poll incoming messages (items received, etc.)
- Provide simple C API for mod developers

### 3. ap_client.lua (Lua Client Wrapper)

**Type**: Pure Lua Module
**Location**: Distributed with framework, placed in mod's `Scripts/lib/`

**Responsibilities**:
- Named Pipes IPC wrapper (pure Lua)
- Message serialization/deserialization (JSON)
- Event callback system
- Polling helper functions
- Provide simple Lua API for mod developers

### 4. AP-Enabled Mods

**Types**: Lua Mod, C++ Mod, or BP Logic Mod (with Lua companion)

**Requirements**:
- `ap_config.json` file (capabilities declaration)
- Integration with client library (Lua or C++)
- Registration with framework via IPC
- Message polling in mod's update loop

---

## Communication Flow

### 1. Startup Sequence

```
1. UE4SS starts loading mods (order from mods.txt)
   ↓
2. APFramework loads FIRST (enforced by load order)
   ├─ Loads APFrameworkCore.dll via FFI
   ├─ Starts IPC server (Named Pipes)
   ├─ Scans for ap_config.json files (auto-discovery)
   └─ Connects to AP server
   ↓
3. Other mods load in normal UE4SS order
   ↓
4. AP-enabled mods initialize
   ├─ Load client library (ap_client.lua or APClientLib.dll)
   ├─ Connect to framework via IPC
   ├─ Register event handlers
   └─ Start polling message queue
   ↓
5. Framework sends "ready" message to all connected mods
   ↓
6. Normal gameplay begins
```

### 2. Runtime Message Flow

**Item Received Example**:
```
Archipelago Server
    ↓ WebSocket
APFrameworkCore.dll (polling thread)
    ├─ Receives item from AP server
    ├─ Routes to target mod based on item ID
    └─ Enqueues message in mod's queue
    ↓ Named Pipe
Mod polls message queue
    ├─ Receives item_received message
    └─ Grants item to player (UE4 calls in mod's context)
```

**Location Check Example**:
```
Mod detects location (e.g., chest opened)
    ↓ Named Pipe
APFrameworkCore.dll
    ├─ Receives location_check message
    └─ Sends to AP server via WebSocket
    ↓ WebSocket
Archipelago Server
    └─ Broadcasts to all players
```

### 3. IPC Server Architecture

**Named Pipe Design**:
```
Pipe Name: \\.\pipe\APFramework_<instance_id>
Direction: Bidirectional
Protocol: JSON messages (newline-delimited)
Async: Non-blocking reads/writes
Per-Mod Queues: Thread-safe message queues
```

**Thread Model**:
```
APFrameworkCore.dll

Main Thread:
  └─ IPC Server (listens for mod connections)
      ├─ Accept new connections
      ├─ Read incoming messages from mods
      └─ Dispatch to AP polling thread

Polling Thread:
  └─ Continuous AP server polling
      ├─ Poll lua-apclientpp every 16ms
      ├─ Process incoming AP messages
      ├─ Route to mod-specific queues
      └─ Write outgoing messages to mods via Named Pipes
```

---

## Message Protocol

### JSON Message Format

All IPC messages use JSON with this structure:

```json
{
  "type": "message_type",
  "mod_id": "sender_mod_id",
  "data": { /* message-specific data */ }
}
```

### Message Types

#### Mod → Framework

**1. Register**
```json
{
  "type": "register",
  "mod_id": "mymod",
  "data": {
    "capabilities": {
      "items": [...],
      "locations": [...],
      "regions": [...]
    }
  }
}
```

**2. Location Check**
```json
{
  "type": "location_check",
  "mod_id": "mymod",
  "data": {
    "location_id": 12345
  }
}
```

**3. Status Update**
```json
{
  "type": "status_update",
  "mod_id": "mymod",
  "data": {
    "status": "InGame"
  }
}
```

**4. Poll Request**
```json
{
  "type": "poll",
  "mod_id": "mymod",
  "data": {}
}
```
**Response**: Array of queued messages for this mod

#### Framework → Mod

**1. Item Received**
```json
{
  "type": "item_received",
  "data": {
    "item": 8370050,
    "location": 12345,
    "player": 1,
    "flags": 0,
    "index": 42
  }
}
```

**2. Location Checked**
```json
{
  "type": "location_checked",
  "data": {
    "location_id": 12345
  }
}
```

**3. Connection Status**
```json
{
  "type": "connection_status",
  "data": {
    "connected": true,
    "state": 4,
    "slot_name": "Player1"
  }
}
```

**4. Framework Ready**
```json
{
  "type": "framework_ready",
  "data": {
    "version": "2.0.0"
  }
}
```

**5. Error**
```json
{
  "type": "error",
  "data": {
    "code": "INVALID_MESSAGE",
    "message": "Error description"
  }
}
```

---

## Mod Integration

### Lua Mod Integration

**Directory Structure**:
```
ue4ss/mods/MyLuaMod/
├── Scripts/
│   ├── main.lua              # Mod entry point
│   ├── lib/
│   │   └── ap_client.lua     # IPC wrapper (provided by framework)
│   └── my_mod_logic.lua      # Mod-specific code
└── ap_config.json            # Capabilities
```

**Example Integration**:
```lua
-- MyLuaMod/Scripts/main.lua
local APClient = require("lib.ap_client")

-- Connect to framework
local success = APClient:Connect("myluamod")
if not success then
    print("[MyMod] APFramework not available - AP features disabled")
    return
end

-- Register event handlers
APClient:OnItemReceived(function(item_data)
    print("[MyMod] Received item: " .. item_data.item)
    -- Grant item to player (can use UE4 functions here)
    GrantItemToPlayer(item_data.item)
end)

-- Poll messages in tick/update
RegisterCustomEvent("Tick", function(deltaTime)
    APClient:ProcessMessages() -- Polls IPC and fires callbacks
end)

-- Send location check when player opens chest
RegisterHook("/Script/Pal.SomeChestClass:OnOpened", function(self)
    local location_id = GetLocationIDForChest(self)
    APClient:CheckLocation(location_id)
end)
```

### C++ Mod Integration

**Directory Structure**:
```
ue4ss/mods/MyCppMod/
├── dlls/
│   ├── main.dll              # C++ mod (UE4SS C++ mod)
│   └── APClientLib.dll       # IPC client library
└── ap_config.json            # Capabilities
```

**Example Integration**:
```cpp
// MyCppMod/main.cpp
#include <Mod/CppUserModBase.hpp>
#include <ap_client_lib.h>

class MyCppMod : public RC::CppUserModBase {
private:
    APClientLib* ap_client = nullptr;

public:
    void on_mod_load() override {
        // Connect to framework
        ap_client = ap_client_connect("mycppmod");
        if (!ap_client) {
            Output::send(STR("APFramework not available\n"));
            return;
        }

        Output::send(STR("Connected to APFramework\n"));
    }

    void on_update() override {
        // Poll messages from framework
        auto messages = ap_client_poll_messages(ap_client);

        for (auto& msg : messages) {
            if (msg.type == MessageType::ItemReceived) {
                grant_item(msg.item_id);
            }
        }
    }

    void on_chest_opened(int chest_id) {
        // Send location check
        int location_id = map_chest_to_location(chest_id);
        ap_client_check_location(ap_client, location_id);
    }
};
```

### BP Logic Mod Integration

**Strategy**: Companion Lua mod that bridges BP ↔ Lua ↔ IPC

**Directory Structure**:
```
Pal/Content/Paks/LogicMods/
└── MyBPMod.pak              # BP Logic Mod

ue4ss/mods/MyBPMod_Companion/
├── Scripts/
│   ├── main.lua             # Lua companion
│   └── lib/
│       └── ap_client.lua    # IPC wrapper
└── ap_config.json           # Capabilities
```

**Example Integration**:
```lua
-- MyBPMod_Companion/Scripts/main.lua
local APClient = require("lib.ap_client")
APClient:Connect("mybpmod")

local MyBPMod = nil

-- Wait for BP to initialize
RegisterCustomEvent("MyBPMod_InitLuaInterop", function(BP)
    MyBPMod = BP
    print("[MyBPMod] BP-Lua interop established")
end)

-- Poll for items and notify BP
RegisterCustomEvent("Tick", function(deltaTime)
    local messages = APClient:PollMessages()
    for _, msg in ipairs(messages) do
        if msg.type == "item_received" and MyBPMod then
            -- Call BP event
            MyBPMod:OnItemReceived(msg.data.item)
        end
    end
end)

-- BP calls Lua to check location
function CheckLocationFromBP(location_id)
    APClient:CheckLocation(location_id)
end
```

---

## Threading Model

### Framework Threading

**APFrameworkCore.dll** runs two threads:

1. **Main Thread** (IPC Server):
   - Listens for mod connections on Named Pipe
   - Accepts new connections
   - Reads messages from mods
   - Dispatches location checks to polling thread
   - Writes responses back to mods

2. **Polling Thread** (AP Client):
   - Continuously polls lua-apclientpp (every 16ms)
   - Processes incoming AP messages
   - Routes messages to mod-specific queues
   - Thread-safe queue operations

**Synchronization**:
- Per-mod message queues protected by mutexes
- Lock-free reading when possible (single consumer)
- Minimal contention (each mod has own queue)

### Mod Threading

**Mods control their own threading**:
- Lua mods: Single-threaded (UE4SS Lua context)
- C++ mods: Can be multi-threaded if desired
- IPC client library is thread-safe
- Mods poll at their own pace (no forced synchronization)

**Polling Patterns**:

**Option A: Tick/Update Polling**
```lua
RegisterCustomEvent("Tick", function(deltaTime)
    APClient:ProcessMessages()
end)
```

**Option B: Callback Registration** (internally polls)
```lua
APClient:OnItemReceived(function(item)
    -- Called when item message received
end)
-- ap_client.lua polls internally and fires callbacks
```

**Option C: Manual Polling**
```lua
local messages = APClient:PollMessages()
for _, msg in ipairs(messages) do
    -- Handle message
end
```

---

## Benefits of IPC Architecture

### For Framework Developers

- Clean separation of concerns
- No Lua state management issues
- Native threading for AP polling
- Easy to test components independently
- Can support future enhancements (HTTP API, web dashboard, etc.)

### For Mod Developers

- Simple, well-defined API
- No framework internals knowledge needed
- Standard UE4SS mod development
- Can use any UE4SS features (hooks, events, etc.)
- Freedom to choose Lua, C++, or BP

### For Users

- Compatible with existing mods
- No special installation for AP-enabled mods
- Better performance (no blocking)
- More stable (fault isolation)

---

## Future Enhancements

### Potential Extensions

1. **HTTP REST API**: Framework could expose HTTP endpoints for external tools
2. **Web Dashboard**: Monitor connected mods, view stats, configure settings
3. **Hot Reload**: Restart framework without restarting game
4. **Multi-Instance**: Support multiple game instances on same machine
5. **Remote Debugging**: Debug tools for mod developers

### Backward Compatibility

Not a goal for IPC branch. This is a clean-slate redesign. Users can stick with main branch if needed.

---

## Technical Constraints

### Platform Support

- **Windows Only** (initial release)
  - Named Pipes are Windows-specific
  - Could port to Unix domain sockets for Linux/Steam Deck later

### Dependencies

- UE4SS (latest version)
- lua-apclientpp v0.6.4+
- C++17 or later (for framework core)
- LuaJIT FFI (for framework Lua mod)

### Performance Considerations

- IPC overhead: ~1-5ms per message (acceptable for game context)
- Named Pipes: Very low latency on same machine
- Message polling: Mods control frequency (typically once per frame)
- Queue depth: Unbounded (relies on timely mod polling)

---

**This architecture provides a robust, scalable foundation for the Palworld Archipelago integration while maintaining compatibility with the broader UE4SS mod ecosystem.**
