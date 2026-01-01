# Phase 1: C++ Framework Core (APFrameworkCore.dll)

## Overview

Phase 1 implements the core C++ library that handles:
- Archipelago server communication (via apclientpp)
- IPC server for mod communication (Windows Named Pipes)
- Mod discovery and registration tracking
- Message routing between AP and mods
- Configuration management

## Build Output

- **APFrameworkCore.dll** - Main framework library
- Exports C API via FFI bindings for Lua integration
- Dependencies: apclientpp, nlohmann/json, asio, websocketpp, wswrap

## Component Architecture

```
FrameworkCore
├── APClientWrapper      (AP server communication)
├── IPCServer           (Named Pipes IPC server)
├── MessageRouter       (Route messages between AP and mods)
├── ModRegistry         (Track discovered/registered mods)
├── PollingThread       (Background AP message polling)
├── ConfigManager       (Configuration and profiles)
└── CapabilitiesGenerator (Generate APCapabilities.json)
```

## Header Files Status

All headers are **COMPLETE** and serve as the source of truth:

### Core Components

#### [framework_core.h](../../../src/framework_core/include/framework_core.h)
**Purpose**: Main orchestrator coordinating all components
**Key Methods**:
- `start_ipc()`, `stop_ipc()` - IPC server lifecycle
- `connect_ap()`, `disconnect_ap()` - AP connection management
- `discover_mods()` - Scan for ap_config.json files
- `all_mods_registered()` - Check if registration complete
- `generate_capabilities()` - Generate APCapabilities.json
- `start_polling()`, `stop_polling()` - Polling thread control
- `load_config()`, `save_config()` - Configuration management

**Lifecycle**:
1. Create instance with pipe name
2. Load config (auto-loads on startup)
3. Start IPC server
4. Discover mods
5. Wait for all discovered mods to register
6. Generate APCapabilities.json
7. Broadcast "registration_complete"
8. Wait for connection request (autoconnect or explicit IPC)
9. Connect to AP server
10. Start polling thread

#### [ap_client.h](../../../src/framework_core/include/ap_client.h)
**Purpose**: Wrapper around apclientpp library
**Key Features**:
- Connection management (`connect()`, `disconnect()`, `is_connected()`)
- Continuous polling (`poll()`) - called by PollingThread
- Message queuing (`get_messages()`) - thread-safe retrieval
- Commands (`check_location()`, `status_update()`)
- Callback registration for AP events

**Message Types** (APMessage::Type):
- `ItemReceived` - Player received an item
- `LocationChecked` - A location was checked
- `SlotConnected` - Successfully connected to slot
- `Disconnected` - Disconnected from server
- `RoomInfo` - Room information received
- `DataPackage` - Data package received

#### [ipc_server.h](../../../src/framework_core/include/ipc_server.h)
**Purpose**: Windows Named Pipes IPC server
**Key Features**:
- Listens on named pipe for mod connections
- Per-mod message queues
- Thread-safe message sending (`send_to_mod()`, `send_to_all_mods()`)
- Mod registration tracking
- Dispatches incoming messages to handler function

**Message Format** (IPCMessage):
```cpp
struct IPCMessage {
    std::string type;      // "register", "location_check", "connect", etc.
    std::string mod_id;    // Sender/receiver mod ID
    std::string data_json; // Payload as JSON
};
```

#### [message_router.h](../../../src/framework_core/include/message_router.h)
**Purpose**: Routes AP messages to appropriate mods
**Key Features**:
- Maintains item_id → mod_id and location_id → mod_id mappings
- Routes `ItemReceived` and `LocationChecked` to specific mods
- Broadcasts connection status messages to all mods
- Populates routing tables from mod capabilities

**Routing Logic**:
- Targeted: ItemReceived, LocationChecked (uses routing tables)
- Broadcast: SlotConnected, Disconnected, RoomInfo, DataPackage

#### [mod_registry.h](../../../src/framework_core/include/mod_registry.h)
**Purpose**: Promise-based mod discovery and registration
**Key Features**:
- Auto-discovery: Scans for `ap_config.json` files
- Tracks discovered vs registered mods
- Promise fulfillment: `all_discovered_mods_registered()`
- Returns pending registrations for timeout handling

**ModCapabilities Structure**:
```cpp
struct ModCapabilities {
    std::string mod_id;
    std::vector<int64_t> items;      // Item IDs this mod handles
    std::vector<int64_t> locations;  // Location IDs this mod handles
    std::vector<std::string> regions; // Region names this mod provides
};
```

**Note**: `logging_only` and `priority` fields were removed - not part of design

#### [polling_thread.h](../../../src/framework_core/include/polling_thread.h)
**Purpose**: Background thread for continuous AP polling
**Key Features**:
- Calls `ap_client_->poll()` at regular intervals (default: 16ms ~60fps)
- Retrieves messages via `ap_client_->get_messages()`
- Routes messages via `message_router_->route_ap_message()`
- Configurable poll interval

