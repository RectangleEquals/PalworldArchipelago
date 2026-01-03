# Phase 2: Important Improvements - Implementation Plan

**Phase**: 2 of 3
**Priority**: 🟡 Important
**Duration**: 2-3 days (13-19 hours)
**Status**: Ready after Phase 1 completion
**Dependencies**: Phase 1 (metadata schema)

---

## Overview

Phase 2 implements important improvements that enhance developer experience and system robustness. These features build on Phase 1's foundation and prepare the framework for community adoption.

### Goals

1. **Enforcement**: Strict mod ID format validation
2. **Integration**: Client library logging support
3. **Usability**: Clear load order documentation
4. **Examples**: Working C++ mod reference implementation

### Success Criteria

- [ ] Mod IDs follow `author.game.mod` format or are rejected with clear error
- [ ] Mods can log to framework via client library API
- [ ] Load order documentation complete and tested by developers
- [ ] C++ mod example compiles, runs, and demonstrates best practices
- [ ] Error messages are actionable and helpful
- [ ] All features documented

---

## Feature 2.1: Mod ID Format Validation

### Problem Statement

**Current State**: Mod IDs are any string, no format enforcement.

**Intended Design**: Mod IDs must follow `author.game.mod` format for proper namespacing.

**Impact**: Risk of mod ID collisions, poor organization.

### Solution Design

#### Mod ID Format Specification

**Format**: `author.game.mod`

**Rules**:
- Three components separated by dots
- Each component: lowercase alphanumeric + underscores
- Regex: `^[a-z0-9_]+\.[a-z0-9_]+\.[a-z0-9_]+$`

**Examples**:
- ✅ Valid: `john.palworld.chest_shuffle`
- ✅ Valid: `team_x.palworld.tech_randomizer`
- ❌ Invalid: `MyMod` (not three components)
- ❌ Invalid: `John.Palworld.Mod` (uppercase not allowed)
- ❌ Invalid: `author.game` (only two components)

#### Error Messages

**For Invalid Format**:
```
ERROR: Invalid mod_id format: 'MyMod'
Expected format: author.game.mod
  - 'author': Your username or team name (lowercase)
  - 'game': Game identifier ('palworld')
  - 'mod': Mod identifier (lowercase, descriptive)
Example: john.palworld.chest_shuffle
```

### Implementation Steps

#### Step 1: Add Validation Function (1 hour)

**File**: [src/framework_core/src/mod_registry.cpp](../../../src/framework_core/src/mod_registry.cpp)

**Add Function**:
```cpp
bool ModRegistry::validate_mod_id_format(const std::string& mod_id, std::string& error) {
    // Regex for author.game.mod format
    std::regex mod_id_regex("^[a-z0-9_]+\\.[a-z0-9_]+\\.[a-z0-9_]+$");

    if (!std::regex_match(mod_id, mod_id_regex)) {
        error = format_mod_id_error(mod_id);
        return false;
    }

    return true;
}

std::string ModRegistry::format_mod_id_error(const std::string& invalid_id) {
    return std::string("Invalid mod_id format: '") + invalid_id + "'\n" +
           "Expected format: author.game.mod\n" +
           "  - 'author': Your username or team name (lowercase)\n" +
           "  - 'game': Game identifier ('palworld')\n" +
           "  - 'mod': Mod identifier (lowercase, descriptive)\n" +
           "Example: john.palworld.chest_shuffle";
}
```

#### Step 2: Enforce in Discovery (30 min)

**File**: [src/framework_core/src/mod_registry.cpp](../../../src/framework_core/src/mod_registry.cpp)

**Update validate_metadata()**:
```cpp
bool ModRegistry::validate_metadata(const ModMetadata& metadata, std::string& error) {
    // Existing required field checks...

    // NEW: Strict mod_id format validation
    if (!validate_mod_id_format(metadata.mod_id, error)) {
        return false;  // Reject mod
    }

    // Rest of validation...
    return true;
}
```

#### Step 3: Enforce in Registration (30 min)

