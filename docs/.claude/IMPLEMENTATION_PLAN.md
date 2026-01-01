# APFramework IPC Branch - Implementation Plan

**Version**: 2.0.0
**Last Updated**: December 31, 2024
**Status**: Design Phase

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
- apclientpp (C++ library for Archipelago protocol)
- nlohmann/json (C++ JSON library)
- Windows SDK (Named Pipes API)

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
├── framework_mod/                # APFramework UE4SS mod
│   ├── Scripts/
│   │   ├── main.lua
│   │   ├── APFramework.lua
│   │   └── lib/
│   └── config.json
├── examples/                     # Example mods
│   ├── lua_example/
│   ├── cpp_example/
│   └── bp_companion_example/
├── build/                        # Build output (gitignored)
└── CMakeLists.txt                # Root CMake
```

---

## Phase 1: C++ Framework Core

**Goal**: Implement `APFrameworkCore.dll` - the heart of the framework

### 1.1 Project Setup

**File**: `src/framework_core/CMakeLists.txt`

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
    src/message_queue.cpp
    src/message_router.cpp
    src/mod_registry.cpp
    src/polling_thread.cpp
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

### 1.2 Core Components

#### APClient Wrapper

**File**: `src/framework_core/include/ap_client.h`

```cpp
#pragma once
#include <string>
#include <functional>
#include <memory>
#include <apclientpp/apclientpp.hpp>

// Internal implementation details
struct APClientImpl;

struct APMessage {
    enum Type {
        ItemReceived,
        LocationChecked,
        SlotConnected,
        Disconnected
    };

    Type type;
    int64_t item_id;
    int64_t location_id;
    int player_slot;
    std::string data_json; // Full message as JSON
};

class APClient {
public:
    APClient(const std::string& uuid, const std::string& game, const std::string& server);
    ~APClient();

    // Connection
    bool connect_slot(const std::string& slot_name, const std::string& password);
    void disconnect();
    bool is_connected() const;
    int get_state() const;

    // Polling
    void poll(); // Call this continuously
    std::vector<APMessage> get_messages(); // Get pending messages

    // Commands
    void check_location(int64_t location_id);
    void status_update(int status);

private:
    std::unique_ptr<APClientImpl> impl;
};
```

#### IPC Server

**File**: `src/framework_core/include/ipc_server.h`

```cpp
#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <map>
#include <functional>
#include "message_queue.h"

struct IPCMessage {
    std::string type;
    std::string mod_id;
    std::string data_json;
};

class IPCServer {
public:
    IPCServer(const std::string& pipe_name);
    ~IPCServer();

    // Lifecycle
    void start();
    void stop();
    bool is_running() const;

    // Message handling
    void send_to_mod(const std::string& mod_id, const IPCMessage& msg);
    void set_message_handler(std::function<void(const IPCMessage&)> handler);

    // Mod management
    void register_mod(const std::string& mod_id);
    void unregister_mod(const std::string& mod_id);

private:
    void server_loop();
    void handle_client(void* pipe_handle);

    std::string pipe_name_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    std::map<std::string, MessageQueue<IPCMessage>> mod_queues_;
    std::function<void(const IPCMessage&)> message_handler_;
};
```

#### Message Queue

**File**: `src/framework_core/include/message_queue.h`

```cpp
#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

template<typename T>
class MessageQueue {
public:
    void push(const T& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(message);
        cond_var_.notify_one();
    }

    std::optional<T> pop(bool blocking = false) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (blocking) {
            cond_var_.wait(lock, [this]{ return !queue_.empty(); });
        } else if (queue_.empty()) {
            return std::nullopt;
        }

        T message = queue_.front();
        queue_.pop();
        return message;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    std::queue<T> queue_;
};
```

#### Polling Thread

**File**: `src/framework_core/include/polling_thread.h`

```cpp
#pragma once
#include <thread>
#include <atomic>
#include <functional>
#include "ap_client.h"
#include "message_router.h"

class PollingThread {
public:
    PollingThread(APClient* client, MessageRouter* router);
    ~PollingThread();

    void start();
    void stop();
    bool is_running() const;

private:
    void polling_loop();

    APClient* ap_client_;
    MessageRouter* router_;
    std::thread thread_;
    std::atomic<bool> running_;
};
```

#### Message Router

**File**: `src/framework_core/include/message_router.h`

```cpp
#pragma once
#include <string>
#include <map>
#include <functional>
#include "ap_client.h"
#include "ipc_server.h"

class MessageRouter {
public:
    MessageRouter(IPCServer* ipc_server);

    // Route AP message to appropriate mod(s)
    void route_ap_message(const APMessage& msg);

    // Register which mod handles which items/locations
    void register_item_handler(int64_t item_id, const std::string& mod_id);
    void register_location_handler(int64_t location_id, const std::string& mod_id);

    // Load routing table from mod capabilities
    void load_routing_table(const std::string& capabilities_json);

private:
    IPCServer* ipc_server_;
    std::map<int64_t, std::string> item_to_mod_;
    std::map<int64_t, std::string> location_to_mod_;
};
```

### 1.3 FFI Bindings

**File**: `src/framework_core/include/ffi_bindings.h`

```cpp
#pragma once

// C API for Lua FFI
extern "C" {
    // Lifecycle
    __declspec(dllexport) void* framework_core_create(const char* pipe_name);
    __declspec(dllexport) void framework_core_destroy(void* handle);

    // IPC Server
    __declspec(dllexport) void framework_core_start_ipc(void* handle);
    __declspec(dllexport) void framework_core_stop_ipc(void* handle);

    // AP Client
    __declspec(dllexport) bool framework_core_connect_ap(void* handle,
        const char* host, int port, const char* slot, const char* password);
    __declspec(dllexport) void framework_core_disconnect_ap(void* handle);
    __declspec(dllexport) int framework_core_get_ap_state(void* handle);

    // Mod Registration
    __declspec(dllexport) void framework_core_register_mod(void* handle,
        const char* mod_id, const char* capabilities_json);

    // Polling
    __declspec(dllexport) void framework_core_start_polling(void* handle);
    __declspec(dllexport) void framework_core_stop_polling(void* handle);
}
```

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

**File**: `framework_mod/Scripts/main.lua`

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

**File**: `framework_mod/Scripts/APFramework.lua`

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

    -- Auto-discover mods
    print("[APFramework] Discovering AP-enabled mods...")
    self:DiscoverMods()

    -- Connect to AP server
    print("[APFramework] Connecting to AP server...")
    local config = self:LoadConfig()
    if config and config.ap_connection and config.ap_connection.enabled then
        local success = core_dll.framework_core_connect_ap(
            core_handle,
            config.ap_connection.server,
            config.ap_connection.port,
            config.ap_connection.slot_name,
            config.ap_connection.password or ""
        )

        if success then
            print("[APFramework] Connected to AP server successfully")

            -- Start polling thread
            core_dll.framework_core_start_polling(core_handle)
            print("[APFramework] Background polling started")
        else
            print("[APFramework] Warning: Failed to connect to AP server")
        end
    else
        print("[APFramework] AP connection disabled in config")
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