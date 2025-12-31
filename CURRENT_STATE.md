# Palworld Archipelago - Current State

**Last Updated**: December 30, 2024
**Status**: Phase 3 - Client Connection (BLOCKED)

## Table of Contents
1. [Project Overview](#project-overview)
2. [Architecture](#architecture)
3. [Current Progress](#current-progress)
4. [Blocking Issue](#blocking-issue)
5. [Repository Structure](#repository-structure)
6. [Development Workflow](#development-workflow)
7. [Technical Details](#technical-details)
8. [Next Steps](#next-steps)

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

### Current Phase: Phase 3 - Client Connection
**Objective**: Establish WebSocket connection between the Lua client (running in Palworld via UE4SS) and the Archipelago server.

**Status**: ❌ **BLOCKED** - Connection failing (see [Blocking Issue](#blocking-issue))

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
│  │  │  - Mod discovery & registration                 │  │  │
│  │  │  - Capability manifest generation               │  │  │
│  │  │  - APClient (lua-apclientpp wrapper)            │  │  │
│  │  │    └─ WebSocket connection to AP server         │  │  │
│  │  │                                                   │  │  │
│  │  │  Submodules (e.g., APTest):                     │  │  │
│  │  │  - ap_config.json (connection info)             │  │  │
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
├── Scripts/           # Core framework code
│   ├── main.lua       # UE4SS entry point
│   ├── APFramework.lua
│   ├── APClient.lua   # WebSocket client
│   ├── ModRegistry.lua
│   ├── CapabilityManager.lua
│   └── lib/
│       ├── apclient_wrapper.lua  # Debug wrapper for lua-apclientpp
│       ├── lua-apclientpp.dll    # Native WebSocket library
│       └── lunajson/             # JSON parsing
└── Mods/              # Submodules (AP-enabled mods)
    └── APTest/
        ├── ap_config.json  # Mod metadata + connection info
        └── main.lua        # Mod-specific logic (optional)
```

**Discovery Process**:
1. APFramework scans `APFramework/Mods/*/ap_config.json`
2. Parses each config for items, locations, regions, connection info
3. Generates `APCapabilities.json` manifest (used by Python AP World during generation)
4. Connects to AP server using connection info from first mod with `ap_connection` field

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

See `worlds/palworld/docs/CURRENT_STATE.md` for restoration plan.

### ✅ Phase 2: APFramework Core - COMPLETE
**Status**: Working

- File-based submodule discovery ✅
- JSON parsing (lunajson) ✅
- Manifest generation (`APCapabilities.json`) ✅
- Event bus for mod communication ✅
- State management ✅
- Test submodule (APTest) ✅

**Verified Working**:
```
[APFramework] Total: 1 mods, 1 locations, 1 items
```
APTest mod discovered, capabilities aggregated correctly.

### ❌ Phase 3: Client Connection - BLOCKED
**Status**: Connection failing

**What Works**:
- lua-apclientpp DLL loads successfully ✅
- APClient wrapper created with logging ✅
- Client instantiation succeeds ✅
- Event handlers register without errors ✅
- Polling executes without crashes ✅

**What Doesn't Work**:
- Connection stuck at state 1 (SOCKET_CONNECTING) ❌
- Never progresses to state 2 (SOCKET_CONNECTED) ❌
- No event handlers fire (no socket_connected, no errors, nothing) ❌
- Server sees either "400 Bad Request" or no connection at all ❌

See [Blocking Issue](#blocking-issue) for details.

---

## Blocking Issue

### Problem: lua-apclientpp Connection Failure

**Symptoms**:
```lua
[APClient] Poll iteration 20 (state=1, connected=false, authenticated=false)
[APClient] Poll iteration 40 (state=1, connected=false, authenticated=false)
[APClient] Poll iteration 60 (state=1, connected=false, authenticated=false)
[APClient] Poll iteration 80 (state=1, connected=false, authenticated=false)
[APClient] Poll iteration 100 (state=1, connected=false, authenticated=false)
[APClient] Final state: 1 (connected=false, authenticated=false)
```

**Connection States** (from lua-apclientpp API):
- 0 = DISCONNECTED
- 1 = **SOCKET_CONNECTING** ← stuck here
- 2 = SOCKET_CONNECTED
- 3 = ROOM_INFO
- 4 = SLOT_CONNECTED

**Analysis**:
State 1 means the underlying socket is attempting to connect, but the **WebSocket handshake never completes**.

### Evidence

**Test 1: Network Connectivity**
Created `test_ap_connection.bat` to verify TCP connection works:
```
SUCCESS: Connected to localhost:38281
```
✅ Network is fine - raw TCP connects successfully.

**Test 2: lua-apclientpp DLL Builds Tried**

| Build | Load Result | Connection Result |
|-------|-------------|-------------------|
| `ucrt64-dynamic` | ❌ Missing dependencies | N/A |
| ClairObscur mod DLL (7.1MB) | ✅ Loads | ❌ Server: "400 Bad Request" |
| `lua54-clang64-static` (current) | ✅ Loads | ❌ State stuck at 1, no server connection seen |

**Test 3: Server Logs**

With ClairObscur DLL:
```
connection rejected (400 Bad Request)
connection closed
```
Indicates WebSocket handshake reached server but was invalid.

With clang64-static DLL:
```
(no connection attempt logged)
```
Suggests handshake never reaches server.

### Root Cause Hypotheses

1. **WebSocket Protocol Mismatch**
   - lua-apclientpp may be using incompatible WebSocket protocol
   - AP server expects specific headers/format

2. **Version Incompatibility**
   - lua-apclientpp built with apclientpp v0.6.4
   - AP server may be v0.6.5 or newer
   - Protocol may have changed between versions

3. **Build Configuration Issues**
   - DLL built with wrong Lua version (5.4 vs 5.1/LuaJIT)
   - Missing SSL/TLS support
   - Incompatible compiler flags

4. **Connection Parameters**
   - Server URL format may be wrong (`localhost:38281` vs `ws://localhost:38281`)
   - Missing required headers or authentication

### Current Code

**APClient initialization** ([APClient.lua:49-95](APFramework/Scripts/APClient.lua#L49-L95)):
```lua
function APClient:Initialize(host, port)
    local wrapper_path = "ue4ss/Mods/APFramework/Scripts/lib/apclient_wrapper"
    local success, APClientWrapper = pcall(require, wrapper_path)

    if not success then
        print("[APClient] ERROR: Failed to load APClient wrapper")
        return true  -- Run in stub mode
    end

    local uuid = self.connection.uuid or ""
    local game_name = self.connection.game or "Palworld"
    local server = string.format("%s:%d", host, port)  -- "localhost:38281"

    self.client = APClientWrapper.new(uuid, game_name, server)
    return true
end
```

**APClient authentication** ([APClient.lua:102-151](APFramework/Scripts/APClient.lua#L102-L151)):
```lua
function APClient:Authenticate(slot_name, password, game_name)
    self:SetupEventHandlers()  -- Register handlers

    -- Poll to process network events
    for i = 1, 100 do
        self:Poll()
        if i % 20 == 0 then
            local state = self.client:get_state()
            print(string.format("[APClient] Poll iteration %d (state=%d, connected=%s, authenticated=%s)",
                i, state, tostring(self.connected), tostring(self.authenticated)))
        end
    end

    return true
end
```

**Event handlers** ([APClient.lua:154-242](APFramework/Scripts/APClient.lua#L154-L242)):
```lua
function APClient:SetupEventHandlers()
    -- Socket connected
    self.client:set_socket_connected_handler(function()
        print("[APClient] Socket connected to server")
        self.connected = true
    end)

    -- Room info (triggers ConnectSlot)
    self.client:set_room_info_handler(function()
        print("[APClient] Received room info, authenticating...")

        local items_handling = 7  -- 0b111 = receive all items
        local tags = {"Lua-APClientPP", "Palworld"}
        local client_version = {0, 5, 0}

        self.client:ConnectSlot(
            self.connection.slot_name,
            self.connection.password or "",
            items_handling,
            tags,
            client_version
        )
    end)

    -- Slot connected (authenticated)
    self.client:set_slot_connected_handler(function(slot_data)
        print("[APClient] Slot connected! Authenticated successfully")
        self.authenticated = true
    end)

    -- Error handler
    self.client:set_socket_error_handler(function(error_msg)
        print("[APClient] Socket error: " .. tostring(error_msg))
    end)

    -- ... other handlers
end
```

**NONE of these handlers fire.** State stays at 1 indefinitely.

### Debugging Resources

**lua-apclientpp Documentation**:
- Repository: https://github.com/black-sliver/lua-apclientpp
- Connection failure handling: https://github.com/black-sliver/lua-apclientpp?tab=readme-ov-file#handling-connection-failures
- Built on apclientpp: https://github.com/black-sliver/apclientpp

**Working Example**:
The ClairObscur mod successfully uses lua-apclientpp, suggesting it's possible. May need to:
- Contact ClairObscur mod author for build instructions
- Compare their DLL build settings
- Check their connection code for differences

### What We've Tried

1. ✅ Verified network connectivity (raw TCP works)
2. ✅ Tried multiple DLL builds (ucrt64-dynamic, ClairObscur, clang64-static)
3. ✅ Fixed all API calls (new, ConnectSlot, poll, handlers)
4. ✅ Added comprehensive logging (wrapper module)
5. ✅ Implemented polling loop (100 iterations)
6. ✅ Added state monitoring (checked every 20 polls)
7. ✅ Registered all event handlers (connected, error, room_info, slot_connected, etc.)

### What We Haven't Tried

1. ❌ Building lua-apclientpp ourselves with custom settings
2. ❌ Using WebSocket protocol prefix (`ws://localhost:38281` instead of `localhost:38281`)
3. ❌ Testing with different AP server versions
4. ❌ Alternative connection approaches (Python bridge, native Lua WebSocket, etc.)
5. ❌ Contacting lua-apclientpp or ClairObscur authors for help

---

## Repository Structure

### This Repository (`PWAP/`)

```
PWAP/
├── README.md                    # Project description
├── LICENSE                      # MIT License
├── CURRENT_STATE.md            # This file
│
├── APFramework/                # Lua client framework
│   ├── enabled.txt             # Empty file (UE4SS mod enabled marker)
│   ├── Scripts/
│   │   ├── main.lua            # UE4SS entry point
│   │   ├── APFramework.lua     # Core framework
│   │   ├── APClient.lua        # WebSocket client (BLOCKED)
│   │   ├── ModRegistry.lua     # Submodule discovery
│   │   ├── CapabilityManager.lua  # Manifest generation
│   │   ├── ConfigManager.lua   # Config parsing
│   │   ├── StateManager.lua    # State management
│   │   ├── EventBus.lua        # Event system
│   │   └── lib/
│   │       ├── apclient_wrapper.lua  # Debug wrapper
│   │       ├── lua-apclientpp.dll    # WebSocket library (NOT IN REPO - user provides)
│   │       ├── lua-apclientpp.lua    # API type definitions
│   │       └── lunajson/       # JSON parser
│   └── Mods/                   # Submodules
│       └── APTest/             # Test submodule
│           ├── ap_config.json  # Test config with connection info
│           ├── main.lua        # Test mod entry point
│           └── README.md
│
└── worlds/                     # AP World implementation
    └── palworld/
        ├── __init__.py         # World definition
        ├── archipelago.json    # Manifest for AP launcher
        ├── items.py            # Item definitions
        ├── locations.py        # Location definitions
        ├── regions.py          # Region/entrance definitions
        ├── options.py          # Player options
        ├── rules.py            # Logic rules
        ├── mod_interface.py    # Reads APCapabilities.json
        └── docs/
            └── CURRENT_STATE.md  # AP World status
```

### Development Directory (`CC/`)

**Location**: `C:\Users\micha\Desktop\AP\CC\`

This is the **active development directory** - all changes are made here, then copied to `PWAP/` for version control.

**Why separate?**:
- `CC/` is a workspace for testing/debugging
- `PWAP/` is the clean Git repository
- Prevents cluttering repo with test files, logs, etc.
- `.claude/` folder in `CC/` contains development notes (not in repo)

**Workflow**:
1. Make changes in `CC/APFramework/` or `CC/worlds/palworld/`
2. Test locally
3. Copy working files to `PWAP/` when ready to commit
4. User manually copies `CC/APFramework/` to game directory for in-game testing

### Game Installation

**Location**: `E:\SteamLibrary\steamapps\common\Palworld\Pal\Binaries\Win64\ue4ss\Mods\APFramework\`

**Important**: We **never modify files directly here**. Only copy from `CC/` for testing.

---

## Development Workflow

### Testing the AP World (Python)

1. **Package the .apworld**:
   ```bash
   cd C:/Users/micha/Desktop/AP/CC
   python make_apworld.py
   # Creates palworld.apworld
   ```

2. **Install to Archipelago**:
   ```bash
   cp palworld.apworld D:/Programs/Archipelago/custom_worlds/
   ```

3. **Generate multiworld**:
   ```bash
   cd D:/Programs/Archipelago
   ./ArchipelagoGenerate.exe --player_files_path "C:/Users/micha/Desktop/AP/CC/test_yamls"
   ```
   Output: `D:/Programs/Archipelago/output/AP_*.zip`

4. **Host multiworld**:
   ```bash
   ./ArchipelagoServer.exe
   # Listens on localhost:38281
   ```

### Testing APFramework (Lua)

1. **Edit files** in `C:/Users/micha/Desktop/AP/CC/APFramework/`

2. **Copy to game directory**:
   ```bash
   # User manually copies entire APFramework folder
   cp -r C:/Users/micha/Desktop/AP/CC/APFramework/* \
         E:/SteamLibrary/steamapps/common/Palworld/Pal/Binaries/Win64/ue4ss/Mods/APFramework/
   ```

3. **Launch Palworld**

4. **Check logs**:
   ```
   E:/SteamLibrary/steamapps/common/Palworld/Pal/Binaries/Win64/ue4ss/UE4SS.log
   ```

### Current Test Configuration

**AP World YAML** (`CC/test_yamls/test_player.yaml`):
```yaml
name: Test Player 1
game: Palworld
Palworld:
  goal: defeat_eternal_pyre_tower
  starting_pal_count: 3
  # ... other options
```

**APTest Config** (`CC/APFramework/Mods/APTest/ap_config.json`):
```json
{
  "ap_enabled": true,
  "ap_connection": {
    "server": "localhost",
    "port": 38281,
    "slot_name": "Test Player 1",
    "password": ""
  },
  "capabilities": {
    "items": [{"id": "test_item", "name": "Test Item"}],
    "locations": [{"id": "test_location", "name": "Test Location"}],
    "regions": [{"name": "test_region"}]
  }
}
```

**Expected Flow**:
1. UE4SS loads APFramework
2. APFramework discovers APTest
3. Generates APCapabilities.json
4. Connects to localhost:38281
5. Authenticates as "Test Player 1"
6. Receives items/sends location checks

**Actual Flow**:
Steps 1-3 work. Step 4 fails (connection stuck at state 1).

---

## Technical Details

### UE4SS Integration

**UE4SS** (Unreal Engine 4 Scripting System) injects Lua scripting into UE4 games.

**Mod Loading**:
- Each enabled mod must have `enabled.txt` in its root
- UE4SS runs `Scripts/main.lua` for each mod
- Each mod runs in **isolated Lua environment** (separate `_G` namespace)

**APFramework as UE4SS Mod**:
```
ue4ss/
└── Mods/
    ├── APFramework/          # Our framework
    │   ├── enabled.txt
    │   └── Scripts/
    │       └── main.lua      # Entry point
    └── SomeOtherMod/         # Can't access APFramework's _G
        ├── enabled.txt
        └── Scripts/
            └── main.lua
```

**Why Submodule Architecture**:
Since mods can't share globals, we put AP mods **inside** APFramework as submodules, so they're discovered via file scanning instead of Lua communication.

### lua-apclientpp Library

**Purpose**: Native C++ library with Lua bindings for Archipelago WebSocket protocol.

**API Version**: v0.6.4 (built on apclientpp)

**Key Methods**:
```lua
-- Create client
client = APClient.new(uuid, game_name, server)  -- server = "host:port"

-- Connection lifecycle
client:poll()  -- Call every frame to process network I/O
state = client:get_state()  -- 0-4 (DISCONNECTED to SLOT_CONNECTED)

-- Event handlers
client:set_socket_connected_handler(function() end)
client:set_room_info_handler(function() end)
client:set_slot_connected_handler(function(slot_data) end)
client:set_socket_error_handler(function(error_msg) end)
client:set_items_received_handler(function(items) end)

-- Commands
client:ConnectSlot(name, password, items_handling, tags, version)
client:LocationChecks(location_ids)
client:StatusUpdate(status)
```

**Expected Connection Flow**:
1. `new()` creates client, starts connecting (state → 1)
2. `poll()` processes WebSocket handshake (state → 2)
3. `socket_connected` handler fires
4. Server sends RoomInfo (state → 3)
5. `room_info` handler fires, calls `ConnectSlot()`
6. Server sends Connected (state → 4)
7. `slot_connected` handler fires with slot_data

**Actual Behavior**:
Steps 1-2 happen, but state never progresses past 1. No handlers fire.

### Capability Manifest System

**Purpose**: Allow Python AP World to discover what items/locations/regions are available in Palworld mods.

**Generation** ([CapabilityManager.lua](APFramework/Scripts/CapabilityManager.lua)):
1. Scan `APFramework/Mods/*/ap_config.json`
2. Aggregate `capabilities.items`, `capabilities.locations`, `capabilities.regions`
3. Write to `APCapabilities.json` in game's working directory

**Consumption** ([mod_interface.py](worlds/palworld/mod_interface.py)):
```python
def load_capabilities():
    # Read APCapabilities.json
    # Merge with built-in items/locations
    # Return combined data for randomizer
```

**Why This Design**:
- Allows dynamic content (mods can add items/locations)
- Python world doesn't need to hardcode everything
- Mods can be developed independently

**Current Status**: ✅ Working - APTest capabilities correctly aggregated.

---

## Next Steps

### Immediate Priority: Unblock Connection

**Option 1: Build lua-apclientpp Ourselves**
- Clone https://github.com/black-sliver/lua-apclientpp
- Build with Lua 5.4 + static linking
- Match ClairObscur's working configuration
- May require contacting ClairObscur author for build instructions

**Option 2: Protocol Debugging**
- Add Wireshark packet capture to see exact WebSocket handshake
- Compare with successful AP client (e.g., Python client)
- Identify protocol differences

**Option 3: Alternative Connection Method**
- Python bridge: Run Python AP client alongside game, communicate via file/socket
- Native Lua WebSocket: Implement WebSocket in pure Lua (slower, but controllable)
- AP.lua: Use existing Lua AP client if available

**Option 4: Community Help**
- Contact lua-apclientpp maintainer (black-sliver)
- Contact ClairObscur mod author (working implementation)
- Post on Archipelago Discord #tech-support

### After Connection Works

1. **Implement Location Checks**
   - Hook into Palworld's location unlock events
   - Call `APClient:LocationChecks(location_ids)`
   - Verify server receives checks

2. **Implement Item Receiving**
   - Handle `items_received` events
   - Grant items to player in Palworld
   - Update save state

3. **State Persistence**
   - Save received items to file
   - Load on game start
   - Handle reconnection

4. **Restore AP World Rules**
   - Restore item classifications (progression/useful/filler)
   - Restore region connections (progression-based)
   - Restore access rules (require items to unlock)
   - Restore proper completion condition

5. **Full Integration Testing**
   - Generate multiworld with multiple games
   - Test item sending/receiving
   - Test goal completion
   - Test edge cases (disconnect/reconnect, save/load, etc.)

6. **Polish & Release**
   - Remove debug logging
   - Add user-friendly error messages
   - Write installation guide
   - Publish to AP community

---

## Questions for Collaborator

1. **Connection Issue**: Do you have experience with lua-apclientpp or WebSocket debugging?
   - Can you help diagnose why the WebSocket handshake fails?
   - Do you have access to a working build environment for lua-apclientpp?

2. **Architecture Feedback**: Does the submodule approach make sense?
   - Are there better ways to handle UE4SS mod isolation?
   - Should we consider alternative frameworks (e.g., native C++ mod)?

3. **Python AP World**: Any suggestions for the simplified logic?
   - See `worlds/palworld/docs/CURRENT_STATE.md` for restoration plan
   - Should we restore full logic before or after connection works?

4. **Development Priorities**: What should we focus on?
   - Unblock connection (highest priority)
   - Improve AP World logic
   - Add more items/locations
   - Better error handling

---

## Resources

### Documentation
- Archipelago API: https://github.com/ArchipelagoMW/Archipelago/blob/main/docs/network%20protocol.md
- lua-apclientpp: https://github.com/black-sliver/lua-apclientpp
- apclientpp: https://github.com/black-sliver/apclientpp
- UE4SS: https://docs.ue4ss.com/

### Similar Projects
- ClairObscur (uses lua-apclientpp): Check mod for working configuration
- Other Lua-based AP integrations: Research for patterns/solutions

### Communication
- Archipelago Discord: https://discord.gg/archipelago
- Channel: #tech-support (for connection issues)
- Channel: #world-dev (for AP World questions)

---

## Development History

This document reflects the state after extensive debugging of the connection issue. Key milestones:

- **Phase 1**: AP World implementation (simplified for testing) ✅
- **Phase 2**: APFramework core (discovery, manifest) ✅
- **Phase 3**: Connection attempt
  - Tried multiple DLL builds (ucrt64, ClairObscur, clang64)
  - Fixed API incompatibilities (new, ConnectSlot, poll)
  - Added comprehensive logging
  - **Currently blocked on WebSocket handshake failure**

The active development workspace is at `C:\Users\micha\Desktop\AP\CC\`, with this repository (`PWAP/`) serving as the clean version-controlled copy.

---

**For questions or collaboration, contact the project owner or discuss in the Archipelago community.**