**File**: [src/framework_core/src/framework_core.cpp](../../../src/framework_core/src/framework_core.cpp)

**Update handle_mod_registration()**:
```cpp
void FrameworkCore::handle_mod_registration(const IPCMessage& msg) {
    std::string mod_id = msg.mod_id;

    // NEW: Validate mod_id format before processing
    std::string error;
    if (!mod_registry_->validate_mod_id_format(mod_id, error)) {
        LOG_ERROR("Registration rejected for {}: {}", mod_id, error);

        // Send error response to mod
        IPCMessage response;
        response.type = "registration_error";
        response.mod_id = mod_id;
        response.data_json = nlohmann::json{{"error", error}}.dump();
        ipc_server_->send_to_mod(mod_id, response);

        return;  // Reject registration
    }

    // Existing registration logic...
}
```

### Testing Plan

**Test Cases**:

| Mod ID | Valid | Expected Result |
|--------|-------|-----------------|
| `john.palworld.testmod` | ✅ | Accepted |
| `team_x.palworld.chest_mod` | ✅ | Accepted |
| `a.b.c` | ✅ | Accepted (minimal) |
| `MyMod` | ❌ | Rejected: format error |
| `Author.Palworld.Mod` | ❌ | Rejected: uppercase |
| `john.palworld` | ❌ | Rejected: only 2 components |
| `john.palworld.mod.extra` | ❌ | Rejected: 4 components |
| `john.palworld.mod-name` | ❌ | Rejected: hyphen not allowed |

**Integration Test**:
1. Create mod with invalid ID `MyMod`
2. Start framework
3. Verify: Error logged with helpful message
4. Verify: Mod not discovered
5. Fix mod_id to `myname.palworld.mymod`
6. Restart framework
7. Verify: Mod discovered successfully

---

## Feature 2.2: Client Library Logging Support

### Problem Statement

**Current State**: Mods must implement their own logging or can't log at all.

**Intended Design**: Client libraries should provide logging support.

**Impact**: Inconsistent logging, mods can't contribute to framework log.

### Solution Design

#### Logging Architecture

```
Mod
 ↓
Client Library (log call)
 ↓
IPC Message (type: "log")
 ↓
Framework
 ↓
Logger (append to framework.log)
```

#### API Design

**C++ Library**:
```cpp
// Set log callback (optional)
void ap_client_set_log_callback(
    APClientHandle handle,
    void (*callback)(const char* level, const char* message, void* user_data),
    void* user_data
);

// Log directly to framework
void ap_client_log(APClientHandle handle, const char* level, const char* message);
```

**Lua Library**:
```lua
-- Set log callback (optional)
client.on_log = function(level, message)
    print(string.format("[MyMod] [%s] %s", level, message))
end

-- Log directly to framework
client:log("INFO", "My mod initialized")
```

### Implementation Steps

#### Step 1: Add IPC Log Message Type (1 hour)

**File**: [src/framework_core/src/framework_core.cpp](../../../src/framework_core/src/framework_core.cpp)

**Add Handler**:
```cpp
void FrameworkCore::handle_ipc_message(const IPCMessage& msg) {
    if (msg.type == "register") {
        handle_mod_registration(msg);
    } else if (msg.type == "location_check") {
        handle_location_check(msg);
    } else if (msg.type == "connect") {
        handle_connection_request(msg);
    } else if (msg.type == "status_update") {
        handle_status_update(msg);
    }
    // NEW: Handle log messages from mods
    else if (msg.type == "log") {
        handle_mod_log(msg);
    }
    else {
        LOG_WARNING("Unknown IPC message type: {}", msg.type);
    }
}

void FrameworkCore::handle_mod_log(const IPCMessage& msg) {
    try {
        nlohmann::json data = nlohmann::json::parse(msg.data_json);
        std::string level = data["level"];
        std::string message = data["message"];

        // Prefix with mod_id for clarity
        std::string prefixed_message = std::string("[") + msg.mod_id + "] " + message;

        // Log to framework log
        if (level == "DEBUG") {
            LOG_DEBUG("{}", prefixed_message);
        } else if (level == "INFO") {
            LOG_INFO("{}", prefixed_message);
        } else if (level == "WARNING") {
            LOG_WARNING("{}", prefixed_message);
        } else if (level == "ERROR") {
            LOG_ERROR("{}", prefixed_message);
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse log message from {}: {}", msg.mod_id, e.what());
    }
}
```

