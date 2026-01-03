# Phase 1: Critical Safety Features - Implementation Plan

**Document Version**: 1.0
**Date**: 2026-01-02
**Status**: Ready for Implementation
**Parent Document**: [REDESIGN_PLAN_OVERVIEW.md](../REDESIGN_PLAN_OVERVIEW.md)

---

## Overview

Phase 1 implements the **7 critical safety features** that are absolutely required for a production-ready release. Without these features, the framework cannot:
- Prevent mod conflicts (crashes/corruption)
- Support multiple Palworld players (multi-player limitation)
- Display errors to users (blind debugging)
- Enforce mod dependencies (missing requirements)
- Resolve conflicts automatically (no conflict resolution)
- Handle complex JSON (fragile parsing)

**Total Estimated Time**: 39-52 hours (5-7 days)

---

## Feature Roadmap

### Recommended Implementation Order

1. **lunajson integration** (4-6 hours) → Improves all subsequent Lua work
2. **Multi-slot capabilities** (3-4 hours) → Easy win, enables multi-player
3. **Framework mod dual-role** (4-6 hours) → Required for log routing
4. **Dependency system** (8-10 hours) → Foundation for safety
5. **Incompatibility system** (8-10 hours) → Foundation for safety
6. **Runtime enablement** (6-8 hours) → Depends on dependencies/incompatibilities
7. **Log routing** (6-8 hours) → Depends on framework mod dual-role

**Rationale**:
- Start with lunajson (cleanest code for subsequent features)
- Do multi-slot early (simple, high value)
- Dual-role before log routing (dependency)
- Dependencies and incompatibilities before enablement (dependencies)
- Log routing last (depends on dual-role)

---

## Feature 1.7: lunajson Integration

**Priority**: 1st (Do First)
**Estimate**: 4-6 hours
**Dependencies**: None

### Problem Statement

Current Lua code uses fragile custom JSON implementations:

**Problem Area 1**: [src/lua_client/ap_client.lua](../../../../src/lua_client/ap_client.lua) lines 8-96
- Custom regex-based JSON encoder/decoder
- Cannot handle nested arrays/objects properly
- Cannot handle unicode or escape sequences
- Hundreds of lines of complex regex patterns

**Problem Area 2**: [APFramework/Scripts/config.lua](../../../../APFramework/Scripts/config.lua) lines 31-37
- Hard-coded regex patterns for each config field
- Breaks on format changes or comments
- Cannot handle nested config objects
- Example: `self.server = content:match('"server"%s*:%s*"([^"]+)"') or self.server`

### Solution

Replace all custom JSON with **lunajson** library:
- Pure Lua implementation (no C dependencies)
- Handles ALL valid JSON (nested, unicode, escapes)
- Battle-tested and widely used
- Already tested successfully in this project
- Complements `nlohmann/json` in C++ codebase

### Implementation Steps

#### Step 1: Add lunajson to Project

**File**: `third_party/lunajson.lua` (or `third_party/lunajson/` directory)

**Option A**: Single-file lunajson
```lua
-- Download lunajson.lua from: https://github.com/grafi-tt/lunajson
-- Place in: third_party/lunajson.lua
```

**Option B**: Multi-file lunajson (preferred for maintainability)
```
third_party/lunajson/
  ├── init.lua       (main module)
  ├── decoder.lua    (JSON → Lua)
  ├── encoder.lua    (Lua → JSON)
  └── sax.lua        (SAX parser)
```

**Estimate**: 30 minutes (download + verify)

---

#### Step 2: Update Client Library (ap_client.lua)

**File**: [src/lua_client/ap_client.lua](../../../../src/lua_client/ap_client.lua)

**Current Code** (lines 8-96):
```lua
-- Custom JSON encoder/decoder using regex
-- ~90 lines of complex pattern matching
```

**New Code**:
```lua
-- Add to top of file
local json = require("lunajson")

-- In APClient class
function APClient:new(mod_id)
    local instance = setmetatable({}, {__index = self})
    instance.mod_id = mod_id
    instance.ipc_client = nil  -- Will be initialized later
    instance.callbacks = {}
    return instance
end

-- Registration with automatic encoding
function APClient:register(capabilities)
    if not self.ipc_client then
        error("IPC client not connected. Call connect() first.")
    end

    -- Build registration message
    local message = {
        type = "register",
        mod_id = self.mod_id,
        capabilities = capabilities
    }

    -- Automatic JSON encoding
    local json_str = json.encode(message)
    self.ipc_client:send(json_str)
end

-- Receive with automatic decoding
function APClient:poll_messages()
    if not self.ipc_client then
        return
    end

    local raw_messages = self.ipc_client:receive()
    for _, raw_msg in ipairs(raw_messages) do
        -- Automatic JSON decoding
        local ok, message = pcall(json.decode, raw_msg)
        if ok then
            self:_handle_message(message)
        else
            print("[APClient] [ERROR] Failed to parse message: " .. raw_msg)
        end
    end
end

-- Internal message handler
function APClient:_handle_message(message)
    local msg_type = message.type

    if msg_type == "item_received" and self.callbacks.on_item_received then
        self.callbacks.on_item_received(message.data)
    elseif msg_type == "location_checked" and self.callbacks.on_location_checked then
        self.callbacks.on_location_checked(message.data)
    elseif msg_type == "log" and self.callbacks.on_log then
        self.callbacks.on_log(message.level, message.message)
    elseif msg_type == "error" and self.callbacks.on_error then
        self.callbacks.on_error(message.error)
    end
end

-- Callback registration
function APClient:on_item_received(callback)
    self.callbacks.on_item_received = callback
end

function APClient:on_location_checked(callback)
    self.callbacks.on_location_checked = callback
end

function APClient:on_log(callback)
    self.callbacks.on_log = callback
end

function APClient:on_error(callback)
    self.callbacks.on_error = callback
end
```

**Changes**:
- **Remove**: Lines 8-96 (custom JSON implementation)
- **Add**: `require("lunajson")` at top
- **Update**: All `encode_json()` calls → `json.encode()`
- **Update**: All `decode_json()` calls → `json.decode()`
- **Add**: Error handling for JSON parse failures

**Estimate**: 2 hours (rewrite + test)

---

#### Step 3: Update Config Management (config.lua)

**File**: [APFramework/Scripts/config.lua](../../../../APFramework/Scripts/config.lua)

