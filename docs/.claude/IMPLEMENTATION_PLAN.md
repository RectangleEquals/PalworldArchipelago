# APFramework IPC Branch - Implementation Plan

**Version**: 2.0.0
**Last Updated**: January 1, 2026
**Status**: Implementation Phase - Phase 1 (Headers Complete)

---

## Table of Contents
1. [Development Environment](#development-environment)
2. [Phase 1: C++ Framework Core](#phase-1-c-framework-core)
3. [Phase 2: C++ Client Library](#phase-2-c-client-library)
4. [Phase 3: Lua Client Wrapper](#phase-3-lua-client-wrapper)
5. [Phase 4: Framework Lua Mod](#phase-4-framework-lua-mod)
6. [Phase 5: Example Mods](#phase-5-example-mods)
7. [Phase 6: Testing & Documentation](#phase-6-testing--documentation)
8. [Build System](#build-system)
9. [Testing Strategy](#testing-strategy)

---

## Development Environment

### Required Tools

**C++ Development**:
- Visual Studio 2022 (or VS Build Tools)
- CMake 3.20+
- C++17 compiler (MSVC recommended for Windows)

**Lua Development**:
- LuaJIT (included with UE4SS)
- Lua 5.4 (for testing outside UE4SS)

**Testing**:
- Google Test (C++ unit tests)
- Lua busted (Lua unit tests)

**Dependencies**:
- **apclientpp** (C++ library for Archipelago protocol) - NOT lua-apclientpp
- **nlohmann/json** (C++ JSON library)
- **Windows SDK** (Named Pipes API)

### Directory Structure

```
ipc_branch/
├── docs/                         # Documentation (public)
│   ├── ARCHITECTURE.md
│   ├── PROJECT_STATE.md
│   └── .claude/                  # Internal docs
│       └── IMPLEMENTATION_PLAN.md
├── src/                          # Source code
│   ├── framework_core/           # APFrameworkCore.dll
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   ├── src/
│   │   └── tests/
│   ├── client_lib/               # APClientLib.dll
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   ├── src/
│   │   └── tests/
│   └── lua_client/               # ap_client.lua
│       ├── ap_client.lua
│       └── tests/
├── APFramework/                  # APFramework UE4SS mod (installed to ue4ss/mods/)
│   ├── Scripts/
│   │   ├── main.lua
│   │   ├── APFramework.lua
│   │   └── lib/
│   │       ├── APFrameworkCore.dll
│   │       ├── ap_client.lua
│   │       └── windows_pipe.lua
│   └── config.json
├── examples/                     # Example mods
│   ├── lua_example/
│   ├── cpp_example/
│   └── bp_companion_example/
├── worlds/                       # Archipelago world package
│   └── palworld/
│       ├── __init__.py
│       ├── items.py
│       ├── locations.py
│       ├── regions.py
│       └── ...
├── test_yamls/                   # Test YAML configurations
│   └── example_palworld.yaml
├── build/                        # Build output (gitignored)
├── CMakeLists.txt                # Root CMake
├── LICENSE                       # Project license
└── README.md                     # Branch overview
```

---

## Phase 1: C++ Framework Core

**Goal**: Implement `APFrameworkCore.dll` - the heart of the framework

### 1.1 Project Setup ✅ COMPLETE

**File**: `src/framework_core/CMakeLists.txt`

**Status**: ✅ Created and configured

```cmake
cmake_minimum_required(VERSION 3.20)
project(APFrameworkCore VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(nlohmann_json REQUIRED)
find_package(apclientpp REQUIRED)

# Source files
set(SOURCES
    src/ap_client.cpp
    src/ipc_server.cpp
    src/message_router.cpp
    src/mod_registry.cpp
    src/polling_thread.cpp
    src/config_manager.cpp
    src/capabilities_generator.cpp
    src/ffi_bindings.cpp
    src/framework_core.cpp
    src/main.cpp
)

# Headers
set(HEADERS
    include/ap_client.h
    include/ipc_server.h
    include/message_queue.h
    include/message_router.h
    include/mod_registry.h
    include/polling_thread.h
    include/config_manager.h
    include/capabilities_generator.h
    include/framework_core.h
    include/ffi_bindings.h
)

# Build DLL
add_library(APFrameworkCore SHARED ${SOURCES} ${HEADERS})

target_include_directories(APFrameworkCore PUBLIC include)
target_link_libraries(APFrameworkCore
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE apclientpp::apclientpp
    PRIVATE ws2_32  # Windows sockets for networking
)
```

**Changes from original plan**:
- Added `config_manager.cpp` and `config_manager.h` for configuration/profile management
- Added `capabilities_generator.cpp` and `capabilities_generator.h` for APCapabilities.json generation
- Added `framework_core.cpp` and `framework_core.h` as main orchestrator
- Removed `message_queue.cpp` (header-only template)
- All components use `APFramework` namespace (not `apframework`)

### 1.2 Core Components ✅ HEADERS COMPLETE

**Status**: All header files created with complete documentation and `APFramework` namespace

#### 1. FrameworkCore (Main Orchestrator)

**File**: `src/framework_core/include/framework_core.h` ✅

**Purpose**: Central coordinator for all framework components

**Key responsibilities**:
- Owns all component instances (APClient, IPC, Router, Registry, etc.)
- Coordinates component lifecycle
- Handles IPC message dispatch
- Manages mod registration flow
- Provides FFI binding implementations

#### 2. APClientWrapper

**File**: `src/framework_core/include/ap_client.h` ✅

**Namespace**: `APFramework` (updated from original plan)

**Key changes from original plan**:
- Renamed from `APClient` to `APClientWrapper` to avoid confusion
- Uses `APFramework` namespace
- Forward declares `APClient::APClient` from apclientpp to avoid header exposure
- Thread-safe message queuing with mutex

```cpp
namespace APFramework {
    struct APMessage {
        enum Type {
            ItemReceived, LocationChecked, SlotConnected,
            Disconnected, RoomInfo, DataPackage
        };
        Type type;
        int64_t item_id;
        int64_t location_id;
        int player_slot;
        std::string data_json;
    };

    class APClientWrapper {
        // ... (see actual header for full API)
    };
}
```

#### 3. IPCServer

**File**: `src/framework_core/include/ipc_server.h` ✅

**Namespace**: `APFramework`

**Key changes**:
- Added `send_to_all_mods()` for broadcast messages
- Uses Windows `HANDLE` type explicitly
- Separate maps for pipe handles and message queues
- Thread-safe with multiple mutexes

```cpp
namespace APFramework {
    struct IPCMessage {
        std::string type;
        std::string mod_id;
        std::string data_json;
    };

    class IPCServer {
        // Runs on dedicated thread
        // Manages per-mod message queues
        // Handles bidirectional Named Pipe communication
    };
}
```

#### 4. MessageQueue (Header-Only Template)

**File**: `src/framework_core/include/message_queue.h` ✅ COMPLETE

**Namespace**: `APFramework`

**Implementation status**: Fully implemented as header-only template

**Key features**:
- Thread-safe push/pop operations
- Blocking and non-blocking pop modes
- Condition variable for efficient blocking waits
- Used throughout framework for inter-thread communication

**Note**: This is header-only, so no .cpp file needed

#### 5. PollingThread

**File**: `src/framework_core/include/polling_thread.h` ✅

**Namespace**: `APFramework`

**Key changes**:
- Added configurable poll interval (default 16ms for 60fps)
- Uses `std::chrono::milliseconds` for timing
- Getter/setter for poll interval

```cpp
namespace APFramework {
    class PollingThread {
        // Dedicated background thread
        // Continuously polls APClientWrapper
        // Routes messages via MessageRouter
        // Configurable interval (default 16ms)
    };
}
```

#### 6. MessageRouter

**File**: `src/framework_core/include/message_router.h` ✅

**Namespace**: `APFramework`

**Key features**:
- Routes AP messages based on item/location ownership
- Bulk registration from mod capabilities
- Thread-safe routing with mutex
- Query methods for debugging

```cpp
namespace APFramework {
    class MessageRouter {
        // Maintains item_id → mod_id mapping
        // Maintains location_id → mod_id mapping
        // Routes AP messages to correct mods via IPC
    };
}
```

#### 7. ModRegistry

**File**: `src/framework_core/include/mod_registry.h` ✅

**Namespace**: `APFramework`

**Purpose**: Promise-based mod registration system

**Key responsibilities**:
- Discovery phase: Scan for ap_config.json files
- Registration phase: Wait for mods to connect via IPC
- Completion: All discovered mods registered → ready for AP connection
- Generate APCapabilities.json from registered mods

#### 8. ConfigManager (NEW)

**File**: `src/framework_core/include/config_manager.h` ✅

**Namespace**: `APFramework`

**Purpose**: Configuration and connection profile management

**Key features**:
- Load/save config.json
- Connection profiles (server, port, slot, password)
- Profile switching
- Framework settings (polling interval, logging, etc.)
- Thread-safe access

#### 9. CapabilitiesGenerator (NEW)

**File**: `src/framework_core/include/capabilities_generator.h` ✅

**Namespace**: `APFramework`

**Purpose**: Generate APCapabilities.json from registered mod data

**Key features**:
- Collect items, locations, regions from all mods
- Merge capabilities into single JSON
- Validate ID ranges (no conflicts)
- Validate region connections
- Write to disk

### 1.3 FFI Bindings ✅ HEADERS COMPLETE

**File**: `src/framework_core/include/ffi_bindings.h` ✅

**Key changes from original plan**:
- Uses `FrameworkHandle` typedef instead of raw `void*`
- Added comprehensive documentation for all functions
- Added mod discovery functions
- Added configuration management functions
- Added capabilities generation functions
- Uses opaque handle pattern to hide C++ implementation

**New functions** (not in original plan):
- `framework_core_discover_mods()` - Auto-discovery of AP-enabled mods
- `framework_core_all_mods_registered()` - Check registration status
- `framework_core_generate_capabilities()` - Generate APCapabilities.json
- `framework_core_load_config()` - Load configuration
- `framework_core_save_config()` - Save configuration
- `framework_core_free_string()` - Memory management for returned strings

**Status**: Header complete, implementation pending

### 1.4 Implementation Notes

**Threading Safety**:
- All public methods must be thread-safe
- Use mutexes for shared state
- Prefer lock-free queues where possible

**Error Handling**:
- Exceptions should not cross DLL boundary
- Return error codes to Lua
- Log errors internally

**Memory Management**:
- Use RAII for all resources
- No raw pointers in public API
- Clear ownership semantics

**Performance**:
- Minimize lock contention
- Batch message processing
- Preallocate buffers

---

## Phase 2: C++ Client Library

**Goal**: Implement `APClientLib.dll` for C++ mods

### 2.1 Project Setup

**File**: `src/client_lib/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)
project(APClientLib VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# Source files
set(SOURCES
    src/ipc_client.cpp
    src/ap_client_api.cpp
)

set(HEADERS
    include/ap_client_lib.h
    include/ipc_client.h
)

# Build DLL
add_library(APClientLib SHARED ${SOURCES} ${HEADERS})
target_include_directories(APClientLib PUBLIC include)
```

### 2.2 Public API

**File**: `src/client_lib/include/ap_client_lib.h`

```cpp
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Handle type
typedef void* APClientHandle;

// Message types
enum APMessageType {
    AP_MSG_ITEM_RECEIVED = 0,
    AP_MSG_LOCATION_CHECKED = 1,
    AP_MSG_CONNECTION_STATUS = 2,
    AP_MSG_ERROR = 99
};

// Message structure
struct APClientMessage {
    APMessageType type;
    int64_t item_id;
    int64_t location_id;
    int player_slot;
    char data_json[4096]; // Full message as JSON
};

// API Functions
__declspec(dllexport) APClientHandle ap_client_connect(const char* mod_id);
__declspec(dllexport) void ap_client_disconnect(APClientHandle handle);
__declspec(dllexport) bool ap_client_is_connected(APClientHandle handle);

__declspec(dllexport) int ap_client_poll_messages(APClientHandle handle,
    APClientMessage* out_messages, int max_messages);

__declspec(dllexport) void ap_client_check_location(APClientHandle handle,
    int64_t location_id);

__declspec(dllexport) void ap_client_status_update(APClientHandle handle,
    int status);

#ifdef __cplusplus
}
#endif
```

### 2.3 Implementation

**IPC Client** (connects to framework's Named Pipe):

```cpp
// src/client_lib/src/ipc_client.cpp
class IPCClient {
    HANDLE pipe_handle_;
    std::string mod_id_;
    MessageQueue<APClientMessage> incoming_queue_;
    std::thread read_thread_;

    void read_loop() {
        while (running_) {
            // Read from Named Pipe
            char buffer[8192];
            DWORD bytes_read;
            if (ReadFile(pipe_handle_, buffer, sizeof(buffer), &bytes_read, nullptr)) {
                // Parse JSON and enqueue
                auto msg = parse_message(buffer, bytes_read);
                incoming_queue_.push(msg);
            }
        }
    }
};
```

---

## Phase 3: Lua Client Wrapper

**Goal**: Implement `ap_client.lua` for Lua mods

### 3.1 Module Structure

**File**: `src/lua_client/ap_client.lua`

```lua
local APClient = {}
APClient.__index = APClient

-- Named Pipe wrapper (Windows)
local pipe_lib = require("lib.windows_pipe") -- Lua FFI wrapper for Named Pipes

function APClient.new(mod_id)
    local self = setmetatable({}, APClient)
    self.mod_id = mod_id
    self.pipe = nil
    self.connected = false
    self.callbacks = {}
    self.message_queue = {}
    return self
end

function APClient:Connect()
    -- Connect to \\.\pipe\APFramework_<instance>
    local pipe_name = "\\\\.\\pipe\\APFramework_default"
    self.pipe = pipe_lib.open(pipe_name)

    if not self.pipe then
        return false
    end

    -- Send register message
    local register_msg = {
        type = "register",
        mod_id = self.mod_id,
        data = {}
    }

    self:SendMessage(register_msg)
    self.connected = true
    return true
end

function APClient:SendMessage(msg)
    local json = require("lib.lunajson")
    local json_str = json.encode(msg)
    pipe_lib.write(self.pipe, json_str .. "\n")
end

function APClient:PollMessages()
    if not self.connected then return {} end

    local messages = {}
    local json = require("lib.lunajson")

    -- Non-blocking read
    while true do
        local line = pipe_lib.read_line(self.pipe, false) -- non-blocking
        if not line then break end

        local msg = json.decode(line)
        table.insert(messages, msg)
    end

    return messages
end

function APClient:ProcessMessages()
    local messages = self:PollMessages()

    for _, msg in ipairs(messages) do
        if msg.type == "item_received" and self.callbacks.item_received then
            self.callbacks.item_received(msg.data)
        elseif msg.type == "location_checked" and self.callbacks.location_checked then
            self.callbacks.location_checked(msg.data)
        end
    end
end

function APClient:OnItemReceived(callback)
    self.callbacks.item_received = callback
end

function APClient:OnLocationChecked(callback)
    self.callbacks.location_checked = callback
end

function APClient:CheckLocation(location_id)
    local msg = {
        type = "location_check",
        mod_id = self.mod_id,
        data = {
            location_id = location_id
        }
    }
    self:SendMessage(msg)
end

return APClient
```

### 3.2 Windows Pipe Wrapper

**File**: `src/lua_client/lib/windows_pipe.lua`

```lua
local ffi = require("ffi")

ffi.cdef[[
    void* CreateFileA(const char*, uint32_t, uint32_t, void*, uint32_t, uint32_t, void*);
    int WriteFile(void*, const char*, uint32_t, uint32_t*, void*);
    int ReadFile(void*, char*, uint32_t, uint32_t*, void*);
    int CloseHandle(void*);
]]

local kernel32 = ffi.load("kernel32")

local M = {}

function M.open(pipe_name)
    local GENERIC_READ = 0x80000000
    local GENERIC_WRITE = 0x40000000
    local OPEN_EXISTING = 3

    local handle = kernel32.CreateFileA(
        pipe_name,
        bit.bor(GENERIC_READ, GENERIC_WRITE),
        0, nil, OPEN_EXISTING, 0, nil
    )

    if handle == ffi.cast("void*", -1) then
        return nil
    end

    return handle
end

function M.write(handle, data)
    local bytes_written = ffi.new("uint32_t[1]")
    kernel32.WriteFile(handle, data, #data, bytes_written, nil)
    return bytes_written[0]
end

function M.read_line(handle, blocking)
    -- Implementation: read until newline
    -- (simplified - actual implementation would handle buffering)
    local buffer = ffi.new("char[8192]")
    local bytes_read = ffi.new("uint32_t[1]")

    if kernel32.ReadFile(handle, buffer, 8192, bytes_read, nil) ~= 0 then
        return ffi.string(buffer, bytes_read[0])
    end

    return nil
end

function M.close(handle)
    kernel32.CloseHandle(handle)
end

return M
```

---

## Phase 4: Framework Lua Mod

**Goal**: Implement APFramework UE4SS mod that loads C++ core

### 4.1 Entry Point

**File**: `APFramework/Scripts/main.lua`

```lua
print("=== APFramework v2.0 (IPC) Loading ===")

-- Load framework wrapper
local APFramework = require("APFramework")

-- Initialize
local success = APFramework:Initialize()

if success then
    print("[APFramework] Framework initialized successfully!")
else
    print("[APFramework] ERROR: Failed to initialize framework")
end

print("=== APFramework Loading Complete ===")

return APFramework
```

### 4.2 Framework Wrapper

**File**: `APFramework/Scripts/APFramework.lua`

```lua
local APFramework = {}

local ffi = require("ffi")
local json = require("lib.lunajson")

-- Load C++ core
ffi.cdef[[
    void* framework_core_create(const char* pipe_name);
    void framework_core_destroy(void* handle);
    void framework_core_start_ipc(void* handle);
    bool framework_core_connect_ap(void* handle, const char* host, int port,
        const char* slot, const char* password);
    void framework_core_start_polling(void* handle);
]]

local core_dll = ffi.load("lib/APFrameworkCore.dll")
local core_handle = nil

function APFramework:Initialize()
    print("[APFramework] Loading C++ core...")

    -- Create framework core
    core_handle = core_dll.framework_core_create("\\\\.\\pipe\\APFramework_default")
    if core_handle == nil then
        print("[APFramework] ERROR: Failed to create framework core")
        return false
    end

    -- Start IPC server
    print("[APFramework] Starting IPC server...")
    core_dll.framework_core_start_ipc(core_handle)

    -- Auto-discover mods (scans for ap_config.json files)
    print("[APFramework] Discovering AP-enabled mods...")
    self:DiscoverMods()

    -- Wait for mod registrations (registration phase)
    print("[APFramework] Waiting for mod registrations...")
    -- Note: Registration happens asynchronously via IPC
    -- Framework will generate APCapabilities.json once all discovered mods register

    -- Optional: Auto-connect if configured
    local config = self:LoadConfig()
    if config and config.ap_connection and config.ap_connection.autoconnect then
        print("[APFramework] Auto-connect enabled, will connect after registration")

        -- Register callback for registration_complete event
        self:OnRegistrationComplete(function()
            print("[APFramework] Registration complete, initiating auto-connect...")

            local success = core_dll.framework_core_connect_ap(
                core_handle,
                config.ap_connection.server,
                config.ap_connection.port,
                config.ap_connection.slot_name,
                config.ap_connection.password or ""
            )

            if success then
                print("[APFramework] Connected to AP server successfully")
                core_dll.framework_core_start_polling(core_handle)
                print("[APFramework] Background polling started")
            else
                print("[APFramework] Warning: Failed to connect to AP server")
            end
        end)
    else
        print("[APFramework] Auto-connect disabled - waiting for connection request")
    end

    return true
end

function APFramework:DiscoverMods()
    -- Scan ../*/ap_config.json
    -- (implementation details)
end

function APFramework:LoadConfig()
    local file = io.open("config.json", "r")
    if not file then return nil end

    local content = file:read("*all")
    file:close()

    return json.decode(content)
end

return APFramework
```

---

## Build System

### Root CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(APFramework VERSION 2.0.0)

# Options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build example mods" ON)

# Subdirectories
add_subdirectory(src/framework_core)
add_subdirectory(src/client_lib)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
```

### Build Instructions

```bash
# Create build directory
mkdir build
cd build

# Configure
cmake .. -G "Visual Studio 17 2022" -A x64

# Build
cmake --build . --config Release

# Run tests
ctest -C Release
```

---

## Testing Strategy

### Unit Tests (C++)

**Framework Core Tests**:
- IPC server connection handling
- Message queue thread safety
- Message routing logic
- AP client integration

**Client Library Tests**:
- IPC client connection
- Message polling
- API correctness

### Integration Tests

**Framework + Client Tests**:
- Full connection flow
- Message round-trip (mod → framework → mod)
- Multi-mod scenarios
- Error handling

### Performance Tests

- IPC latency measurement
- Throughput testing (messages/sec)
- Memory leak detection
- Thread contention analysis

### Manual Testing

- Example mods in actual game
- Load order testing
- Compatibility with other mods

---

## Next Steps

1. Set up development environment
2. Implement Phase 1 (Framework Core)
3. Implement Phase 2 (Client Library)
4. Implement Phase 3 (Lua Wrapper)
5. Implement Phase 4 (Framework Mod)
6. Create example mods
7. Test and document

**Estimated Timeline**: 3-4 weeks for full implementation