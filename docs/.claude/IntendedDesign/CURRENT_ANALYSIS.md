# Current Implementation vs Intended Design - Gap Analysis (REVISED)

**Document Version**: 2.0
**Date**: 2026-01-02
**Status**: Comprehensive re-analysis with corrected design understanding

---

## Executive Summary

This document provides a comprehensive comparison between the **current implementation** of APFramework (IPC branch) and the **corrected intended design** as specified in [DESIGN_AND_FLOW.md](DESIGN_AND_FLOW.md), incorporating critical design clarifications.

### Quick Assessment

| Aspect | Current State | Intended Design | Gap Level |
|--------|---------------|-----------------|-----------|
| Library Separation | ✅ Two libraries exist | Framework + Client libs | 🟢 **Correct** |
| Framework Mod Dual Role | ❌ Not implemented | Server + Priority Client | 🔴 **CRITICAL** |
| Incompatibility System | ❌ Not implemented | Auto-disable conflicts | 🔴 **CRITICAL** |
| Dependency System | ❌ Not implemented | Hard requirements | 🔴 **CRITICAL** |
| Multi-Slot Capabilities | ❌ Single file only | Per-slot JSON files | 🔴 **CRITICAL** |
| Capabilities Validation | ⚠️ Basic | Zero-tolerance conflicts | 🟡 **Partial** |
| Runtime Enablement | ❌ Not implemented | Editable at runtime | 🔴 **Missing** |
| Log Routing to UE4SS | ❌ Not implemented | Via framework mod | 🔴 **Missing** |
| Lua JSON Integration | ⚠️ Custom impl | lunajson integration | 🟡 **Partial** |
| AP World Python Support | ✅ Exists | Dynamic capabilities | 🟢 **Advanced** |

### Overall Gap Assessment

- **Architecture**: ⚠️ 70% aligned - missing dual-role framework mod
- **Features**: ⚠️ 45% complete - missing critical safety systems
- **Quality**: ✅ Solid foundation, but needs major additions

---

## Critical Design Clarifications

### Two Separate Libraries (CORRECTED UNDERSTANDING)

**Framework C++ Library** (`APFrameworkCore.dll`):
- Used **ONLY** by Lua framework mod
- Provides: IPC server, AP client, mod registry, message routing
- Acts as the "server" in the IPC architecture

**Client C++ Library** (`APClientLib.dll`):
- Used by **ALL** mods including the Lua framework mod
- Provides: IPC client, message sending/receiving, callbacks
- Lightweight wrapper for mod communication

**Lua Framework Mod**:
- Uses `APFrameworkCore.dll` to run as IPC server
- Uses `APClientLib` (Lua wrapper) to register as priority client
- Mod ID: `archipelago.palworld.framework`
- Receives ALL important framework logs and status updates
- Pumps messages to UE4SS console for user visibility

---

## 1. Framework Architecture - Library Separation

### 1.1 Intended Design

**Two Libraries with Clear Separation**:
```
APFrameworkCore.dll (Framework Library)
  - IPC Server (Named Pipes)
  - AP Client Wrapper (apclientpp)
  - Mod Registry & Discovery
  - Message Router
  - Capabilities Generator
  - Logger
  - Config Manager
  ↓
  Used ONLY by: Lua Framework Mod

APClientLib.dll (Client Library)
  - IPC Client (Named Pipes)
  - Message serialization
  - Callback system
  - Simple registration API
  ↓
  Used by: ALL mods (including Lua Framework Mod)
```

**Lua Framework Mod** ([APFramework/Scripts/main.lua](../../APFramework/Scripts/main.lua)):
- Loads `APFrameworkCore.dll` via Lua C bindings
- Loads `ap_client.lua` (Lua wrapper for client lib)
- Calls `framework_core.start_ipc()` to become server
- Calls `ap_client:register("archipelago.palworld.framework", {...})` to become priority client
- Acts as message pump to UE4SS console

### 1.2 Current Implementation

**Libraries**:
- ✅ `APFrameworkCore.dll` exists ([src/framework_core/](../../src/framework_core/))
- ✅ `APClientLib.dll` exists ([src/client_lib/](../../src/client_lib/))
- ✅ Lua wrapper `ap_client.lua` exists ([src/lua_client/ap_client.lua](../../src/lua_client/ap_client.lua))

**Framework Mod**:
- ✅ Uses `APFrameworkCore.dll` via Lua bindings
- ❌ **DOES NOT** use `ap_client.lua` to register as client
- ❌ **DOES NOT** register with mod ID `archipelago.palworld.framework`
- ❌ **DOES NOT** act as dual-role (server + priority client)

### 1.3 Gap Analysis

| Feature | Intended | Current | Status |
|---------|----------|---------|--------|
| Two separate libraries | Required | Implemented | ✅ **Complete** |
| Framework lib for server only | Required | Correct usage | ✅ **Complete** |
| Client lib for all mods | Required | Library exists | ✅ **Complete** |
| Framework mod uses both libs | Required | Only uses framework lib | ❌ **MISSING** |
| Registers as priority client | Required | Not implemented | ❌ **MISSING** |
| Mod ID: archipelago.palworld.framework | Required | Not registered | ❌ **MISSING** |