**Current Code** (lines 31-37):
```lua
-- Hard-coded regex for each field
self.server = content:match('"server"%s*:%s*"([^"]+)"') or self.server
self.port = tonumber(content:match('"port"%s*:%s*(%d+)')) or self.port
self.slot_name = content:match('"slot_name"%s*:%s*"([^"]*)"') or self.slot_name
self.password = content:match('"password"%s*:%s*"([^"]*)"') or self.password
self.autoconnect = content:match('"autoconnect"%s*:%s*(true)') ~= nil
self.registration_timeout = tonumber(content:match('"registration_timeout"%s*:%s*(%d+)')) or self.registration_timeout
```

**New Code**:
```lua
-- Add to top of file
local json = require("lunajson")

-- Config class
local Config = {}
Config.__index = Config

function Config:new()
    local instance = setmetatable({}, Config)

    -- Default values
    instance.server = "localhost"
    instance.port = 38281
    instance.slot_name = ""
    instance.password = ""
    instance.autoconnect = true
    instance.registration_timeout = 30000
    instance.log_mode = "framework_only"  -- NEW
    instance.log_verbosity = "info"       -- NEW

    return instance
end

function Config:load(config_path)
    local file, err = io.open(config_path, "r")
    if not file then
        print("[Config] [WARNING] Could not open config file: " .. config_path)
        print("[Config] [WARNING] Using default configuration")
        return false
    end

    local content = file:read("*a")
    file:close()

    -- Parse JSON cleanly
    local ok, config_data = pcall(json.decode, content)
    if not ok then
        print("[Config] [ERROR] Failed to parse config JSON: " .. config_data)
        print("[Config] [WARNING] Using default configuration")
        return false
    end

    -- Apply config values (with defaults for missing fields)
    self.server = config_data.server or self.server
    self.port = config_data.port or self.port
    self.slot_name = config_data.slot_name or self.slot_name
    self.password = config_data.password or self.password
    self.autoconnect = config_data.autoconnect ~= nil and config_data.autoconnect or self.autoconnect
    self.registration_timeout = config_data.registration_timeout or self.registration_timeout
    self.log_mode = config_data.log_mode or self.log_mode
    self.log_verbosity = config_data.log_verbosity or self.log_verbosity

    print("[Config] [INFO] Configuration loaded successfully")
    return true
end

function Config:save(config_path)
    local config_data = {
        server = self.server,
        port = self.port,
        slot_name = self.slot_name,
        password = self.password,
        autoconnect = self.autoconnect,
        registration_timeout = self.registration_timeout,
        log_mode = self.log_mode,
        log_verbosity = self.log_verbosity
    }

    local json_str = json.encode(config_data)

    local file, err = io.open(config_path, "w")
    if not file then
        print("[Config] [ERROR] Could not save config file: " .. config_path)
        return false
    end

    file:write(json_str)
    file:close()

    print("[Config] [INFO] Configuration saved successfully")
    return true
end

return Config
```

**Changes**:
- **Remove**: Lines 31-37 (regex patterns)
- **Add**: `require("lunajson")` at top
- **Update**: `load()` method to use `json.decode()`
- **Add**: `save()` method for config persistence (useful for runtime changes)
- **Add**: Error handling for JSON parse failures
- **Add**: Default values for new log_mode and log_verbosity fields

**Estimate**: 1 hour (rewrite + test)

---

#### Step 4: Update Package Path

**File**: [APFramework/Scripts/main.lua](../../../../APFramework/Scripts/main.lua)

**Add at top** (before any requires):
```lua
-- Add third_party to Lua package path
local script_dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)")
package.path = package.path .. ";" .. script_dir .. "../../third_party/?.lua"
package.path = package.path .. ";" .. script_dir .. "../../third_party/?/init.lua"
```

**Estimate**: 15 minutes

---

#### Step 5: Testing

**Test Cases**:

1. **Simple JSON**: `{\"key\": \"value\"}`
2. **Nested JSON**: `{\"outer\": {\"inner\": \"value\"}}`
3. **Arrays**: `{\"items\": [1, 2, 3]}`
4. **Unicode**: `{\"name\": \"日本語\"}` (Japanese characters)
5. **Escapes**: `{\"path\": \"C:\\\\Users\\\\Name\"}`
6. **Boolean/Null**: `{\"enabled\": true, \"data\": null}`

**Test Script** (create `test_lunajson.lua`):
```lua
local json = require("lunajson")

-- Test cases
local test_cases = {
    {name = "Simple", data = {key = "value"}},
    {name = "Nested", data = {outer = {inner = "value"}}},
    {name = "Array", data = {items = {1, 2, 3}}},
    {name = "Unicode", data = {name = "日本語"}},
    {name = "Boolean", data = {enabled = true, disabled = false}},
}

print("Testing lunajson integration...")
for _, test in ipairs(test_cases) do
    print("\nTest: " .. test.name)

    -- Encode
    local encoded = json.encode(test.data)
    print("  Encoded: " .. encoded)

    -- Decode
    local decoded = json.decode(encoded)
    print("  Decoded: " .. tostring(decoded))

    -- Verify
    if type(decoded) == "table" then
        print("  ✓ Success")
    else
        print("  ✗ Failed")
    end
end
```

**Estimate**: 1 hour (testing + fixes)

---

### Files Modified

- `third_party/lunajson.lua` (or `third_party/lunajson/`) - **NEW**
- [src/lua_client/ap_client.lua](../../../../src/lua_client/ap_client.lua) - **MAJOR REWRITE**
- [APFramework/Scripts/config.lua](../../../../APFramework/Scripts/config.lua) - **MAJOR REWRITE**
- [APFramework/Scripts/main.lua](../../../../APFramework/Scripts/main.lua) - **MINOR** (package path)

**Estimated Lines Changed**: ~200-250 lines (mostly deletions)

---

### Success Criteria

- [ ] lunajson library added to project
- [ ] All custom JSON code removed from ap_client.lua
- [ ] All regex patterns removed from config.lua
- [ ] Complex JSON structures (nested objects, arrays) parse correctly
- [ ] Unicode characters handled correctly
- [ ] Escape sequences handled correctly
- [ ] Error messages displayed for invalid JSON
- [ ] All existing functionality works with lunajson

---

## Feature 1.5: Multi-Slot APCapabilities

**Priority**: 2nd (Easy Win)
**Estimate**: 3-4 hours
**Dependencies**: lunajson (recommended but not required)

### Problem Statement

**Current Implementation**: Hardcoded filename `APCapabilities.json`

**File**: [APFramework/Scripts/main.lua](../../../../APFramework/Scripts/main.lua) line 14
```lua
CAPABILITIES_PATH = "APCapabilities.json"  -- HARDCODED
```