#### Step 2: C++ Client Library API (2 hours)

**File**: [src/client_lib/include/ap_client_lib.h](../../../src/client_lib/include/ap_client_lib.h)

**Add Functions**:
```cpp
// Log callback type
typedef void (*LogCallback)(const char* level, const char* message, void* user_data);

// Set optional log callback (called before sending to framework)
APCLIENT_API void ap_client_set_log_callback(
    APClientHandle handle,
    LogCallback callback,
    void* user_data
);

// Log message (sends to framework and optionally calls callback)
APCLIENT_API void ap_client_log(
    APClientHandle handle,
    const char* level,  // "DEBUG", "INFO", "WARNING", "ERROR"
    const char* message
);

// Convenience macros
#define AP_LOG_DEBUG(handle, msg) ap_client_log(handle, "DEBUG", msg)
#define AP_LOG_INFO(handle, msg) ap_client_log(handle, "INFO", msg)
#define AP_LOG_WARNING(handle, msg) ap_client_log(handle, "WARNING", msg)
#define AP_LOG_ERROR(handle, msg) ap_client_log(handle, "ERROR", msg)
```

**File**: [src/client_lib/src/ap_client_lib.cpp](../../../src/client_lib/src/ap_client_lib.cpp)

**Implement**:
```cpp
void ap_client_set_log_callback(APClientHandle handle, LogCallback callback, void* user_data) {
    if (!handle) return;
    APClientInstance* instance = static_cast<APClientInstance*>(handle);
    instance->log_callback = callback;
    instance->log_user_data = user_data;
}

void ap_client_log(APClientHandle handle, const char* level, const char* message) {
    if (!handle) return;
    APClientInstance* instance = static_cast<APClientInstance*>(handle);

    // Call user callback first (if set)
    if (instance->log_callback) {
        instance->log_callback(level, message, instance->log_user_data);
    }

    // Send to framework via IPC
    nlohmann::json data;
    data["level"] = level;
    data["message"] = message;

    IPCMessage msg;
    msg.type = "log";
    msg.mod_id = instance->mod_id;
    msg.data_json = data.dump();

    instance->ipc_client->send_message(msg);
}
```

**Update APClientInstance Struct**:
```cpp
struct APClientInstance {
    std::string mod_id;
    std::unique_ptr<IPCClient> ipc_client;

    // Existing callbacks...
    ItemReceivedCallback item_received_callback;
    void* item_received_user_data;
    // ...

    // NEW: Log callback
    LogCallback log_callback = nullptr;
    void* log_user_data = nullptr;
};
```

#### Step 3: Lua Client Library API (1 hour)

**File**: [src/lua_client/ap_client.lua](../../../src/lua_client/ap_client.lua)

**Add Method**:
```lua
function APClient:log(level, message)
    -- Call user callback first (if set)
    if self.on_log then
        self.on_log(level, message)
    end

    -- Send to framework via IPC
    local msg = {
        type = "log",
        mod_id = self.mod_id,
        data = {
            level = level,
            message = message
        }
    }

    self:send_message(msg)
end

-- Convenience methods
function APClient:log_debug(message)
    self:log("DEBUG", message)
end

function APClient:log_info(message)
    self:log("INFO", message)
end

function APClient:log_warning(message)
    self:log("WARNING", message)
end

function APClient:log_error(message)
    self:log("ERROR", message)
end
```