**Critical Issue**: Framework mod cannot receive its own messages, cannot participate in AP session as a mod, and cannot demonstrate the client library to other mod developers.

**Files to Modify**:
- [APFramework/Scripts/main.lua](../../APFramework/Scripts/main.lua) - Add client registration
- [src/framework_core/src/framework_core.cpp](../../src/framework_core/src/framework_core.cpp) - Special handling for framework mod_id

---

## 2. Mod Metadata Schema

### 2.1 Intended Schema (v2 - CORRECTED)

```json
{
  "schema_version": 2,
  "mod_id": "author.game.mod",
  "version": "1.2.3",
  "display_name": "My Awesome Mod",
  "description": "Adds cool features",
  "supported_game_versions": ">=0.3.0 <0.4.0",

  "dependencies": {
    "required.mod.id": {"min_version": "1.0.0", "max_version": "2.0.0"},
    "another.required.mod": true
  },

  "incompatible_mods": {
    "conflicting.mod.id": {"min_version": "1.0.0", "max_version": "2.0.0"},
    "broken.mod.id": ["1.5.0", "1.5.1", "1.6.3"],
    "completely.incompatible.mod": true
  },

  "enabled": true,

  "capabilities": {
    "items": [
      {"id": 100000, "name": "Item", "classification": "useful"}
    ],
    "locations": [
      {"id": 200000, "name": "Location", "region": "Region"}
    ],
    "regions": [
      {"name": "Region", "connects_to": ["Other"]}
    ],
    "custom_fields": {
      "tech_trees": [...],
      "breeding_data": [...]
    }
  }
}
```

**Key Design Points**:
- **Dependencies**: Hard requirements - registration **DENIED** if missing
- **Incompatibilities**: Three types:
  1. Version range: `{"min_version": "1.0", "max_version": "2.0"}`
  2. Specific versions: `["1.5.0", "1.5.1"]`
  3. Complete ban: `true`
- **Mutual incompatibility**: Both mods disabled if both declare each other
- **One-way incompatibility**: Only the declaring mod disabled
- **Enablement**: Editable at runtime by framework (in memory AND file)
- **Custom capabilities**: Extensible base schema for future-proofing

### 2.2 Current Implementation

**Current Schema**:
```json
{
  "mod_id": "SomeModId",
  "items": [...],
  "locations": [...],
  "regions": [...]
}
```

**What Exists**:
- ✅ mod_id field
- ✅ items, locations, regions

**What's Missing**:
- ❌ schema_version
- ❌ version, display_name, description
- ❌ supported_game_versions
- ❌ **dependencies** (CRITICAL)
- ❌ **incompatible_mods** (CRITICAL)
- ❌ **enabled** field
- ❌ Custom extensible fields

### 2.3 Gap Analysis

| Field | Intended | Current | Status |
|-------|----------|---------|--------|
| schema_version | Required | Not present | ❌ **Missing** |
| mod_id | Required | Present | ✅ **Complete** |
| version | Required | Not present | ❌ **Missing** |
| display_name | Required | Not present | ❌ **Missing** |
| description | Optional | Not present | ❌ **Missing** |
| supported_game_versions | Optional | Not present | ❌ **Missing** |
| **dependencies** | **CRITICAL** | **Not present** | ❌ **CRITICAL** |
| **incompatible_mods** | **CRITICAL** | **Not present** | ❌ **CRITICAL** |
| **enabled** | **CRITICAL** | **Not present** | ❌ **CRITICAL** |
| capabilities | Required | Present | ✅ **Complete** |
| Custom fields in capabilities | Optional | Supported via JSON | ✅ **Complete** |

**Impact**:
- Cannot enforce mod dependencies → mods may load without required dependencies
- Cannot detect incompatibilities → conflicts cause crashes
- Cannot disable mods at runtime → no conflict resolution
- No versioning → cannot warn about outdated mods

**Files to Modify**:
- [src/framework_core/include/mod_registry.h](../../src/framework_core/include/mod_registry.h) - Extend ModMetadata struct
- [src/framework_core/src/mod_registry.cpp](../../src/framework_core/src/mod_registry.cpp) - Parse new fields, add validation

---

## 3. Dependency System (NEW - CRITICAL)

### 3.1 Intended Design

**Purpose**: Ensure mods can declare hard requirements on other mods.

**Behavior**:
- If dependency is missing → **Registration DENIED**
- If dependency exists but wrong version → **Registration DENIED**
- If dependency is disabled due to its own missing dependency → **Cascade disable**

**Schema**:
```json
"dependencies": {
  "base.palworld.lib": true,  // Any version
  "helper.palworld.utils": {"min_version": "1.0.0"},  // Min only
  "data.palworld.tables": {"min_version": "2.0.0", "max_version": "3.0.0"}  // Range
}
```