**Issue**: Cannot support multiple Palworld players in same Archipelago multiworld. If two players generate capabilities, they overwrite each other's files.

### Solution

Use **slot-based filenames**: `APCapabilities_<slot_name>.json`

**Source of slot name**: `framework_config.json`
```json
{
  "slot_name": "Player1"
}
```

**Generated files**:
```
APCapabilities_Player1.json
APCapabilities_Player2.json
APCapabilities_P1.json
```

### Implementation Steps

#### Step 1: Update framework_config.json

**File**: [APFramework/framework_config.json](../../../../APFramework/framework_config.json)

**Current**:
```json
{
  "server": "localhost",
  "port": 38281,
  "slot_name": "",
  "password": "",
  "autoconnect": true,
  "registration_timeout": 30000
}
```

**Add** (already handled by lunajson integration if done first):
```json
{
  "server": "localhost",
  "port": 38281,
  "slot_name": "Player1",
  "password": "",
  "autoconnect": true,
  "registration_timeout": 30000,
  "log_mode": "framework_only",
  "log_verbosity": "info"
}
```

**Note**: `slot_name` should have a default value that's non-empty.

**Estimate**: 5 minutes

---

#### Step 2: Update main.lua to Generate Dynamic Filename

**File**: [APFramework/Scripts/main.lua](../../../../APFramework/Scripts/main.lua)

**Current** (line 14):
```lua
CAPABILITIES_PATH = "APCapabilities.json"
```

**Replace with**:
```lua
-- Function to build capabilities filename
local function build_capabilities_filename(config)
    local slot_name = config.slot_name

    if slot_name == "" or slot_name == nil then
        print("[APFramework] [WARNING] No slot_name in config, using default 'Player1'")
        slot_name = "Player1"
    end

    local filename = "APCapabilities_" .. slot_name .. ".json"
    print("[APFramework] [INFO] Capabilities filename: " .. filename)

    return filename
end

-- Later in initialization
local config = Config:new()
config:load("framework_config.json")

local CAPABILITIES_PATH = build_capabilities_filename(config)
```

**Changes**:
- **Remove**: Hardcoded `CAPABILITIES_PATH` constant
- **Add**: `build_capabilities_filename()` function
- **Update**: Generate filename dynamically from config
- **Add**: Warning if slot_name is missing
- **Add**: Log filename for user visibility

**Estimate**: 30 minutes

---

#### Step 3: Log Full Absolute Path

**File**: [APFramework/Scripts/main.lua](../../../../APFramework/Scripts/main.lua)

**Add to capabilities generation**:
```lua
-- In generate_capabilities() function
local function generate_capabilities()
    print("[APFramework] [INFO] Generating capabilities...")

    local capabilities = framework_core.generate_capabilities()

    -- Get absolute path for logging
    local absolute_path = lfs.currentdir() .. "/" .. CAPABILITIES_PATH

    print("[APFramework] [INFO] Writing capabilities to: " .. absolute_path)

    -- Write to file
    local file = io.open(CAPABILITIES_PATH, "w")
    if file then
        file:write(capabilities)
        file:close()
        print("[APFramework] [INFO] Capabilities generated successfully")
        print("[APFramework] [INFO] File: " .. absolute_path)
    else
        print("[APFramework] [ERROR] Failed to write capabilities file")
    end
end
```

**Note**: May need to add `lfs = require("lfs")` if not already present, or use alternative method to get current directory.

**Estimate**: 30 minutes

---

#### Step 4: Update C++ Bindings (Optional)

**File**: [src/framework_core/src/capabilities_generator.cpp](../../../../src/framework_core/src/capabilities_generator.cpp)

**Current**: Capabilities generator doesn't know about filename (Lua handles file writing)

**Option A**: Keep as-is (Lua handles filename and file writing)
- **Pros**: Simple, no C++ changes needed
- **Cons**: Filename logic split between Lua and C++

**Option B**: Pass filename to C++ (cleaner architecture)
- **Pros**: C++ handles complete capability generation
- **Cons**: Requires C++ changes and Lua binding updates

**Recommendation**: **Option A** (keep as-is) for Phase 1, consider Option B in Phase 2

**Estimate**: 0 hours (defer to Phase 2 if needed)

---

#### Step 5: Testing

**Test Cases**:

1. **Default slot name**: Config has `"slot_name": "Player1"` → generates `APCapabilities_Player1.json`
2. **Custom slot name**: Config has `"slot_name": "P2"` → generates `APCapabilities_P2.json`
3. **Empty slot name**: Config has `"slot_name": ""` → warns, uses default `Player1`
4. **Special characters**: Config has `"slot_name": "Player_1-A"` → generates `APCapabilities_Player_1-A.json`
5. **Multiple generations**: Generate twice with same slot name → overwrites same file
6. **Different slots**: Change slot name, generate again → creates new file (old file preserved)

**Test Script**:
```lua
-- Test multi-slot capabilities
local config1 = {slot_name = "Player1"}
local config2 = {slot_name = "Player2"}
local config3 = {slot_name = ""}

print("Test 1: " .. build_capabilities_filename(config1))  -- APCapabilities_Player1.json
print("Test 2: " .. build_capabilities_filename(config2))  -- APCapabilities_Player2.json
print("Test 3: " .. build_capabilities_filename(config3))  -- APCapabilities_Player1.json (default)
```

**Estimate**: 1 hour (testing + edge cases)

---

### Files Modified

- [APFramework/framework_config.json](../../../../APFramework/framework_config.json) - **MINOR** (add default slot_name)
- [APFramework/Scripts/main.lua](../../../../APFramework/Scripts/main.lua) - **MODERATE** (dynamic filename generation)

**Estimated Lines Changed**: ~30-40 lines

---

### Success Criteria

- [ ] Capabilities filename uses slot_name from config
- [ ] Default slot_name used if config value is empty
- [ ] Full absolute path logged when generating capabilities
- [ ] Multiple players can generate distinct capability files
- [ ] Old capability files preserved when generating new ones
- [ ] Filename logged clearly for user to find file

---

## Feature 1.1: Framework Mod Dual-Role

**Priority**: 3rd (Required for Log Routing)
**Estimate**: 4-6 hours
**Dependencies**: lunajson

### Problem Statement

**Current**: Framework mod only uses `APFrameworkCore.dll` (framework library)

**Missing**:
- Framework mod doesn't use `ap_client.lua` (client library)
- Framework mod doesn't register as a client
- Framework mod cannot receive IPC messages from framework
- Cannot demonstrate client library usage to mod developers

### Solution

