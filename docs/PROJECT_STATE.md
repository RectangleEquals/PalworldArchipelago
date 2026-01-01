# Palworld Archipelago - Current State

**Last Updated**: December 31, 2024
**Status**: Phase 4 - Runtime Integration ✅ **CONNECTION CONFIG & POLLING COMPLETE**

## Table of Contents
1. [Project Overview](#project-overview)
2. [Architecture](#architecture)
3. [Current Progress](#current-progress)
4. [Phase 4: Runtime Integration](#phase-4-runtime-integration-current)
5. [Technical Details](#technical-details)
6. [Next Steps](#next-steps)

---

## Project Overview

### What is this project?
This project integrates Palworld into the [Archipelago](https://archipelago.gg) multiworld randomizer ecosystem. It consists of two main components:

1. **AP World** (Python) - Server-side logic for randomization (`worlds/palworld/`)
2. **APFramework** (Lua) - In-game client that connects to AP server (`APFramework/`)

### Goals
- Allow Palworld to participate in multiworld randomizer sessions
- Randomize key items, locations, and progression across Palworld regions
- Enable cross-game item sharing through Archipelago protocol
- Provide a flexible framework for AP-enabled Palworld mods

### Current Phase: Phase 4 - Runtime Integration
**Objective**: Implement runtime location checks, item receiving, and continuous polling for real-time AP events.

---

## Architecture

### High-Level Overview
```
┌─────────────────────────────────────────────────────────────┐
│ Palworld (Unreal Engine 4)                                  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ UE4SS (Lua Scripting System)                          │  │
│  │  ┌─────────────────────────────────────────────────┐  │  │
│  │  │ APFramework (Lua)                               │  │  │
│  │  │  - Centralized connection config                │  │  │
│  │  │  - Continuous polling (blocking loop)           │  │  │
│  │  │  - Mod discovery & registration                 │  │  │
│  │  │  - Capability manifest generation               │  │  │
│  │  │  - APClient (lua-apclientpp wrapper)            │  │  │
│  │  │    └─ WebSocket connection to AP server         │  │  │
│  │  │                                                   │  │  │
│  │  │  Submodules (e.g., APTest):                     │  │  │
│  │  │  - ap_config.json (capabilities only)           │  │  │
│  │  │  - Items, locations, regions for this mod       │  │  │
│  │  └─────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                         ↕ WebSocket (ws://)
┌─────────────────────────────────────────────────────────────┐
│ Archipelago Server (Python)                                 │
│  - Hosts multiworld session                                 │
│  - Manages item/location randomization                      │
│  - Routes items between players/games                       │
│  - Uses worlds/palworld/ for Palworld logic                 │
└─────────────────────────────────────────────────────────────┘
```

### Design Philosophy: Submodule Architecture

**Problem**: UE4SS mods run in isolated Lua sandboxes - they cannot share data via global namespace.

**Solution**: Instead of separate standalone mods, AP-enabled mods are **submodules** of APFramework:

```
APFramework/
├── config.json          # Framework-level configuration
├── config.example.json  # Template for users
├── Scripts/             # Core framework code
│   ├── main.lua         # UE4SS entry point
│   ├── APFramework.lua  # Core framework
│   ├── APClient.lua     # WebSocket client
│   ├── EventBus.lua     # Event system + frame callbacks
│   ├── ModRegistry.lua  # Mod discovery
│   ├── ConfigManager.lua # Config management
│   └── lib/
│       ├── lua-apclientpp.dll  # Native WebSocket library
│       └── lunajson/           # JSON parsing
└── Mods/                # Submodules (AP-enabled mods)
    └── APTest/
        ├── ap_config.json  # Mod metadata + capabilities
        └── main.lua        # Mod-specific logic (optional)
```

**Discovery Process**:
1. APFramework scans `APFramework/Mods/*/ap_config.json`
2. Parses each config for items, locations, regions
3. Generates `APCapabilities.json` manifest (used by Python AP World during generation)
4. Connects to AP server using centralized config at `APFramework/config.json`

---

## Current Progress

### ✅ Phase 1: AP World - COMPLETE
**Status**: Generates successfully

- Python AP World implementation in `worlds/palworld/`
- 61 locations across 9 regions
- 33 unique items (+ Wood as filler to reach 61)
- 14 player options (goal type, starting items, etc.)
- Generates valid `.apworld` files and multiworld ZIPs

**Known Simplifications** (temporary for Phase 3 testing):
- All items classified as `filler` (should be `progression`/`useful`)
- Hub-and-spoke region connections (should be progression-based)
- No access rules (should require items to unlock regions/locations)
- Completion condition is `lambda state: True` (should check goal)

### ✅ Phase 2: APFramework Core - COMPLETE
**Status**: Working

- File-based submodule discovery ✅
- JSON parsing (lunajson) ✅
- Manifest generation (`APCapabilities.json`) ✅
- Event bus for mod communication ✅
- State management ✅
- Test submodule (APTest) ✅

### ✅ Phase 3: Client Connection - COMPLETE
**Status**: Successfully connecting and authenticating!

**What Works**:
- lua-apclientpp DLL loads successfully ✅
- APClient wrapper created with logging ✅
- Client instantiation succeeds ✅
- Event handlers register without errors ✅
- Polling executes without crashes ✅
- Connection reaches state 4 (SLOT_CONNECTED) ✅
- Server logs: "TP1 (Team #1) playing Palworld has joined" ✅

**Key Fix**:
```lua
-- Before (broken):
self.client = apclientpp.new(uuid, game_name, server)

-- After (working):
self.client = apclientpp(uuid, game_name, server)
```

The lua-apclientpp library uses `__call` metamethod as constructor, not a `.new()` method.

---

## Phase 4: Runtime Integration (CURRENT)

### ✅ Completed Features

#### 1. Connection Config Redesign ✅
**Status**: COMPLETE

**Implementation**:
- Centralized connection configuration at `APFramework/config.json`
- Per-mod configs (`ap_config.json`) now only contain capabilities (items/locations/regions)
- ConfigManager updated with new methods:
  - `GetConnectionConfig()` - Get AP connection settings
  - `SetConnectionConfig(config)` - Update connection settings
  - `Save()` - Persist config changes to disk
- Poll interval configurable (`poll_interval_ms` in framework config, default: 16ms)

**Benefits**:
- Single source of truth for connection settings
- Easier to manage for users
- Cleaner separation of concerns (framework config vs mod capabilities)
- Foundation for future APMenuMod UI

**Files Modified**:
- `APFramework/config.json` - Created with connection settings
- `APFramework/config.example.json` - Moved to framework root
- `ConfigManager.lua` - Added new methods, updated file path
- `main.lua` - Reads connection from ConfigManager instead of mod scanning
- `APTest/ap_config.json` - Removed `ap_connection` section

#### 2. Continuous Polling (Temporary Solution) ✅
**Status**: WORKING (temporary implementation)

**Implementation**:
Blocking while loop in main Lua state:

```lua
local processing = true
local start_time = os.clock()

while processing do
    local current_time = os.clock()
    local elapsed_time = current_time - start_time

    if elapsed_time >= poll_interval_sec then
        -- Poll the client
        local apclient = APFrameworkCore:GetAPClient()
        if apclient then
            apclient:Poll()
        end

        -- Execute frame callbacks for submods
        EventBus:ExecuteFrameCallbacks()

        start_time = current_time
    end
end
```

**Why This Works**:
- Runs in the **same Lua state** as APClient creation
- No threading = No "Lua state changed" errors
- APClient connection stays alive
- Handlers fire correctly when items received

**Critical Limitations**:
- ⚠️ **Blocks all subsequent mods** - UE4SS loads mods sequentially on shared thread
- ⚠️ **No yielding to UE4** - Cannot use UE4 functions in event handlers yet
- ⚠️ **Not production-ready** - Acceptable for testing only

**Testing Results**:
```
[APFramework] Starting continuous polling (interval: 16ms)
[APClient] Item received: 8370050  ← SUCCESS!
```

Item receiving via `!getitem` command confirmed working!

#### 3. Frame Callback System for Submods ✅
**Status**: IMPLEMENTED

**Purpose**: Allow submods to execute code during polling loop

**API**:
```lua
APFramework.RegisterFrameCallback(mod_id, callback)
APFramework.UnregisterFrameCallback(mod_id)
```

**Usage**:
```lua
-- In submod's main.lua
APFramework.RegisterFrameCallback("my_mod", function()
    -- Runs every poll iteration (~16ms by default)
    -- Same Lua state as APClient
    -- Can perform Lua operations
    -- Cannot use UE4 functions yet (no game thread)
end)
```

**Implementation**:
- EventBus manages callbacks
- Executed during polling loop after APClient:Poll()
- Error isolation via pcall() - one failure won't crash others
- Logged errors show mod_id for debugging

**Files Modified**:
- `EventBus.lua` - Added frame callback management
- `APFramework.lua` - Exposed public API
- `main.lua` - Integrated callback execution into polling loop

### ❌ Known Limitations (To Be Addressed)

1. **Threading Issue**:
   - Blocking loop prevents other mods from loading
   - Need to find UE4 game tick hook that runs in same Lua state
   - Possible approaches: RegisterHook on PlayerController tick, UE4SS event system

2. **No UE4 Operations Yet**:
   - Event handlers run in polling loop (not game thread)
   - Cannot grant items to player yet
   - Cannot check locations via UE4 hooks yet
   - Will need ExecuteInGameThread() once we find proper hook

3. **Item Granting Not Implemented**:
   - Handlers fire correctly
   - Item data received
   - Need to map AP item IDs to Palworld items
   - Need UE4 inventory modification functions

4. **Location Checking Not Implemented**:
   - Need to hook Palworld game events (chest open, pal capture, etc.)
   - Need to map game events to AP location IDs
   - Need to call `APClient:CheckLocation(location_id)`

5. **State Persistence Not Implemented**:
   - Need to save `last_received_index`
   - Need to save `checked_locations`
   - Need to restore on reconnection

### 📋 Current Testing Status

**What's Been Tested**:
- ✅ Framework initialization
- ✅ Mod discovery
- ✅ Connection to AP server
- ✅ Authentication (slot connected)
- ✅ Continuous polling (stable, no crashes)
- ✅ Item receiving handler fires (tested with `!getitem` command)
- ✅ Event data logged correctly

**What Hasn't Been Tested**:
- ❌ Item granting to player inventory
- ❌ Location checking
- ❌ State persistence across sessions
- ❌ Multi-mod support
- ❌ Performance with other UE4SS mods installed

---

## Next Steps

### Immediate Priorities

1. **Research Permanent Polling Solution** (HIGH PRIORITY)
   - Find UE4 game tick hook that runs in same Lua state
   - Investigate RegisterHook on PlayerController:Tick
   - Contact UE4SS maintainers for guidance
   - Goal: Non-blocking polling that doesn't interfere with other mods

2. **Implement Item Granting** (MEDIUM PRIORITY)
   - Research Palworld inventory modification functions
   - Map AP item IDs to Palworld items
   - Implement item spawning in handler
   - Test with various item types (resources, pals, technology)

3. **Implement Location Checking** (MEDIUM PRIORITY)
   - Research Palworld game event hooks
   - Start with simple events (chest opens)
   - Map game events to AP location IDs
   - Send LocationChecks() to server

4. **Implement State Persistence** (LOW PRIORITY)
   - Extend StateManager for AP state
   - Save last_received_index after each item
   - Save checked_locations after each check
   - Load on startup and restore state

### Future Work

5. **Restore AP World Rules**
   - Restore item classifications (progression/useful/filler)
   - Restore region connections (progression-based)
   - Restore access rules (require items to unlock)
   - Restore proper completion condition

6. **Full Integration Testing**
   - Generate multiworld with multiple games
   - Test cross-game item sending
   - Test goal completion
   - Test edge cases (disconnect/reconnect, save/load)

7. **Polish & Release**
   - Remove debug logging
   - Add user-friendly error messages
   - Write installation guide
   - Publish to AP community

---

## Technical Details

### lua-apclientpp Library

**API Version**: v0.6.4 (built on apclientpp)

**Key Methods**:
```lua
-- Create client
client = apclientpp(uuid, game_name, server)  -- server = "host:port"

-- Connection lifecycle
client:poll()  -- Call every frame to process network I/O
state = client:get_state()  -- 0-4 (DISCONNECTED to SLOT_CONNECTED)

-- Event handlers
client:set_socket_connected_handler(function() end)
client:set_room_info_handler(function() end)
client:set_slot_connected_handler(function(slot_data) end)
client:set_items_received_handler(function(items) end)
client:set_location_checked_handler(function(locations) end)

-- Commands
client:ConnectSlot(name, password, items_handling, tags, version)
client:LocationChecks(location_ids)
client:StatusUpdate(status)
```

### Configuration System

**Framework Config** (`APFramework/config.json`):
```json
{
  "framework": {
    "version": "1.0.0",
    "debug_mode": true,
    "auto_connect": true,
    "poll_interval_ms": 16
  },
  "ap_connection": {
    "enabled": true,
    "server": "localhost",
    "port": 38281,
    "slot_name": "Player1",
    "password": "",
    "auto_reconnect": true
  },
  "ui": {
    "show_notifications": true,
    "show_debug_overlay": false
  }
}
```

**Mod Config** (`APFramework/Mods/*/ap_config.json`):
```json
{
  "ap_enabled": true,
  "mod_info": {
    "id": "aptest",
    "name": "AP Test Mod",
    "version": "1.0.0"
  },
  "capabilities": {
    "items": [...],
    "locations": [...],
    "regions": [...]
  }
}
```

---

## Resources

### Documentation
- Archipelago API: https://github.com/ArchipelagoMW/Archipelago/blob/main/docs/network%20protocol.md
- lua-apclientpp: https://github.com/black-sliver/lua-apclientpp
- apclientpp: https://github.com/black-sliver/apclientpp
- UE4SS: https://docs.ue4ss.com/

### Communication
- Archipelago Discord: https://discord.gg/archipelago
- Channel: #tech-support (for connection issues)
- Channel: #world-dev (for AP World questions)

---

**For questions or collaboration, see COLLABORATOR_NOTES.md in the repository.**