**Discovery Flow**:
```
1. Discover all mods (scan ap_config.json)
2. Parse dependencies for each mod
3. Build dependency graph
4. For each mod:
   a. Check all dependencies exist
   b. Check all dependency versions satisfied
   c. If any missing/wrong version → mark mod as DISABLED
   d. Send IPC error to mod explaining why
5. Recursive pass: If mod X disabled, disable mods depending on X
6. Generate final list of enabled mods
```

**Example**:
```
Mod A (enabled) depends on Mod B v1.0+
Mod B v0.5 (wrong version)
Result: Mod A DISABLED (dependency version mismatch)

Mod C depends on Mod A
Result: Mod C DISABLED (dependency chain broken)
```

### 3.2 Current Implementation

**Status**: ❌ **COMPLETELY MISSING**

**What Doesn't Exist**:
- No `dependencies` field parsing
- No dependency validation logic
- No dependency graph construction
- No cascade disabling
- No IPC error messages for missing dependencies

### 3.3 Gap Analysis

| Feature | Intended | Current | Status |
|---------|----------|---------|--------|
| Dependencies schema | Required | Not present | ❌ **Missing** |
| Dependency validation | Required | Not implemented | ❌ **Missing** |
| Version range checking | Required | Not implemented | ❌ **Missing** |
| Registration denial | Required | Not implemented | ❌ **Missing** |
| Cascade disabling | Required | Not implemented | ❌ **Missing** |
| Error messages to mods | Required | Not implemented | ❌ **Missing** |

**Impact**: Mods can register without their required dependencies, leading to runtime crashes or broken functionality.

**Implementation Priority**: 🔴 **CRITICAL** - Must be Phase 1

---

## 4. Incompatibility System (NEW - CRITICAL)

### 4.1 Intended Design

**Purpose**: Prevent conflicting mods from running simultaneously.

**Three Types of Incompatibility**:

**Type 1: Version Range**
```json
"incompatible_mods": {
  "other.mod.id": {"min_version": "1.0.0", "max_version": "2.0.0"}
}
```
- Incompatible with specific version range
- Versions outside range are compatible

**Type 2: Specific Versions**
```json
"incompatible_mods": {
  "problematic.mod": ["1.5.0", "1.5.1", "1.6.3"]
}
```
- Incompatible with specific version numbers only
- All other versions are compatible

**Type 3: Complete Incompatibility**
```json
"incompatible_mods": {
  "conflicting.mod": true
}
```
- Completely incompatible with all versions
- Cannot coexist under any circumstances

**Conflict Resolution**:
- **Mutual incompatibility** (both mods list each other): Both disabled
- **One-way incompatibility** (only Mod A lists Mod B): Only Mod A disabled
- Disabled mods marked in discovery list with reason
- IPC warning message sent to disabled mods

**Discovery Flow**:
```
1. Discover all mods
2. Parse incompatibilities for each mod
3. Build incompatibility matrix
4. For each mod:
   a. Check if any discovered mods are in its incompatible list
   b. Check if versions match incompatibility criteria
   c. If conflict found:
      - Check if mutual (both list each other) → disable BOTH
      - Otherwise → disable ONLY the declaring mod
5. Send IPC warning to disabled mods with reason
```

**Example**:
```
Mod A: incompatible_mods: {"mod.b": true}
Mod B: (no incompatibility declaration)
Result: Mod A DISABLED (declared incompatibility with Mod B)

Mod C: incompatible_mods: {"mod.d": true}
Mod D: incompatible_mods: {"mod.c": true}
Result: BOTH Mod C and Mod D DISABLED (mutual incompatibility)
```

### 4.2 Current Implementation

**Status**: ❌ **COMPLETELY MISSING**

**What Doesn't Exist**:
- No `incompatible_mods` field parsing
- No incompatibility checking logic
- No conflict matrix construction
- No auto-disabling logic
- No mutual incompatibility detection
- No IPC warning messages

### 4.3 Gap Analysis

| Feature | Intended | Current | Status |
|---------|----------|---------|--------|
| Incompatibility schema | Required | Not present | ❌ **Missing** |
| Version range check | Required | Not implemented | ❌ **Missing** |
| Specific version check | Required | Not implemented | ❌ **Missing** |
| Complete ban check | Required | Not implemented | ❌ **Missing** |
| Mutual incompatibility | Required | Not implemented | ❌ **Missing** |
| Auto-disabling | Required | Not implemented | ❌ **Missing** |
| IPC warnings | Required | Not implemented | ❌ **Missing** |

**Impact**: Conflicting mods can register and run, causing crashes, data corruption, or unexpected behavior.

**Implementation Priority**: 🔴 **CRITICAL** - Must be Phase 1

---

## 5. Runtime Enablement System (NEW - CRITICAL)

### 5.1 Intended Design

**Purpose**: Allow framework (and framework mod with special privilege) to enable/disable mods at runtime.