**Add Callback Hook**:
```lua
function APClient:new(mod_id, pipe_name)
    local instance = setmetatable({}, APClient)
    instance.mod_id = mod_id
    instance.pipe_name = pipe_name or "APFramework_default"

    -- Existing callbacks...
    instance.on_item_received = nil
    instance.on_location_checked = nil
    instance.on_connection_status = nil
    instance.on_registration_complete = nil

    -- NEW: Log callback (optional)
    instance.on_log = nil

    -- Connect to framework...
    instance:connect()

    return instance
end
```

### Testing Plan

**C++ Test**:
```cpp
APClientHandle client = ap_client_create("test.palworld.logtest");

// Set log callback (optional)
ap_client_set_log_callback(client, [](const char* level, const char* msg, void* data) {
    printf("[Callback] [%s] %s\n", level, msg);
}, nullptr);

// Log messages
ap_client_log(client, "INFO", "C++ mod initialized");
ap_client_log(client, "DEBUG", "Processing item...");
ap_client_log(client, "ERROR", "Failed to grant item");

// Check framework.log contains:
// [2026-01-02 14:23:45.123] [INFO] [test.palworld.logtest] C++ mod initialized
// [2026-01-02 14:23:45.456] [DEBUG] [test.palworld.logtest] Processing item...
// [2026-01-02 14:23:45.789] [ERROR] [test.palworld.logtest] Failed to grant item
```

**Lua Test**:
```lua
local client = APClient:new("test.palworld.logtest")

-- Set log callback (optional)
client.on_log = function(level, message)
    print(string.format("[MyCallback] [%s] %s", level, message))
end

-- Log messages
client:log_info("Lua mod initialized")
client:log_debug("Processing location check...")
client:log_error("Failed to check location")

-- Check framework.log contains the same prefixed format
```

---

## Feature 2.3: Load Order Documentation

### Problem Statement

**Current State**: No documentation on how to configure UE4SS load order.

**Intended Design**: Clear instructions for users and developers.

**Impact**: Users don't know framework must load before mods.

### Solution Design

#### Documentation Structure

**README.md**: High-level overview with quick start
**docs/LOAD_ORDER.md**: Detailed configuration guide
**examples/README.md**: Example mod setup instructions

### Implementation Steps

#### Step 1: Update README.md (1 hour)

**File**: [README.md](../../../README.md)

**Add Section**:
```markdown
## Installation

### Prerequisites

- Palworld (v0.3.0 or later)
- UE4SS (v3.0.0 or later)

### Framework Installation

1. Download `APFramework.zip` from releases
2. Extract to `Palworld/ue4ss/Mods/` (creates `APFramework/` folder)
3. Verify `APFramework/enabled.txt` exists

### Configure Load Order

**CRITICAL**: APFramework must load BEFORE any AP-enabled mods.

Edit `Palworld/ue4ss/Mods/mods.txt` and ensure APFramework is listed first:

```
APFramework : 1
PalworldAPMod : 1
AnotherAPMod : 1
; Other mods...
```

See [docs/LOAD_ORDER.md](docs/LOAD_ORDER.md) for detailed instructions.

### Verify Installation

1. Launch Palworld
2. Open UE4SS console (`)
3. Look for: `[APFramework] [INFO] Framework initialized`
4. Check `APFramework/Logs/framework.log` for details
```

#### Step 2: Create docs/LOAD_ORDER.md (2 hours)

**File**: [docs/LOAD_ORDER.md](../../../docs/LOAD_ORDER.md)

**Contents**:
```markdown
# Load Order Configuration Guide

## Why Load Order Matters

APFramework acts as a central coordinator for all AP-enabled mods. It must:
1. Start its IPC server before mods connect
2. Discover all mods before they register
3. Generate APCapabilities.json after all registrations

Therefore, **APFramework MUST load before any AP-enabled mods**.

## UE4SS Load Order Mechanisms

UE4SS loads mods in two phases:

### Phase 1: C++ Mods
- Loaded automatically at game startup
- Load order determined by filesystem (not configurable)
- **Issue**: C++ mods load BEFORE Lua mods

### Phase 2: Lua Mods
- Loaded after C++ mods
- Load order configured in `mods.txt` or `mods.json`
- APFramework is a Lua mod