#### [config_manager.h](../../../src/framework_core/include/config_manager.h)
**Purpose**: Configuration and connection profile management
**Key Features**:
- Manages `FrameworkConfig` with active profile, saved profiles, settings
- Load/save configuration from JSON files
- Profile management (add, remove, switch)
- Auto-loads on framework startup
- Supports runtime reload and profile switching

**FrameworkConfig Structure**:
```cpp
struct FrameworkConfig {
    ConnectionProfile active_profile;
    std::map<std::string, ConnectionProfile> saved_profiles;
    int polling_interval_ms;
    bool enable_logging;
    std::string log_level; // "debug", "info", "warn", "error"
    std::map<std::string, std::string> mod_overrides;
};
```

#### [capabilities_generator.h](../../../src/framework_core/include/capabilities_generator.h)
**Purpose**: Generate APCapabilities.json from registered mod data
**Key Features**:
- Structured types: `ItemDefinition`, `LocationDefinition`, `RegionDefinition`
- Methods to add capabilities per mod
- Bulk registration via `register_mod_from_config()`
- Generates JSON for AP Python server
- Validation methods

**Item/Location/Region Definitions**:
```cpp
struct ItemDefinition {
    int64_t id;
    std::string name;
    std::string classification; // "filler", "useful", "progression", "trap"
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

### Utilities

#### [message_queue.h](../../../src/framework_core/include/message_queue.h)
**Purpose**: Thread-safe message queue template
**Status**: Header-only template - **COMPLETE**, no .cpp file needed

#### [ffi_bindings.h](../../../src/framework_core/include/ffi_bindings.h)
**Purpose**: C API for Lua FFI integration
**Key Features**:
- Opaque `FrameworkHandle` pattern
- All framework operations exposed as C functions
- Memory management helpers (e.g., `framework_core_free_string()`)

## Source Files Status

### ✅ Completed

All source files have been implemented and match their headers:

1. **config_manager.cpp** - ✅ Implements FrameworkConfig structure, auto-load on startup
2. **capabilities_generator.cpp** - ✅ Uses structured types (ItemDefinition, LocationDefinition, RegionDefinition)
3. **ap_client.cpp** - ✅ Implemented with opaque pointer pattern (APClientImpl)
4. **ipc_server.cpp** - ✅ Named Pipes IPC server with IPCMessage handling
5. **message_router.cpp** - ✅ Routes messages between AP and mods
6. **mod_registry.cpp** - ✅ Mod discovery and registration tracking
7. **polling_thread.cpp** - ✅ Background AP polling thread
8. **framework_core.cpp** - ✅ Main orchestrator with complete lifecycle management
9. **ffi_bindings.cpp** - ✅ Exposes all framework_core methods via C API
10. **main.cpp** - ✅ DLL entry point (minimal)

## Implementation Details

### FrameworkCore Lifecycle

```cpp
// 1. Create and initialize
FrameworkCore core("APFramework_default");

// 2. Load config (happens automatically in constructor or on startup)
core.load_config("framework_config.json");

// 3. Start IPC server
core.start_ipc();

// 4. Discover mods
core.discover_mods("C:/Path/To/Mods");

// 5. Wait for registration (in main loop or callback)
while (!core.all_mods_registered()) {
    // Wait with timeout, or handle timeout
}

// 6. Generate capabilities
std::string caps_json = core.generate_capabilities();
// Save to APCapabilities.json

// 7. Connection happens when:
// - Autoconnect enabled in config, OR
// - Mod sends "connect" IPC message

// 8. Start polling after connection
core.start_polling();

// 9. Runtime - messages flow automatically