**Capabilities**:
- Disable mods due to conflicts (automatic)
- Disable mods due to missing dependencies (automatic)
- Disable/enable mods via framework mod commands (manual)
- Persist enablement state to mod's `ap_config.json` file

**Implementation**:
```
In-Memory State:
  discovered_mods_[mod_id].enabled = true/false

On Disk:
  mod_folder/ap_config.json:
    "enabled": true/false

Runtime Modification:
  framework.disable_mod(mod_id, reason)
    → Set enabled=false in memory
    → Write to ap_config.json
    → Send IPC message to mod (if registered)
    → Log reason to framework log
```

**Special Privileges**:
- `APFrameworkCore` C++ library: Can disable any mod
- Lua framework mod (as priority client): Can disable any mod via IPC command
- Regular mods: Cannot disable other mods

**Use Cases**:
1. Auto-disable on conflict detected
2. Auto-disable on missing dependency
3. Manual disable via framework command (future UI)
4. User edits `ap_config.json` manually → respected on next startup

### 5.2 Current Implementation

**Status**: ⚠️ **PARTIALLY IMPLEMENTED**

**What Exists**:
- Discovery can filter mods (filesystem scan)
- `enabled.txt` file check mentioned in redesign docs

**What's Missing**:
- ❌ No `enabled` field in `ap_config.json`
- ❌ No in-memory enablement tracking
- ❌ No runtime modification API
- ❌ No file writing on enablement change
- ❌ No IPC message for disabled mods
- ❌ No special privilege system for framework mod

### 5.3 Gap Analysis

| Feature | Intended | Current | Status |
|---------|----------|---------|--------|
| Enabled field in config | Required | Not present | ❌ **Missing** |
| In-memory tracking | Required | Not implemented | ❌ **Missing** |
| Runtime disable API | Required | Not implemented | ❌ **Missing** |
| File persistence | Required | Not implemented | ❌ **Missing** |
| IPC notification | Required | Not implemented | ❌ **Missing** |
| Special privileges | Required | Not implemented | ❌ **Missing** |
| Manual user editing | Required | Would work if field existed | ⚠️ **Partial** |

**Impact**: Cannot auto-resolve conflicts, cannot disable problematic mods at runtime, no conflict resolution mechanism.

**Implementation Priority**: 🔴 **CRITICAL** - Must be Phase 1

---

## 6. Multi-Slot APCapabilities (NEW - CRITICAL)

### 6.1 Intended Design

**Purpose**: Support multiple Palworld players in a single Archipelago multiworld.

**File Naming**:
```
APCapabilities_<slot_name>.json
```

**Source of Slot Name**:
```json
// framework_config.json
{
  "slot_name": "Player1"
}
```

**Generated Files**:
```
APCapabilities_Player1.json
APCapabilities_Player2.json
APCapabilities_P1.json
// etc. (one per player/slot)
```

**Generation Behavior**:
- Framework generates capability file with slot-specific name
- Log full absolute path when generating
- Never auto-delete old files (user can manually clean up)
- Each generation overwrites same-named file (e.g., if slot_name changes mid-session)

**AP World Usage**:
- Host collects `APCapabilities_*.json` files from ALL players
- Provides multiple files to world generator (one per Palworld slot)
- World merges capabilities and generates multiworld

**Example Multi-Player Setup**:
```
Player 1 (slot: "P1"):
  - Generates APCapabilities_P1.json
  - Includes mods: ChestShuffle, TechTree, Breeding

Player 2 (slot: "P2"):
  - Generates APCapabilities_P2.json
  - Includes mods: ChestShuffle, BossShuffle

Host:
  - Collects APCapabilities_P1.json and APCapabilities_P2.json
  - Runs AP generator with both files
  - Generates multiworld with distinct item pools per player
```

### 6.2 Current Implementation

**Status**: ❌ **COMPLETELY MISSING**

**What Exists**:
- ✅ Single capability generation works
- ✅ `slot_name` field exists in `framework_config.json`

**What's Missing**:
- ❌ **Hardcoded filename**: `APCapabilities.json` (no slot name)
- ❌ No dynamic filename generation
- ❌ No full path logging
- ❌ Single-slot assumption throughout codebase

**Current Code**:
```lua
-- APFramework/Scripts/main.lua:14
CAPABILITIES_PATH = "APCapabilities.json"  -- HARDCODED
```

### 6.3 Gap Analysis

| Feature | Intended | Current | Status |
|---------|----------|---------|--------|
| Slot-based filename | Required | Hardcoded single file | ❌ **Missing** |
| Dynamic naming | Required | Not implemented | ❌ **Missing** |
| Full path logging | Required | Relative path only | ❌ **Missing** |
| Multi-slot support | Required | Single slot only | ❌ **Missing** |
| File preservation | Required | Overwrites same file | ⚠️ **Partial** |

**Impact**: Cannot support multiple Palworld players in multiworld. All players would overwrite the same `APCapabilities.json` file.