Framework mod should act as **both server AND priority client**:

**Dual Role**:
1. **Server** (via `APFrameworkCore.dll`): Runs IPC server, AP client, mod registry
2. **Priority Client** (via `ap_client.lua`): Registers with special mod_id, receives logs/status

**Special mod_id**: `archipelago.palworld.framework`

**Benefits**:
- Framework mod receives log messages via IPC (enables log routing)
- Framework mod receives status updates (AP connection, mod registration)
- Demonstrates client library usage (reference for mod developers)
- Framework mod can participate in AP session as a mod (future)

### Implementation Steps

#### Step 1: Load Client Library in Framework Mod

**File**: [APFramework/Scripts/main.lua](../../../../APFramework/Scripts/main.lua)

**Current**:
```lua
-- Load framework core (C++ bindings)
local framework_core = require("framework_core")
```

**Add**:
```lua
-- Load framework core (C++ bindings)
local framework_core = require("framework_core")

-- Load client library (Lua wrapper)
local APClient = require("ap_client")
```

**Estimate**: 5 minutes

---

#### Step 2: Register Framework Mod as Priority Client

**File**: [APFramework/Scripts/main.lua](../../../../APFramework/Scripts/main.lua)

**Add after framework initialization**:
```lua
-- Framework mod client instance
local framework_client = nil

-- Function to register framework mod as priority client
local function register_framework_client()
    print("[APFramework] [INFO] Registering framework mod as priority client...")

    framework_client = APClient:new("archipelago.palworld.framework")

    -- Connect to IPC server
    local connected = framework_client:connect()
    if not connected then
        print("[APFramework] [ERROR] Failed to connect framework client to IPC server")
        return false
    end

    -- Register with empty capabilities (framework mod provides no items/locations)
    framework_client:register({
        items = {},
        locations = {},
        regions = {}
    })

    print("[APFramework] [INFO] Framework mod registered successfully")
    return true
end
```

**Timing**: Call after IPC server starts
```lua
-- Main initialization
local function initialize()
    -- Load config
    local config = Config:new()
    config:load("framework_config.json")

    -- Start IPC server
    framework_core.start_ipc()
    print("[APFramework] [INFO] IPC server started")

    -- Register framework mod as priority client
    -- (slight delay to ensure server is ready)
    ExecuteWithDelay(500, function()
        register_framework_client()
    end)

    -- ... rest of initialization
end
```

**Estimate**: 1 hour

---

#### Step 3: Handle Self-Registration in C++

**File**: [src/framework_core/src/framework_core.cpp](../../../../src/framework_core/src/framework_core.cpp)

**Problem**: Framework core may reject registration from `archipelago.palworld.framework` because it's trying to register with itself.

**Solution**: Add special handling for framework mod_id

**In registration handler**:
```cpp
void FrameworkCore::handle_registration(const std::string& mod_id, const json& capabilities) {
    LOG_INFO("Registration request from: " + mod_id);

    // Special handling for framework mod (allow self-registration)
    if (mod_id == "archipelago.palworld.framework") {
        LOG_INFO("Framework mod registering as priority client");

        // Add to registered mods (but skip capability merging)
        ModMetadata metadata;
        metadata.mod_id = mod_id;
        metadata.display_name = "AP Framework";
        metadata.version = "1.0.0";
        metadata.enabled = true;
        metadata.is_framework_mod = true;  // Special flag

        registered_mods_[mod_id] = metadata;

        // Send registration success
        json response;
        response["type"] = "registration_success";
        response["mod_id"] = mod_id;

        message_router_->send_to_mod(mod_id, response.dump());

        LOG_INFO("Framework mod registered successfully");
        return;
    }

    // Normal registration logic for other mods
    // ...
}
```

**Changes**:
- **Add**: `is_framework_mod` flag to `ModMetadata` struct
- **Add**: Special handling for `archipelago.palworld.framework` mod_id
- **Update**: Skip capability merging for framework mod (empty capabilities)
- **Ensure**: Framework mod can send and receive IPC messages

**Estimate**: 1.5 hours

---

#### Step 4: Update ModMetadata Struct

**File**: [src/framework_core/include/mod_registry.h](../../../../src/framework_core/include/mod_registry.h)

**Add to ModMetadata**:
```cpp
struct ModMetadata {
    std::string mod_id;
    std::string version;
    std::string display_name;
    std::string description;
    bool enabled = true;
    bool is_framework_mod = false;  // NEW

    // Dependencies, incompatibilities (Phase 1 features 1.2, 1.3)
    std::map<std::string, VersionRange> dependencies;
    std::map<std::string, IncompatibilitySpec> incompatible_mods;

    // Capabilities
    json capabilities;
};
```

**Estimate**: 15 minutes

---

#### Step 5: Setup Message Polling in Framework Mod

**File**: [APFramework/Scripts/main.lua](../../../../APFramework/Scripts/main.lua)

**Add to main loop**:
```lua
-- Main loop (called every tick)
local function on_tick()
    if not framework_client then
        return
    end

    -- Poll for messages from framework
    framework_client:poll_messages()
end

-- Register tick callback
RegisterHook("/Script/Engine.PlayerController:PlayerTick", on_tick)
```

**Estimate**: 30 minutes

---

#### Step 6: Add Callbacks for Framework Client

**File**: [APFramework/Scripts/main.lua](../../../../APFramework/Scripts/main.lua)

**Add callbacks**:
```lua
-- Setup framework client callbacks
local function setup_framework_callbacks()
    if not framework_client then
        return
    end

    -- Log messages callback (for Feature 1.6)
    framework_client:on_log(function(level, message)
        print("[APFramework] [" .. level .. "] " .. message)
    end)

    -- Error messages callback
    framework_client:on_error(function(error_msg)
        print("[APFramework] [ERROR] " .. error_msg)
    end)

    -- Status updates callback (future)
    framework_client:on_status(function(status_data)
        -- Handle status updates (AP connected, mods registered, etc.)
    end)
end

-- Call after registration
local function register_framework_client()
    framework_client = APClient:new("archipelago.palworld.framework")
    framework_client:connect()
    framework_client:register({items = {}, locations = {}, regions = {}})

    setup_framework_callbacks()
end
```

**Estimate**: 30 minutes

---

### Files Modified

- [APFramework/Scripts/main.lua](../../../../APFramework/Scripts/main.lua) - **MODERATE** (load client, register, poll)
- [src/framework_core/src/framework_core.cpp](../../../../src/framework_core/src/framework_core.cpp) - **MODERATE** (self-registration handling)
- [src/framework_core/include/mod_registry.h](../../../../src/framework_core/include/mod_registry.h) - **MINOR** (add is_framework_mod flag)