## Configuration Methods

### Method 1: mods.txt (Recommended)

**File**: `Palworld/ue4ss/Mods/mods.txt`

**Format**: `ModName : Enabled`
- `1` = Enabled
- `0` = Disabled

**Example**:
```
; APFramework must be first!
APFramework : 1

; AP-enabled mods come after
PalworldAPMod : 1
ChestShuffleMod : 1

; Non-AP mods can be anywhere
SomeOtherMod : 1
```

**Steps**:
1. Open `ue4ss/Mods/mods.txt` in text editor
2. Move `APFramework : 1` to the **first line** (ignore comments)
3. Ensure all AP-enabled mods are AFTER APFramework
4. Save file
5. Restart Palworld

### Method 2: mods.json (Alternative)

**File**: `Palworld/ue4ss/Mods/mods.json`

**Format**: JSON array, order matters

**Example**:
```json
{
  "mods": [
    {
      "name": "APFramework",
      "enabled": true
    },
    {
      "name": "PalworldAPMod",
      "enabled": true
    }
  ]
}
```

**Steps**:
1. Open `ue4ss/Mods/mods.json` in text editor
2. Ensure APFramework is **first in the array**
3. All AP-enabled mods come after
4. Save file
5. Restart Palworld

## C++ AP-Enabled Mods (Advanced)

C++ mods load before Lua mods, so they must handle delayed registration:

```cpp
// Bad: Immediate registration (framework not ready yet)
void on_load() {
    APClientHandle client = ap_client_create("mymod.palworld.mymod");
    ap_client_register(client, capabilities_json);  // May fail!
}

// Good: Delayed registration with polling
APClientHandle g_client = nullptr;
bool g_registered = false;

void on_load() {
    g_client = ap_client_create("mymod.palworld.mymod");
    // Don't register yet, framework not ready
}

void on_tick() {
    if (!g_registered) {
        // Poll for registration
        ap_client_poll(g_client);

        // Attempt registration
        if (ap_client_register(g_client, capabilities_json)) {
            g_registered = true;
        }
    } else {
        // Normal operation
        ap_client_poll(g_client);
    }
}
```

The APClientLib handles reconnection automatically. Just keep calling `ap_client_poll()`.

## Troubleshooting

### Problem: "Registration timeout" in console

**Cause**: Mod tried to register before framework started IPC server.

**Solution**: Verify load order, APFramework must be first.

### Problem: Mod not discovered

**Cause 1**: `enabled.txt` missing from mod folder.
**Solution**: Create `YourMod/enabled.txt` (empty file).

**Cause 2**: Mod loaded before APFramework.
**Solution**: Fix load order (see above).

### Problem: C++ mod fails to register

**Cause**: C++ mod trying to register before framework ready.

**Solution**: Implement delayed registration pattern (see above).

### Verification

After configuring load order, check logs:

**UE4SS Console**:
```
[APFramework] [INFO] IPC server started
[APFramework] [INFO] Discovered 2 mods: ...
[APFramework] [INFO] All mods registered
```

**framework.log**:
```
[2026-01-02 14:23:45.123] [INFO] FrameworkCore created
[2026-01-02 14:23:45.456] [INFO] IPC server started on pipe: APFramework_default
[2026-01-02 14:23:46.789] [INFO] Discovered 2 mods: mymod.palworld.mod1, mymod.palworld.mod2
[2026-01-02 14:23:47.123] [INFO] Mod registered: mymod.palworld.mod1
[2026-01-02 14:23:47.456] [INFO] Mod registered: mymod.palworld.mod2
[2026-01-02 14:23:47.789] [INFO] All mods registered
```

## Summary