**Files to Modify**:
- [APFramework/Scripts/main.lua](../../APFramework/Scripts/main.lua) - Dynamic filename
- [src/framework_core/src/capabilities_generator.cpp](../../src/framework_core/src/capabilities_generator.cpp) - Accept filename parameter

**Implementation Priority**: 🔴 **CRITICAL** - Must be Phase 1

---

## 7. Capabilities Validation (Zero-Tolerance)

### 7.1 Intended Design

**Purpose**: Ensure **ZERO CONFLICTS** before generation. Block generation if any issues detected.

**Validation Rules**:
1. **Item ID Collisions**: No two mods can claim same item ID
2. **Location ID Collisions**: No two mods can claim same location ID
3. **Region Name Collisions**: No two mods can define same region name
4. **Dependency Cycles**: No circular dependencies
5. **Missing Dependencies**: All dependencies must exist
6. **Incompatibility Conflicts**: Conflicting mods must be disabled
7. **Custom Field Conflicts**: Extensible validation for custom capabilities

**Behavior on Conflict**:
- **BLOCK** generation (do not create APCapabilities file)
- **LOG** all conflicts with details
- **BROADCAST** errors to all mods via IPC
- **DISPLAY** errors in UE4SS console (via framework mod)
- **REQUIRE** user/developer resolution before proceeding

**Error Message Example**:
```
[APFramework] [ERROR] Capability generation FAILED
[APFramework] [ERROR] Item ID collision detected:
  - Mod: author1.palworld.mod1 claims ID 100042 (name: "Super Pickaxe")
  - Mod: author2.palworld.mod2 claims ID 100042 (name: "Magic Sword")
[APFramework] [ERROR] Resolution: Mods must use unique ID ranges
[APFramework] [ERROR] APCapabilities.json NOT generated
```

### 7.2 Current Implementation

**Status**: ⚠️ **PARTIAL - NOT ZERO-TOLERANCE**

**What Exists**:
- ✅ `validate()` method in CapabilitiesGenerator
- ✅ Implicit collision detection via map structure (silent overwrites)

**What's Missing**:
- ❌ **Not zero-tolerance**: Validation returns errors but doesn't halt generation
- ❌ No explicit item ID collision checking
- ❌ No explicit location ID collision checking
- ❌ No explicit region name collision checking
- ❌ No dependency cycle detection
- ❌ No broadcast of errors to mods
- ❌ Generation proceeds even with conflicts

**Current Code**:
```cpp
// capabilities_generator.cpp
bool CapabilitiesGenerator::validate() {
    // Returns true/false but caller may ignore
    // No blocking behavior enforced
}
```

### 7.3 Gap Analysis

| Validation Type | Intended | Current | Status |
|----------------|----------|---------|--------|
| Item ID collision | Block generation | Passive check | ❌ **Not Enforced** |
| Location ID collision | Block generation | Passive check | ❌ **Not Enforced** |
| Region name collision | Block generation | Not checked | ❌ **Missing** |
| Dependency cycles | Block generation | Not checked | ❌ **Missing** |
| Missing dependencies | Block generation | Not checked | ❌ **Missing** |
| Incompatibility conflicts | Block generation | Not checked | ❌ **Missing** |
| Error broadcasting | Required | Not implemented | ❌ **Missing** |
| Zero-tolerance enforcement | Required | Not enforced | ❌ **CRITICAL** |

**Impact**: Conflicting capabilities can be generated, causing server errors or unpredictable behavior in AP multiworld.

**Files to Modify**:
- [src/framework_core/src/capabilities_generator.cpp](../../src/framework_core/src/capabilities_generator.cpp) - Enforce blocking
- [src/framework_core/src/framework_core.cpp](../../src/framework_core/src/framework_core.cpp) - Check validation before generation

**Implementation Priority**: 🔴 **HIGH** - Must be Phase 2

---

## 8. Log Routing to Framework Mod (NEW - CRITICAL)

### 8.1 Intended Design

**Purpose**: Display important framework logs in UE4SS console for user visibility.

**Architecture**:
```
C++ Logger
  ↓ (filter by log_mode)
IPC Message (type: "log")
  ↓
Lua Framework Mod (priority client)
  ↓ (print to console)
UE4SS Console
  ↓ (automatic)
ue4ss/UE4SS.log
```

**Log Verbosity Configuration**:
```json
// framework_config.json
{
  "log_mode": "framework_only",  // or "all", "minimal"
  "log_verbosity": "info",  // or "debug", "warning", "error"
}
```

**Log Modes**:
- `"minimal"`: Errors only
- `"framework_only"`: Framework INFO/WARNING/ERROR (no DEBUG, no mod logs)
- `"all"`: All logs from framework and mods

**Default**: `"framework_only"` with `"info"` verbosity

**Message Flow**:
1. C++ code calls `LOG_INFO("Message")`
2. Logger checks `log_mode` and `log_verbosity`
3. If important, send IPC message to `archipelago.palworld.framework`
4. Framework mod receives `{type: "log", level: "INFO", message: "..."}`
5. Framework mod prints: `print("[APFramework] [INFO] Message")`
6. UE4SS automatically writes to `UE4SS.log`