**Estimated Lines Changed**: ~150-200 lines

---

### Success Criteria

- [ ] Framework mod loads both framework_core and ap_client
- [ ] Framework mod successfully registers as priority client
- [ ] Special mod_id `archipelago.palworld.framework` handled correctly
- [ ] Framework mod can send and receive IPC messages
- [ ] Framework mod polls for messages every tick
- [ ] Callbacks set up for log messages and errors
- [ ] No self-registration errors or deadlocks

---

## Feature 1.2: Dependency System

**Priority**: 4th (Safety Foundation)
**Estimate**: 8-10 hours
**Dependencies**: lunajson

### Problem Statement

**Current**: No dependency enforcement. Mods can register without their required dependencies, leading to crashes or broken functionality.

**Missing**:
- No `dependencies` field in mod metadata
- No dependency validation during discovery
- No registration denial for missing dependencies
- No cascade disabling (Mod A disabled → Mod B depending on A disabled)

### Solution

Implement **hard dependency requirements**:

**Schema**:
```json
"dependencies": {
  "required.mod.id": {"min_version": "1.0.0", "max_version": "2.0.0"},
  "another.required.mod": true  // Any version
}
```

**Behavior**:
- Parse dependencies during mod discovery
- Build dependency graph
- Check all dependencies exist and satisfy version constraints
- **DENY registration** if dependencies missing or wrong version
- **Cascade disable**: If Mod A disabled, disable all mods depending on A
- Send IPC error to mod explaining why registration denied

### Implementation Steps

#### Step 1: Define Dependency Schema

**File**: [src/framework_core/include/mod_registry.h](../../../../src/framework_core/include/mod_registry.h)

**Add structs**:
```cpp
// Version range for dependency specification
struct VersionRange {
    std::string min_version;  // Empty = no minimum
    std::string max_version;  // Empty = no maximum
    bool any_version;         // true = no version constraint

    VersionRange() : any_version(true) {}

    bool is_satisfied_by(const std::string& version) const;
};

// Update ModMetadata
struct ModMetadata {
    std::string mod_id;
    std::string version;
    std::string display_name;
    std::string description;
    bool enabled = true;
    bool is_framework_mod = false;

    // NEW: Dependencies
    std::map<std::string, VersionRange> dependencies;

    // Incompatibilities (Feature 1.3)
    std::map<std::string, IncompatibilitySpec> incompatible_mods;

    // Capabilities
    json capabilities;
};
```

**Estimate**: 1 hour

---

#### Step 2: Parse Dependencies from ap_config.json

**File**: [src/framework_core/src/mod_registry.cpp](../../../../src/framework_core/src/mod_registry.cpp)

**Update discover_mods()**:
```cpp
void ModRegistry::discover_mods(const std::filesystem::path& mods_directory) {
    LOG_INFO("Discovering mods in: " + mods_directory.string());

    for (const auto& entry : std::filesystem::directory_iterator(mods_directory)) {
        if (!entry.is_directory()) continue;

        auto config_path = entry.path() / "ap_config.json";
        if (!std::filesystem::exists(config_path)) continue;

        // Parse mod metadata
        ModMetadata metadata;
        if (!parse_mod_config(config_path, metadata)) {
            LOG_ERROR("Failed to parse mod config: " + config_path.string());
            continue;
        }

        // Check if mod is enabled
        if (!metadata.enabled) {
            LOG_DEBUG("Skipping disabled mod: " + metadata.mod_id);
            continue;
        }

        // Add to discovered mods
        discovered_mods_[metadata.mod_id] = metadata;
        LOG_INFO("Discovered mod: " + metadata.mod_id + " (v" + metadata.version + ")");
    }

    LOG_INFO("Discovered " + std::to_string(discovered_mods_.size()) + " mods");
}

bool ModRegistry::parse_mod_config(const std::filesystem::path& config_path, ModMetadata& metadata) {
    // Read file
    std::ifstream file(config_path);
    if (!file.is_open()) {
        return false;
    }

    // Parse JSON
    json config;
    try {
        file >> config;
    } catch (const json::exception& e) {
        LOG_ERROR("JSON parse error: " + std::string(e.what()));
        return false;
    }

    // Required fields
    if (!config.contains("mod_id")) {
        LOG_ERROR("Missing required field: mod_id");
        return false;
    }
    metadata.mod_id = config["mod_id"];

    // Optional fields
    metadata.version = config.value("version", "0.0.0");
    metadata.display_name = config.value("display_name", metadata.mod_id);
    metadata.description = config.value("description", "");
    metadata.enabled = config.value("enabled", true);

    // Parse dependencies (NEW)
    if (config.contains("dependencies")) {
        parse_dependencies(config["dependencies"], metadata.dependencies);
    }

    // Parse incompatibilities (Feature 1.3)
    if (config.contains("incompatible_mods")) {
        parse_incompatibilities(config["incompatible_mods"], metadata.incompatible_mods);
    }

    // Parse capabilities
    if (config.contains("capabilities")) {
        metadata.capabilities = config["capabilities"];
    }

    return true;
}

void ModRegistry::parse_dependencies(const json& deps_json, std::map<std::string, VersionRange>& dependencies) {
    for (auto& [mod_id, spec] : deps_json.items()) {
        VersionRange range;

        if (spec.is_boolean() && spec.get<bool>()) {
            // "mod.id": true → any version
            range.any_version = true;
        } else if (spec.is_object()) {
            // "mod.id": {"min_version": "1.0", "max_version": "2.0"}
            range.any_version = false;
            range.min_version = spec.value("min_version", "");
            range.max_version = spec.value("max_version", "");
        } else {
            LOG_WARNING("Invalid dependency spec for: " + mod_id);
            continue;
        }

        dependencies[mod_id] = range;
    }
}
```

**Estimate**: 2 hours

---

#### Step 3: Validate Dependencies and Build Graph

**File**: [src/framework_core/src/mod_registry.cpp](../../../../src/framework_core/src/mod_registry.cpp)