✅ APFramework must load **first** among AP-enabled mods
✅ Configure via `mods.txt` (recommended) or `mods.json`
✅ C++ mods need delayed registration pattern
✅ Verify with console logs and `framework.log`
```

---

## Feature 2.4: C++ Mod Example

### Problem Statement

**Current State**: No reference implementation for C++ mods.

**Intended Design**: Complete working example.

**Impact**: C++ developers have no starting point.

### Solution Design

#### Example Mod Structure

```
examples/cpp_mod_example/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp
│   └── dllmain.cpp
├── ap_config.json
└── enabled.txt
```

### Implementation Steps

#### Step 1: Create Example Mod (3 hours)

**File**: [examples/cpp_mod_example/README.md](../../../examples/cpp_mod_example/README.md)

**Contents**:
```markdown
# C++ Mod Example for APFramework

This example demonstrates how to create a C++ UE4SS mod that uses APFramework.

## Features Demonstrated

- Delayed registration pattern (handles early C++ mod loading)
- Item received callback
- Location checking
- Logging to framework
- Error handling

## Building

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Output: `build/bin/Release/CPPModExample.dll`

## Installation

1. Copy `CPPModExample.dll` to `Palworld/ue4ss/Mods/CPPModExample/`
2. Copy `ap_config.json` to same folder
3. Create `enabled.txt` in same folder
4. Configure load order (see [docs/LOAD_ORDER.md](../../docs/LOAD_ORDER.md))

## Usage

The mod will:
1. Attempt registration when loaded
2. Retry every second until successful
3. Log all activities to framework log
4. Grant items when received from AP server
5. Check locations when triggered
```

**File**: [examples/cpp_mod_example/src/main.cpp](../../../examples/cpp_mod_example/src/main.cpp)

```cpp
#include <Unreal/UObjectGlobals.hpp>
#include <DynamicOutput/DynamicOutput.hpp>
#include "ap_client_lib.h"

using namespace RC;
using namespace RC::Unreal;

// Global state
APClientHandle g_client = nullptr;
bool g_registered = false;
bool g_connected = false;

// Callbacks
void on_item_received(int64_t item_id, int64_t location_id, int player_slot, void* user_data) {
    Output::send(STR("[CPPModExample] Received item: {} from location: {} (player {})\n"),
                 item_id, location_id, player_slot);

    // Grant item to player (game-specific logic)
    // Example: UnlockTechnology(item_id);

    ap_client_log(g_client, "INFO", "Item granted successfully");
}

void on_registration_complete(void* user_data) {
    Output::send(STR("[CPPModExample] Registration complete! Mod is active.\n"));
    g_registered = true;
}

void on_connection_status(bool connected, const char* slot_name, void* user_data) {
    g_connected = connected;

    if (connected) {
        Output::send(STR("[CPPModExample] Connected to AP as: {}\n"), slot_name);
    } else {
        Output::send(STR("[CPPModExample] Disconnected from AP\n"));
    }
}

// UE4SS hooks
void on_program_start() {
    Output::send(STR("[CPPModExample] Mod loading...\n"));

    // Create client (IPC connection will be attempted)
    g_client = ap_client_create("example.palworld.cpp_mod");

    if (!g_client) {
        Output::send(STR("[CPPModExample] ERROR: Failed to create client\n"));
        return;
    }

    // Set callbacks
    ap_client_set_item_received_callback(g_client, on_item_received, nullptr);
    ap_client_set_registration_complete_callback(g_client, on_registration_complete, nullptr);
    ap_client_set_connection_status_callback(g_client, on_connection_status, nullptr);

    // Set log callback (optional - logs will also go to framework)
    ap_client_set_log_callback(g_client, [](const char* level, const char* msg, void* data) {
        Output::send(STR("[CPPModExample] [{}] {}\n"), level, msg);
    }, nullptr);

    ap_client_log(g_client, "INFO", "Mod initialized, waiting for framework...");

    Output::send(STR("[CPPModExample] Mod loaded, awaiting registration...\n"));
}

void on_unreal_init() {
    // Called after Unreal Engine initializes
    // Good place to set up game hooks
}