// 10. Shutdown
core.stop_polling();
core.disconnect_ap();
core.stop_ipc();
```

### IPC Message Handling

FrameworkCore must handle these IPC message types:

```cpp
void FrameworkCore::handle_ipc_message(const IPCMessage& msg) {
    if (msg.type == "register") {
        // Parse capabilities JSON, register mod
        // Check if all mods registered, broadcast if complete
    }
    else if (msg.type == "location_check") {
        // Forward to ap_client_->check_location()
    }
    else if (msg.type == "connect") {
        // Parse connection details, call connect_ap()
    }
    else if (msg.type == "status_update") {
        // Forward to ap_client_->status_update()
    }
    // Special handling for framework Lua wrapper registration
    if (msg.mod_id == "APFramework") {
        // This is the framework's own Lua wrapper
        // Pipe IPC logs to this mod
    }
}
```

### Registration Completion Flow

```cpp
void FrameworkCore::handle_mod_registration(const std::string& mod_id, const std::string& data_json) {
    // 1. Parse mod capabilities from JSON
    // 2. Register in mod_registry_
    // 3. Register in capabilities_generator_
    // 4. Update message_router_ routing tables

    // 5. Check if all discovered mods have registered
    if (mod_registry_->all_discovered_mods_registered()) {
        // 6. Generate APCapabilities.json
        std::string caps = capabilities_generator_->generate_json();
        // Save to file

        // 7. Broadcast registration_complete to all mods
        notify_registration_complete();

        // 8. If autoconnect enabled, connect now
        if (config_manager_->get_active_profile().autoconnect) {
            auto profile = config_manager_->get_active_profile();
            connect_ap(profile.server, profile.port,
                      profile.slot_name, profile.password);
        }
    }
}
```

## Build System

### CMakeLists.txt Structure

**Root** ([CMakeLists.txt](../../../CMakeLists.txt)):
- Adds third_party/apclientpp subdirectory
- Sets up installation rules

**framework_core** ([src/framework_core/CMakeLists.txt](../../../src/framework_core/CMakeLists.txt)):
- Defines source files and headers
- Sets up include directories for dependencies
- Links against apclientpp, ws2_32, crypt32
- Defines preprocessor macros (ASIO_STANDALONE, etc.)

### Dependencies Setup

All dependencies are git submodules in `third_party/`:
- **apclientpp** - AP protocol client (header-only)
- **asio** - Async I/O library (header-only)
- **websocketpp** - WebSocket library (header-only)
- **wswrap** - WebSocket wrapper (header-only)
- **valijson** - JSON schema validation (header-only, optional)
- **nlohmann/json** - Manually added to `include/nlohmann/`

## Testing Strategy

### Unit Tests (Phase 1)
Not yet implemented, but should cover:
- ConfigManager load/save
- ModRegistry discovery and registration
- CapabilitiesGenerator JSON generation
- MessageRouter routing logic

### Integration Tests (Phase 4)
Will be implemented with example mods

## Build Configuration

### Dependency Versions
- **ASIO**: 1.12.2 (downgraded from 1.36.0)
  - websocketpp requires `io_service` which was renamed to `io_context` in ASIO 1.13+
- **websocketpp**: Latest (commit 4dfe1be)
- **apclientpp**: Latest submodule
- **nlohmann/json**: Manually added to include directory

### Preprocessor Definitions
```cmake
ASIO_STANDALONE                      # Use standalone asio (not boost::asio)
WSWRAP_NO_SSL                        # Disable SSL in wswrap (no OpenSSL dependency)
WSWRAP_NO_COMPRESSION                # Disable compression in wswrap (no zlib dependency)
_WEBSOCKETPP_CPP11_RANDOM_DEVICE_    # Use C++11 random device
_WEBSOCKETPP_CPP11_THREAD_           # Use C++11 threading
_WEBSOCKETPP_CPP11_TYPE_TRAITS_      # Use C++11 type traits
_WEBSOCKETPP_CPP11_FUNCTIONAL_       # Use C++11 functional
_WEBSOCKETPP_CPP11_SYSTEM_ERROR_     # Use C++11 system_error
_WEBSOCKETPP_CPP11_MEMORY_           # Use C++11 smart pointers
_WEBSOCKETPP_CPP11_CHRONO_           # Use C++11 chrono
_WIN32_WINNT=0x0601                  # Windows 7+ for networking APIs
WIN32_LEAN_AND_MEAN                  # Reduce Windows.h bloat
ASIO_NO_WIN32_LEAN_AND_MEAN          # But allow asio to include what it needs
```

### Build Output
- **Location**: `build/bin/Release/APFrameworkCore.dll`
- **Size**: 1023 KB (~1 MB)
- **Compiler**: MSVC 14.44 (Visual Studio 2022 Build Tools)

### Known Issues
- Compression disabled: Warning "Archipelago will require compression in the future"
- May need to add zlib support before production release

## Implementation Notes

### APClient Integration (ap_client.cpp)
Used opaque pointer pattern to avoid namespace conflicts:
```cpp
struct APClientImpl {
    std::unique_ptr<::APClient> client;
    std::string uuid;
    std::string game;
    std::string current_uri;

    void reconnect(const std::string& server, int port);
};
```

Key fixes:
- Changed callback parameter types from `std::vector` to `std::list` (apclientpp uses lists)
- Fixed `ConnectSlot()` signature: 3 params (name, password, items_handling)
- URI passed in APClient constructor, not via separate method
- APClient is not copyable, wrapped in `std::unique_ptr` with recreation on reconnect

### Minor Fixes
- Added `#include <set>` to capabilities_generator.cpp
- Fixed opaque pointer pattern in ap_client.h to avoid forward declaration issues

## Phase 1 Status

**✅ COMPLETE** - APFrameworkCore.dll successfully built

All tasks completed:
1. ✅ Clean up headers (remove logging_only, priority)
2. ✅ Implement all source files correctly
3. ✅ Build APFrameworkCore.dll (1023 KB)
4. ⏳ Test with minimal Lua wrapper (deferred to Phase 4)

## Notes

- Headers are the source of truth
- All implementations match header interfaces exactly
- No backward compatibility needed (new branch)
- Minimal external dependencies (SSL and compression disabled)