**Add validation function**:
```cpp
bool ModRegistry::validate_dependencies() {
    LOG_INFO("Validating mod dependencies...");

    std::vector<std::string> mods_to_disable;

    for (auto& [mod_id, metadata] : discovered_mods_) {
        if (!metadata.enabled) continue;

        // Check each dependency
        for (const auto& [dep_mod_id, version_range] : metadata.dependencies) {
            // Check if dependency exists
            auto it = discovered_mods_.find(dep_mod_id);
            if (it == discovered_mods_.end()) {
                LOG_ERROR("Mod '" + mod_id + "' depends on '" + dep_mod_id + "' which is not installed");
                mods_to_disable.push_back(mod_id);
                continue;
            }

            // Check if dependency is enabled
            if (!it->second.enabled) {
                LOG_ERROR("Mod '" + mod_id + "' depends on '" + dep_mod_id + "' which is disabled");
                mods_to_disable.push_back(mod_id);
                continue;
            }

            // Check version constraint
            if (!version_range.is_satisfied_by(it->second.version)) {
                LOG_ERROR("Mod '" + mod_id + "' requires '" + dep_mod_id + "' version in range [" +
                          version_range.min_version + ", " + version_range.max_version + "], but found version " +
                          it->second.version);
                mods_to_disable.push_back(mod_id);
                continue;
            }
        }
    }

    // Disable mods with missing/invalid dependencies
    for (const auto& mod_id : mods_to_disable) {
        LOG_WARNING("Disabling mod: " + mod_id + " (dependency issues)");
        discovered_mods_[mod_id].enabled = false;
    }

    // Cascade disable: disable mods that depend on disabled mods
    cascade_disable_dependents();

    return mods_to_disable.empty();
}

void ModRegistry::cascade_disable_dependents() {
    bool changed = true;

    while (changed) {
        changed = false;

        for (auto& [mod_id, metadata] : discovered_mods_) {
            if (!metadata.enabled) continue;

            // Check if any dependency is disabled
            for (const auto& [dep_mod_id, _] : metadata.dependencies) {
                auto it = discovered_mods_.find(dep_mod_id);
                if (it != discovered_mods_.end() && !it->second.enabled) {
                    LOG_WARNING("Cascade disabling mod: " + mod_id + " (depends on disabled mod: " + dep_mod_id + ")");
                    metadata.enabled = false;
                    changed = true;
                    break;
                }
            }
        }
    }
}

// Implement version comparison
bool VersionRange::is_satisfied_by(const std::string& version) const {
    if (any_version) {
        return true;
    }

    // Simple version comparison (Phase 3 will add semantic versioning)
    // For now, just check min/max bounds with string comparison
    // TODO: Implement proper semantic version comparison in Phase 3

    if (!min_version.empty() && version < min_version) {
        return false;
    }

    if (!max_version.empty() && version > max_version) {
        return false;
    }

    return true;
}
```

**Estimate**: 3 hours

---

#### Step 4: Deny Registration for Disabled Mods

**File**: [src/framework_core/src/framework_core.cpp](../../../../src/framework_core/src/framework_core.cpp)

**Update registration handler**:
```cpp
void FrameworkCore::handle_registration(const std::string& mod_id, const json& capabilities) {
    LOG_INFO("Registration request from: " + mod_id);

    // Special handling for framework mod
    if (mod_id == "archipelago.palworld.framework") {
        // ... (Feature 1.1 code)
        return;
    }

    // Check if mod is discovered
    auto metadata = mod_registry_->get_mod_metadata(mod_id);
    if (!metadata) {
        LOG_ERROR("Registration denied: Mod not discovered: " + mod_id);
        send_registration_error(mod_id, "Mod not discovered during initialization");
        return;
    }

    // Check if mod is enabled (NEW)
    if (!metadata->enabled) {
        LOG_ERROR("Registration denied: Mod is disabled: " + mod_id);
        std::string reason = "Mod is disabled (dependency or incompatibility issue)";
        send_registration_error(mod_id, reason);
        return;
    }

    // Normal registration logic
    registered_mods_[mod_id] = *metadata;

    // Send success
    json response;
    response["type"] = "registration_success";
    response["mod_id"] = mod_id;
    message_router_->send_to_mod(mod_id, response.dump());

    LOG_INFO("Mod registered successfully: " + mod_id);
}

void FrameworkCore::send_registration_error(const std::string& mod_id, const std::string& reason) {
    json error_response;
    error_response["type"] = "registration_error";
    error_response["mod_id"] = mod_id;
    error_response["error"] = reason;

    message_router_->send_to_mod(mod_id, error_response.dump());
}
```

**Estimate**: 1.5 hours

---

#### Step 5: Testing

**Test Cases**:

1. **Valid dependency**: Mod A depends on Mod B (any version), Mod B exists → success
2. **Missing dependency**: Mod A depends on Mod C, Mod C not installed → Mod A disabled
3. **Version mismatch**: Mod A depends on Mod B v2.0+, Mod B is v1.0 → Mod A disabled
4. **Cascade disable**: Mod A depends on Mod B, Mod B depends on Mod C, Mod C missing → Both A and B disabled
5. **Circular dependencies**: Mod A depends on Mod B, Mod B depends on Mod A → Both disabled (or special handling)

**Test Mods** (create mock ap_config.json files):

**Mod A** (depends on B):
```json
{
  "schema_version": 2,
  "mod_id": "test.mod.a",
  "version": "1.0.0",
  "dependencies": {
    "test.mod.b": true
  },
  "capabilities": {}
}
```

**Mod B** (no dependencies):
```json
{
  "schema_version": 2,
  "mod_id": "test.mod.b",
  "version": "1.0.0",
  "capabilities": {}
}
```

**Estimate**: 1-2 hours

---

### Files Modified

- [src/framework_core/include/mod_registry.h](../../../../src/framework_core/include/mod_registry.h) - **MODERATE** (add VersionRange struct)
- [src/framework_core/src/mod_registry.cpp](../../../../src/framework_core/src/mod_registry.cpp) - **MAJOR** (parse deps, validate, cascade)
- [src/framework_core/src/framework_core.cpp](../../../../src/framework_core/src/framework_core.cpp) - **MODERATE** (deny registration)

**Estimated Lines Changed**: ~300-400 lines

---

### Success Criteria

- [ ] Dependencies parsed from ap_config.json
- [ ] Mods with missing dependencies disabled
- [ ] Mods with version mismatches disabled
- [ ] Cascade disabling works (dependency chains)
- [ ] Registration denied for disabled mods
- [ ] Clear error messages sent via IPC
- [ ] Dependency validation logged clearly

---

## Feature 1.3: Incompatibility System

**Priority**: 5th (Safety Foundation)
**Estimate**: 8-10 hours
**Dependencies**: lunajson

### Problem Statement

**Current**: No incompatibility detection. Conflicting mods can run together, causing crashes or data corruption.

**Missing**:
- No `incompatible_mods` field in mod metadata
- No conflict detection during discovery
- No auto-disabling logic
- No mutual incompatibility handling