void on_update() {
    // Called every frame

    if (!g_client) return;

    // Always poll (handles reconnection automatically)
    ap_client_poll(g_client);

    // Attempt registration if not yet registered
    if (!g_registered) {
        static int retry_counter = 0;
        retry_counter++;

        // Try registration every 60 frames (~1 second)
        if (retry_counter >= 60) {
            retry_counter = 0;

            // Build capabilities JSON
            const char* capabilities = R"({
                "items": [
                    {"id": 100000, "name": "Example Item", "classification": "useful"}
                ],
                "locations": [
                    {"id": 200000, "name": "Example Location", "region": "Test Region"}
                ],
                "regions": [
                    {"name": "Test Region", "connects_to": []}
                ]
            })";

            if (ap_client_register(g_client, capabilities)) {
                // Registration sent, wait for confirmation via callback
                ap_client_log(g_client, "INFO", "Registration request sent");
            }
        }
    }

    // Normal operation when registered and connected
    if (g_registered && g_connected) {
        // Example: Check if player opened a chest
        // if (chest_opened) {
        //     ap_client_check_location(g_client, 200000);
        // }
    }
}
```

**File**: [examples/cpp_mod_example/CMakeLists.txt](../../../examples/cpp_mod_example/CMakeLists.txt)

```cmake
cmake_minimum_required(VERSION 3.18)
project(CPPModExample)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add APClientLib
add_subdirectory(${CMAKE_SOURCE_DIR}/../../src/client_lib client_lib)

# UE4SS SDK paths (adjust as needed)
set(UE4SS_SDK_PATH "path/to/ue4ss/SDK" CACHE PATH "Path to UE4SS SDK")

# Create DLL
add_library(CPPModExample SHARED
    src/main.cpp
    src/dllmain.cpp
)

target_link_libraries(CPPModExample PRIVATE
    APClientLib
    # UE4SS libraries...
)

target_include_directories(CPPModExample PRIVATE
    ${UE4SS_SDK_PATH}/include
)

# Output to bin/Release
set_target_properties(CPPModExample PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
)
```

**File**: [examples/cpp_mod_example/ap_config.json](../../../examples/cpp_mod_example/ap_config.json)

```json
{
  "schema_version": 2,
  "mod_id": "example.palworld.cpp_mod",
  "version": "1.0.0",
  "display_name": "C++ Mod Example",
  "description": "Example C++ mod demonstrating APFramework integration",
  "supported_game_versions": ">=0.3.0",
  "incompatible_mods": [],
  "capabilities": {
    "items": [
      {
        "id": 100000,
        "name": "Example Item",
        "classification": "useful"
      }
    ],
    "locations": [
      {
        "id": 200000,
        "name": "Example Location",
        "region": "Test Region"
      }
    ],
    "regions": [
      {
        "name": "Test Region",
        "connects_to": []
      }
    ]
  }
}
```

### Testing Plan

**Build Test**:
1. Run CMake configure
2. Run CMake build
3. Verify DLL created

**Runtime Test**:
1. Copy DLL to UE4SS mods folder
2. Configure load order
3. Launch game
4. Verify console messages
5. Check framework.log for mod messages
6. Test item receipt (if possible)

---

## Integration and Testing

### Integration Test Sequence

**Test 1: Mod ID Validation**
- Create mod with invalid ID
- Verify rejection with helpful error
- Fix ID, verify acceptance

**Test 2: Client Library Logging**
- Create test mod using logging API
- Verify logs appear in framework.log
- Verify callback fires (if set)

**Test 3: Load Order**
- Follow documentation to configure
- Verify framework loads first
- Verify mods load after

**Test 4: C++ Example**
- Build example mod
- Install and configure
- Verify delayed registration works

---

## Documentation Requirements

**Updated Files**:
- README.md
- docs/LOAD_ORDER.md (new)
- examples/cpp_mod_example/README.md (new)
- docs/API_REFERENCE.md (add logging section)

---

## Success Metrics

- [ ] 100% of invalid mod IDs rejected with clear error
- [ ] 100% of mods can log to framework
- [ ] Users can configure load order without help
- [ ] C++ example compiles and runs
- [ ] Developer satisfaction: Clear path to get started

---

**End of Phase 2 Plan**