### 8.2 Current Implementation

**Status**: ❌ **COMPLETELY MISSING**

**What Exists**:
- ✅ Logger writes to file `framework.log`
- ✅ Lua mod can print to UE4SS console (basic)

**What's Missing**:
- ❌ No `log_mode` or `log_verbosity` in `framework_config.json`
- ❌ No IPC message type `"log"` for framework→mod communication
- ❌ No filtering logic in Logger
- ❌ No framework mod registration as priority client (can't receive logs)
- ❌ No message pump in framework mod to print logs

### 8.3 Gap Analysis

| Feature | Intended | Current | Status |
|---------|----------|---------|--------|
| log_mode config | Required | Not present | ❌ **Missing** |
| log_verbosity config | Required | Not present | ❌ **Missing** |
| IPC log messages | Required | Not implemented | ❌ **Missing** |
| Log filtering | Required | Not implemented | ❌ **Missing** |
| Framework mod receives logs | Required | Not implemented | ❌ **Missing** |
| Console printing | Required | Not implemented | ❌ **Missing** |

**Impact**: Users cannot see framework errors/warnings without checking `framework.log` file. Debugging is significantly harder.

**Files to Modify**:
- [APFramework/framework_config.json](../../APFramework/framework_config.json) - Add log_mode, log_verbosity
- [src/framework_core/src/logger.cpp](../../src/framework_core/src/logger.cpp) - Add IPC routing
- [src/framework_core/src/framework_core.cpp](../../src/framework_core/src/framework_core.cpp) - Handle "log" IPC messages
- [APFramework/Scripts/main.lua](../../APFramework/Scripts/main.lua) - Print received logs

**Implementation Priority**: 🔴 **HIGH** - Must be Phase 1

---

## 9. Lua JSON Integration (lunajson)

### 9.1 Intended Design

**Purpose**: Use lunajson throughout the Lua codebase for robust JSON handling.

**Why lunajson?**:
- Pure Lua implementation (no C dependencies)
- Handles all valid JSON (nested objects, arrays, escape sequences, unicode)
- Battle-tested and widely used
- Already tested successfully in this project
- Complements `nlohmann/json` in C++ codebase

**Use Cases in APFramework**:

**1. Client Library** ([src/lua_client/ap_client.lua](../../src/lua_client/ap_client.lua)):
```lua
local json = require("lunajson")
local client = ap_client:new("author.game.mod")

-- Automatic encoding (Lua table → JSON string)
client:register({
    items = {
        {id = 100000, name = "Item", classification = "useful"}
    },
    locations = {
        {id = 200000, name = "Location", region = "Region"}
    }
})

-- Automatic decoding (JSON string → Lua table)
client.on_item_received = function(item_data)
    print("Received item ID: " .. item_data.item_id)
end
```

**2. Configuration Management** ([APFramework/Scripts/config.lua](../../APFramework/Scripts/config.lua)):
```lua
local json = require("lunajson")

function Config:load(config_path)
    local file = io.open(config_path, "r")
    local content = file:read("*a")
    file:close()

    -- Clean JSON parsing instead of regex
    local config_data = json.decode(content)
    self.server = config_data.server or self.server
    self.port = config_data.port or self.port
    self.slot_name = config_data.slot_name or self.slot_name
    self.password = config_data.password or self.password
    self.autoconnect = config_data.autoconnect or self.autoconnect
    self.registration_timeout = config_data.registration_timeout or self.registration_timeout
end
```

**3. Any Future Lua Components**: Use lunajson for all JSON operations.

### 9.2 Current Implementation

**Status**: ❌ **NO lunajson - Multiple Custom Implementations**

**Problem Areas**:

**1. Client Library** ([src/lua_client/ap_client.lua](../../src/lua_client/ap_client.lua) lines 8-96):
- Custom JSON encoder/decoder using regex
- Limited to simple structures
- Cannot handle nested arrays/objects properly
- Cannot handle escape sequences or unicode

**2. Config Management** ([APFramework/Scripts/config.lua](../../APFramework/Scripts/config.lua) lines 31-37):
- Fragile regex parsing: `content:match('"server"%s*:%s*"([^"]+)"')`
- Hard-coded for each field
- Cannot handle nested config objects
- Breaks on comments or formatting changes
- **Example of brittleness**: Adding a new config field requires new regex pattern

**Current Code Examples**:
```lua
-- config.lua:31-37 (FRAGILE)
self.server = content:match('"server"%s*:%s*"([^"]+)"') or self.server
self.port = tonumber(content:match('"port"%s*:%s*(%d+)')) or self.port
self.slot_name = content:match('"slot_name"%s*:%s*"([^"]*)"') or self.slot_name
self.password = content:match('"password"%s*:%s*"([^"]*)"') or self.password
self.autoconnect = content:match('"autoconnect"%s*:%s*(true)') ~= nil
self.registration_timeout = tonumber(content:match('"registration_timeout"%s*:%s*(%d+)')) or self.registration_timeout

-- ap_client.lua:8-96 (FRAGILE)
-- Custom JSON implementation using string.match patterns
-- Cannot handle complex JSON structures
```

### 9.3 Gap Analysis

| Feature | Intended | Current | Status |
|---------|----------|---------|--------|
| lunajson library | Required | Not included | ❌ **Missing** |
| Client lib JSON | lunajson | Custom regex | ❌ **Fragile** |
| Config JSON parsing | lunajson | Regex patterns | ❌ **Fragile** |
| Robust encoding/decoding | Required | Limited | ❌ **Incomplete** |
| Nested structures | Supported | Breaks | ❌ **Broken** |
| Unicode/escapes | Supported | Not handled | ❌ **Missing** |

**Impact**:
- Complex mod capabilities fail to serialize
- Config files with nested objects break
- Mods with unicode characters in names/descriptions fail
- Difficult to debug (silent failures or cryptic errors)
- Maintenance burden (custom code vs proven library)

**Files to Modify**:
1. Add `lunajson.lua` (or `lunajson/` directory) to project
2. [src/lua_client/ap_client.lua](../../src/lua_client/ap_client.lua) - Replace lines 8-96 with lunajson
3. [APFramework/Scripts/config.lua](../../APFramework/Scripts/config.lua) - Replace lines 31-37 with lunajson
4. Update any other Lua files that parse/generate JSON

**Benefits of lunajson**:
- ✅ Handles ALL valid JSON (proven library)
- ✅ Cleaner code (remove hundreds of lines of regex)
- ✅ Easier maintenance (update library, not custom code)
- ✅ Better error messages (library provides detailed parse errors)
- ✅ Future-proof (can handle schema changes without code changes)

**Implementation Priority**: 🟡 **HIGH** - Phase 1 or 2 (bundled with client lib work)

---

## 10. AP World Python Support

### 10.1 Intended Design

**Purpose**: Python AP world must read and adapt to `APCapabilities_<slot>.json` files.

**Features**:
- Read multiple capability files (one per Palworld slot)
- Parse mod capabilities (items, locations, regions)
- Generate multiworld with dynamic content
- Validate capabilities (warnings for conflicts)
- Log mod names for debugging

### 10.2 Current Implementation

**Status**: ✅ **ALREADY IMPLEMENTED (ADVANCED)**

**What Exists**:
- ✅ Python world code in [worlds/palworld/](../../worlds/palworld/)
- ✅ ModCapabilityManager in [mod_interface.py](../../worlds/palworld/mod_interface.py)
- ✅ Dynamic capability loading from `APCapabilities.json`
- ✅ Mod metadata parsing (id, name, version, author, description)
- ✅ Conflict declarations supported
- ✅ Extensible schema (base + custom fields)

**Code Evidence**:
```python
# worlds/palworld/__init__.py:88-110
def _load_capability_manifest(self):
    manifest_path = self.options.capability_manifest_path.value or "APCapabilities.json"
    self.capability_manifest = self.capability_manager.load_manifest(manifest_path)
    if self.capability_manifest:
        self._merge_mod_capabilities()
```

**Mod Interface**:
```python
# worlds/palworld/mod_interface.py
@dataclass
class ModInfo:
    id: str
    name: str
    version: str
    author: Optional[str]
    description: Optional[str]

@dataclass
class ConflictDeclaration:
    mod_id: str
    reason: str
```

### 10.3 Gap Analysis

| Feature | Intended | Current | Status |
|---------|----------|---------|--------|
| Read APCapabilities.json | Required | Implemented | ✅ **Complete** |
| Dynamic capability loading | Required | Implemented | ✅ **Complete** |
| Mod metadata parsing | Required | Implemented | ✅ **Complete** |
| Conflict detection | Required | Implemented | ✅ **Complete** |
| Multi-slot support | Required | Needs testing | ⚠️ **Unclear** |
| Custom field extensibility | Required | Implemented | ✅ **Complete** |

**Python World is MORE Advanced Than Framework!**

**Potential Issues**:
- Python expects single file path from options, may need multi-file support
- Dependency schema may not match (need to verify)
- Incompatibility schema may not match (need to verify)

**Files to Check**:
- [worlds/palworld/options.py](../../worlds/palworld/options.py) - Check if multi-file option exists
- [worlds/palworld/mod_interface.py](../../worlds/palworld/mod_interface.py) - Verify schema matches C++ schema

**Implementation Priority**: 🟡 **MEDIUM** - Phase 2 verification/update

---

## 11. Missing Features Summary

### Critical Missing Features (RED - Phase 1)

1. **Framework Mod Dual-Role** (🔴 CRITICAL)
   - Framework mod must register as priority client
   - Must use both framework lib AND client lib
   - Mod ID: `archipelago.palworld.framework`
   - **Impact**: Cannot demonstrate client library, cannot receive framework logs

2. **Dependency System** (🔴 CRITICAL)
   - Hard requirements enforcement
   - Registration denial if missing
   - Cascade disabling
   - **Impact**: Mods load without dependencies → crashes

3. **Incompatibility System** (🔴 CRITICAL)
   - Three incompatibility types
   - Auto-disable conflicting mods
   - Mutual incompatibility detection
   - **Impact**: Conflicting mods run together → crashes/corruption

4. **Runtime Enablement** (🔴 CRITICAL)
   - Editable `enabled` field in config
   - In-memory + file persistence
   - Auto-disable on conflicts
   - **Impact**: No conflict resolution mechanism

5. **Multi-Slot APCapabilities** (🔴 CRITICAL)
   - Filename: `APCapabilities_<slot>.json`
   - Dynamic naming from config
   - Full path logging
   - **Impact**: Cannot support multiple Palworld players

6. **Log Routing to Framework Mod** (🔴 HIGH)
   - IPC log messages
   - log_mode / log_verbosity config
   - Framework mod prints to UE4SS console
   - **Impact**: Users can't see errors → debugging impossible

### Important Missing Features (YELLOW - Phase 2)

7. **Zero-Tolerance Capabilities Validation** (🟡 HIGH)
   - Block generation on ANY conflict
   - Explicit collision checking
   - Error broadcasting
   - **Impact**: Invalid capabilities can be generated

8. **lunajson Integration** (🟡 MEDIUM)
   - Replace custom JSON implementation
   - Robust parsing for complex structures
   - **Impact**: Fragile JSON handling

9. **Python World Schema Verification** (🟡 MEDIUM)
   - Verify multi-slot support
   - Verify dependency/incompatibility schema match
   - **Impact**: Framework and world may be incompatible

### Nice-to-Have Features (GREEN - Phase 3)

10. **Advanced Error Handling**
11. **Semantic Version Validation**
12. **Static .lib Build Option**

---

## 12. Strengths of Current Implementation

Despite significant gaps, the current implementation has strong foundations:

### Production-Quality Core

- ✅ Two separate libraries (correct architecture)
- ✅ Clean C++ codebase with proper threading
- ✅ Real apclientpp integration (WebSocket AP connection)
- ✅ Named Pipes IPC (reliable, tested)
- ✅ Comprehensive file logging
- ✅ Lua C API bindings (UE4SS compatible)

### Advanced Python World

- ✅ Dynamic capability loading (ahead of framework!)
- ✅ Mod metadata support
- ✅ Conflict declarations
- ✅ Extensible schema
- ✅ Well-structured code

### Solid Foundation

- ✅ All major components exist
- ✅ Message routing works
- ✅ Client library exists (just needs integration)
- ✅ Basic validation exists (needs enforcement)

---

## 13. Implementation Priorities

### Phase 1: Critical Safety Features (MUST HAVE)

**Priority Order**:
1. Multi-slot APCapabilities (easiest, enables multi-player)
2. Dependency system (prevents missing dependency crashes)
3. Incompatibility system (prevents conflict crashes)
4. Runtime enablement (enables auto-resolution)
5. Log routing to framework mod (critical for debugging)
6. Framework mod dual-role (demonstrates client lib usage)

**Estimated Time**: 5-7 days

### Phase 2: Validation & Polish (SHOULD HAVE)

7. Zero-tolerance capabilities validation
8. lunajson integration
9. Python world schema verification
10. Comprehensive testing

**Estimated Time**: 3-4 days

### Phase 3: Advanced Features (NICE TO HAVE)

11. Semantic version validation
12. Static .lib option
13. Advanced error handling

**Estimated Time**: 3-4 days

**Total**: 11-15 days for complete implementation

---

## 14. Conclusion

### Current State

The APFramework has a **solid architectural foundation** with correct library separation, but is **missing critical safety systems** that are essential for the intended design:

- **Architecture**: 70% aligned (library separation correct, dual-role missing)
- **Features**: 45% complete (core works, safety systems missing)
- **Quality**: High-quality code, but incomplete

### Critical Gaps

The **5 critical gaps** that must be addressed:
1. Framework mod dual-role (priority client)
2. Dependency system (hard requirements)
3. Incompatibility system (conflict prevention)
4. Runtime enablement (conflict resolution)
5. Multi-slot capabilities (multi-player support)

### Path Forward

**Phase 1** implements the critical safety features that make the framework production-ready. Without these features, the framework:
- Cannot prevent mod conflicts
- Cannot support multiple players
- Cannot resolve dependency issues
- Cannot display errors to users

**Phase 2** adds validation enforcement and polish.

**Phase 3** adds advanced features for long-term maturity.

### Recommendation

**Do NOT release** until Phase 1 is complete. The missing features are not "nice-to-have" - they are **critical for safety and usability**. Releasing without them will result in:
- User frustration (can't see errors)
- Mod conflicts causing crashes
- Inability to support multi-player
- Poor developer experience

Complete Phase 1 first (5-7 days), then release for beta testing.

---

**End of Analysis**