### Solution

Implement **three types of incompatibility**:

**Type 1: Version Range**
```json
"incompatible_mods": {
  "mod.id": {"min_version": "1.0", "max_version": "2.0"}
}
```

**Type 2: Specific Versions**
```json
"incompatible_mods": {
  "mod.id": ["1.5.0", "1.5.1", "1.6.3"]
}
```

**Type 3: Complete Incompatibility**
```json
"incompatible_mods": {
  "mod.id": true
}
```

**Conflict Resolution**:
- **Mutual incompatibility** (both mods list each other): Disable BOTH
- **One-way incompatibility** (only Mod A lists Mod B): Disable ONLY Mod A

### Implementation Steps

#### Step 1: Define Incompatibility Schema

**File**: [src/framework_core/include/mod_registry.h](../../../../src/framework_core/include/mod_registry.h)

**Add structs**:
```cpp
// Incompatibility specification (three types)
struct IncompatibilitySpec {
    enum Type {
        COMPLETE,           // true - all versions incompatible
        VERSION_RANGE,      // {min, max} - specific version range
        SPECIFIC_VERSIONS   // ["1.0", "1.1"] - specific versions only
    };

    Type type;
    VersionRange version_range;               // For VERSION_RANGE type
    std::vector<std::string> specific_versions;  // For SPECIFIC_VERSIONS type

    IncompatibilitySpec() : type(COMPLETE) {}

    bool is_incompatible_with(const std::string& version) const;
};

// Update ModMetadata
struct ModMetadata {
    std::string mod_id;
    std::string version;
    std::string display_name;
    std::string description;
    bool enabled = true;
    bool is_framework_mod = false;

    // Dependencies (Feature 1.2)
    std::map<std::string, VersionRange> dependencies;

    // NEW: Incompatibilities
    std::map<std::string, IncompatibilitySpec> incompatible_mods;

    // Capabilities
    json capabilities;
};
```

**Estimate**: 1 hour

---

#### Step 2: Parse Incompatibilities from ap_config.json

**File**: [src/framework_core/src/mod_registry.cpp](../../../../src/framework_core/src/mod_registry.cpp)

**Add parsing function**:
```cpp
void ModRegistry::parse_incompatibilities(const json& incompat_json, std::map<std::string, IncompatibilitySpec>& incompatibilities) {
    for (auto& [mod_id, spec] : incompat_json.items()) {
        IncompatibilitySpec incompat;

        if (spec.is_boolean() && spec.get<bool>()) {
            // Type 1: Complete incompatibility
            // "mod.id": true
            incompat.type = IncompatibilitySpec::COMPLETE;

        } else if (spec.is_object()) {
            // Type 2: Version range incompatibility
            // "mod.id": {"min_version": "1.0", "max_version": "2.0"}
            incompat.type = IncompatibilitySpec::VERSION_RANGE;
            incompat.version_range.any_version = false;
            incompat.version_range.min_version = spec.value("min_version", "");
            incompat.version_range.max_version = spec.value("max_version", "");

        } else if (spec.is_array()) {
            // Type 3: Specific versions incompatibility
            // "mod.id": ["1.5.0", "1.5.1", "1.6.3"]
            incompat.type = IncompatibilitySpec::SPECIFIC_VERSIONS;
            for (const auto& version : spec) {
                incompat.specific_versions.push_back(version.get<std::string>());
            }

        } else {
            LOG_WARNING("Invalid incompatibility spec for: " + mod_id);
            continue;
        }

        incompatibilities[mod_id] = incompat;
    }
}

bool IncompatibilitySpec::is_incompatible_with(const std::string& version) const {
    switch (type) {
        case COMPLETE:
            // All versions incompatible
            return true;

        case VERSION_RANGE:
            // Check if version falls within incompatible range
            return version_range.is_satisfied_by(version);

        case SPECIFIC_VERSIONS:
            // Check if version is in specific list
            return std::find(specific_versions.begin(), specific_versions.end(), version) != specific_versions.end();
    }

    return false;
}
```

**Estimate**: 2 hours

---

#### Step 3: Detect Conflicts and Auto-Disable

**File**: [src/framework_core/src/mod_registry.cpp](../../../../src/framework_core/src/mod_registry.cpp)

**Add conflict detection**:
```cpp
void ModRegistry::detect_incompatibilities() {
    LOG_INFO("Detecting mod incompatibilities...");

    // Build incompatibility matrix
    std::map<std::string, std::vector<std::string>> conflicts;

    for (auto& [mod_id, metadata] : discovered_mods_) {
        if (!metadata.enabled) continue;

        // Check each incompatibility declaration
        for (const auto& [incompat_mod_id, spec] : metadata.incompatible_mods) {
            // Check if incompatible mod exists and is enabled
            auto it = discovered_mods_.find(incompat_mod_id);
            if (it == discovered_mods_.end() || !it->second.enabled) {
                continue;  // Incompatible mod not present, no conflict
            }

            // Check if version is incompatible
            if (spec.is_incompatible_with(it->second.version)) {
                LOG_WARNING("Incompatibility detected: " + mod_id + " is incompatible with " + incompat_mod_id);
                conflicts[mod_id].push_back(incompat_mod_id);
            }
        }
    }

    // Resolve conflicts
    resolve_conflicts(conflicts);
}

void ModRegistry::resolve_conflicts(const std::map<std::string, std::vector<std::string>>& conflicts) {
    std::set<std::string> mods_to_disable;

    for (const auto& [mod_id, conflicting_mods] : conflicts) {
        for (const auto& conflict_mod_id : conflicting_mods) {
            // Check if this is a mutual incompatibility
            bool is_mutual = false;

            auto it = conflicts.find(conflict_mod_id);
            if (it != conflicts.end()) {
                auto& reverse_conflicts = it->second;
                if (std::find(reverse_conflicts.begin(), reverse_conflicts.end(), mod_id) != reverse_conflicts.end()) {
                    is_mutual = true;
                }
            }

            if (is_mutual) {
                // Mutual incompatibility: disable BOTH mods
                LOG_ERROR("Mutual incompatibility: " + mod_id + " <-> " + conflict_mod_id + " (disabling both)");
                mods_to_disable.insert(mod_id);
                mods_to_disable.insert(conflict_mod_id);
            } else {
                // One-way incompatibility: disable ONLY the declaring mod
                LOG_ERROR("One-way incompatibility: " + mod_id + " declares incompatibility with " + conflict_mod_id + " (disabling " + mod_id + ")");
                mods_to_disable.insert(mod_id);
            }
        }
    }

    // Disable conflicting mods
    for (const auto& mod_id : mods_to_disable) {
        discovered_mods_[mod_id].enabled = false;
        LOG_WARNING("Mod disabled due to incompatibility: " + mod_id);
    }
}
```

**Estimate**: 3 hours

---

#### Step 4: Integrate with Discovery Flow

**File**: [src/framework_core/src/framework_core.cpp](../../../../src/framework_core/src/framework_core.cpp)

**Update initialization**:
```cpp
void FrameworkCore::initialize() {
    LOG_INFO("Initializing APFramework...");

    // Start IPC server
    ipc_server_->start();

    // Discover mods
    mod_registry_->discover_mods("Mods");

    // Validate dependencies (Feature 1.2)
    mod_registry_->validate_dependencies();

    // Detect incompatibilities (Feature 1.3 - NEW)
    mod_registry_->detect_incompatibilities();

    // Log final enabled mods
    auto enabled_mods = mod_registry_->get_enabled_mods();
    LOG_INFO("Enabled mods: " + std::to_string(enabled_mods.size()));
    for (const auto& mod : enabled_mods) {
        LOG_INFO("  - " + mod.mod_id + " (v" + mod.version + ")");
    }

    LOG_INFO("APFramework initialized successfully");
}
```

**Estimate**: 30 minutes

---

#### Step 5: Send IPC Warnings to Disabled Mods

**File**: [src/framework_core/src/framework_core.cpp](../../../../src/framework_core/src/framework_core.cpp)

**Add warning messages**:
```cpp
void FrameworkCore::send_incompatibility_warnings() {
    for (const auto& [mod_id, metadata] : mod_registry_->get_all_mods()) {
        if (!metadata.enabled) {
            // Send warning to disabled mod
            json warning;
            warning["type"] = "warning";
            warning["level"] = "ERROR";
            warning["message"] = "Mod disabled due to incompatibility or dependency issue";

            // Try to send (mod may not be connected yet)
            message_router_->send_to_mod(mod_id, warning.dump());
        }
    }
}
```

**Call in initialize()**:
```cpp
void FrameworkCore::initialize() {
    // ...
    mod_registry_->detect_incompatibilities();

    // Send warnings (NEW)
    send_incompatibility_warnings();

    // ...
}
```

**Estimate**: 1 hour

---

#### Step 6: Testing

**Test Cases**:

1. **No conflicts**: Mod A and Mod B with no incompatibilities → both enabled
2. **One-way conflict**: Mod A declares incompatibility with Mod B → only Mod A disabled
3. **Mutual conflict**: Mod A and Mod B both declare incompatibility → both disabled
4. **Version range conflict**: Mod A incompatible with Mod B v1.0-2.0, Mod B is v1.5 → conflict detected
5. **Specific version conflict**: Mod A incompatible with Mod B ["1.5.0"], Mod B is v1.5.0 → conflict detected
6. **No conflict (version mismatch)**: Mod A incompatible with Mod B v2.0+, Mod B is v1.0 → no conflict

**Test Mods**:

**Mod A** (incompatible with B):
```json
{
  "schema_version": 2,
  "mod_id": "test.mod.a",
  "version": "1.0.0",
  "incompatible_mods": {
    "test.mod.b": true
  },
  "capabilities": {}
}
```

**Mod B** (no declarations):
```json
{
  "schema_version": 2,
  "mod_id": "test.mod.b",
  "version": "1.0.0",
  "capabilities": {}
}
```

**Mod C** (mutual conflict with D):
```json
{
  "schema_version": 2,
  "mod_id": "test.mod.c",
  "version": "1.0.0",
  "incompatible_mods": {
    "test.mod.d": true
  },
  "capabilities": {}
}
```

**Mod D** (mutual conflict with C):
```json
{
  "schema_version": 2,
  "mod_id": "test.mod.d",
  "version": "1.0.0",
  "incompatible_mods": {
    "test.mod.c": true
  },
  "capabilities": {}
}
```

**Estimate**: 1.5 hours

---

### Files Modified

- [src/framework_core/include/mod_registry.h](../../../../src/framework_core/include/mod_registry.h) - **MODERATE** (add IncompatibilitySpec)
- [src/framework_core/src/mod_registry.cpp](../../../../src/framework_core/src/mod_registry.cpp) - **MAJOR** (parse, detect, resolve)
- [src/framework_core/src/framework_core.cpp](../../../../src/framework_core/src/framework_core.cpp) - **MODERATE** (integrate, warnings)

**Estimated Lines Changed**: ~350-450 lines

---

### Success Criteria

- [ ] Three incompatibility types parsed correctly
- [ ] Conflicts detected during discovery
- [ ] Mutual incompatibility disables both mods
- [ ] One-way incompatibility disables only declaring mod
- [ ] Version range incompatibility works
- [ ] Specific version incompatibility works
- [ ] IPC warnings sent to disabled mods
- [ ] Conflict resolution logged clearly

---

## Features 1.4 and 1.6 (Runtime Enablement & Log Routing)

Due to length constraints, these features will be detailed in a continuation document. They follow the same detailed structure as above.

**Feature 1.4 Summary**: Runtime enablement system with in-memory tracking, file persistence, and auto-disable API.

**Feature 1.6 Summary**: Log routing via IPC from C++ framework to Lua framework mod, with configurable log_mode and log_verbosity.

---

## Phase 1 Completion Checklist

### Implementation Order

- [ ] 1.7 lunajson integration (4-6 hours)
- [ ] 1.5 Multi-slot capabilities (3-4 hours)
- [ ] 1.1 Framework mod dual-role (4-6 hours)
- [ ] 1.2 Dependency system (8-10 hours)
- [ ] 1.3 Incompatibility system (8-10 hours)
- [ ] 1.4 Runtime enablement (6-8 hours)
- [ ] 1.6 Log routing (6-8 hours)

### Testing

- [ ] All features individually tested
- [ ] Integration testing (full flow with multiple mods)
- [ ] Edge case testing (circular deps, mutual conflicts, etc.)
- [ ] Manual testing in UE4SS environment

### Documentation

- [ ] README updated with new features
- [ ] Schema v2 documented
- [ ] Migration guide written
- [ ] Examples updated

### Release Criteria

- [ ] All Phase 1 features implemented and tested
- [ ] No critical bugs
- [ ] Documentation complete
- [ ] Ready for beta testing

---

**End of Phase 1 Implementation Plan**

**Next**: [Phase02_ValidationPolish.md](Phase02_ValidationPolish.md)