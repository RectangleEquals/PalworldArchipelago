# Phase 2: Mod Client Library

## Overview

Phase 2 implements client libraries that mods use to communicate with APFrameworkCore via IPC.

**Two approaches** based on mod type:
1. **C++ Mods**: Use `APClientLib.dll` (C++ library with Named Pipes IPC client)
2. **Lua Mods**: Use `ap_client.lua` (Pure Lua IPC wrapper)

## Goals

- Provide simple, easy-to-use API for mods
- Hide IPC complexity from mod developers
- Thread-safe operations
- Event-driven architecture with callbacks
- Minimal dependencies

## Client Library for C++ Mods (APClientLib.dll)

### Purpose

A lightweight C++ library that C++ mods link against to communicate with APFrameworkCore.

### Architecture

```
APClientLib.dll
├── IPC Client (Named Pipes)
├── Message Queue (thread-safe)
├── Event Callbacks
└── C API (UE4SS compatible)
```

### Key Features

1. **Named Pipes IPC Client**
   - Connects to APFrameworkCore's named pipe
   - Sends messages (register, location_check, etc.)
   - Receives messages from framework

2. **Message Queue**
   - Thread-safe message queue for incoming messages
   - Non-blocking polling

3. **Event Callbacks**
   - `on_item_received(item_id, location_id, player_slot)`
   - `on_location_checked(location_id)`
   - `on_connection_status(connected, slot_name)`
   - `on_registration_complete()`

4. **C API**
   - Compatible with UE4SS C++ mods
   - Opaque handle pattern
   - Memory management helpers

### API Design

```cpp
// Opaque handle
typedef void* APClientHandle;

// Callbacks
typedef void (*ItemReceivedCallback)(int64_t item_id, int64_t location_id, int player_slot, void* user_data);
typedef void (*LocationCheckedCallback)(int64_t location_id, void* user_data);
typedef void (*ConnectionStatusCallback)(bool connected, const char* slot_name, void* user_data);
typedef void (*RegistrationCompleteCallback)(void* user_data);

// Lifecycle
APClientHandle ap_client_create(const char* mod_id);
void ap_client_destroy(APClientHandle handle);

// Registration
bool ap_client_register(APClientHandle handle, const char* capabilities_json);

// Polling
void ap_client_poll(APClientHandle handle);

// Commands
void ap_client_check_location(APClientHandle handle, int64_t location_id);
void ap_client_request_connection(APClientHandle handle,
                                   const char* server, int port,
                                   const char* slot_name, const char* password);

// Callbacks
void ap_client_set_item_received_callback(APClientHandle handle,
                                           ItemReceivedCallback callback,
                                           void* user_data);
void ap_client_set_location_checked_callback(APClientHandle handle,
                                              LocationCheckedCallback callback,
                                              void* user_data);
void ap_client_set_connection_status_callback(APClientHandle handle,
                                               ConnectionStatusCallback callback,
                                               void* user_data);
void ap_client_set_registration_complete_callback(APClientHandle handle,
                                                   RegistrationCompleteCallback callback,
                                                   void* user_data);

// Utility
const char* ap_client_get_last_error(APClientHandle handle);
void ap_client_free_string(const char* str);
```

### Implementation Structure

**Headers** (`src/client_lib/include/`):
- `ap_client_lib.h` - Public C API
- `ipc_client.h` - Named Pipes client (internal)
- `message_queue.h` - Thread-safe queue (can reuse from Phase 1)

**Source** (`src/client_lib/src/`):
- `ap_client_lib.cpp` - Public API implementation
- `ipc_client.cpp` - Named Pipes IPC client
- `callbacks.cpp` - Callback management

### Named Pipes IPC Client

```cpp
class IPCClient {
public:
    IPCClient(const std::string& pipe_name);
    ~IPCClient();

    // Connection
    bool connect();
    void disconnect();
    bool is_connected() const;

    // Messaging
    bool send_message(const std::string& type, const std::string& mod_id, const std::string& data_json);
    bool poll_messages(std::vector<IPCMessage>& out_messages);

private:
    std::string pipe_name_;
    HANDLE pipe_handle_;
    std::mutex mutex_;
};
```

### Usage Example (C++ Mod)

```cpp
#include <ap_client_lib.h>

void on_item_received(int64_t item_id, int64_t location_id, int player_slot, void* user_data) {
    // Handle received item
}

void on_registration_complete(void* user_data) {
    // All mods registered, can now request connection
}

void ModMain() {
    // Create client
    APClientHandle client = ap_client_create("MyModID");

    // Set callbacks
    ap_client_set_item_received_callback(client, on_item_received, nullptr);
    ap_client_set_registration_complete_callback(client, on_registration_complete, nullptr);

    // Register with framework
    const char* capabilities = R"({
        "items": [{"id": 100000, "name": "MyItem", "classification": "progression"}],
        "locations": [{"id": 200000, "name": "MyLocation", "region": "MyRegion"}],
        "regions": [{"name": "MyRegion", "connects_to": []}]
    })";
    ap_client_register(client, capabilities);

    // Game loop
    while (game_running) {
        ap_client_poll(client);
        // ... game logic ...
    }

    // Cleanup
    ap_client_destroy(client);
}
```

## Client Library for Lua Mods (ap_client.lua)

### Purpose

A pure Lua module that Lua mods require to communicate with APFrameworkCore.

### Architecture

```
ap_client.lua
├── Named Pipes wrapper (Lua)
├── JSON serialization
├── Event callbacks
└── Simple API
```

### Key Features

1. **Named Pipes Wrapper**
   - Uses Windows FFI or io.popen for pipe communication
   - Non-blocking reads

2. **JSON Serialization**
   - Serialize/deserialize IPC messages
   - Can use external JSON library or simple implementation

3. **Event Callbacks**
   - Similar to C++ API but Lua-style
   - Callback functions instead of C function pointers

### API Design

```lua
local APClient = {}

-- Create new client
function APClient:new(mod_id)
    -- Returns new APClient instance
end

-- Register with framework
function APClient:register(capabilities)
    -- capabilities: table with items, locations, regions
end

-- Polling
function APClient:poll()
    -- Call each frame to process messages
end

-- Commands
function APClient:check_location(location_id)
end

function APClient:request_connection(server, port, slot_name, password)
end

-- Callbacks (set as fields)
client.on_item_received = function(item_id, location_id, player_slot)
end

client.on_location_checked = function(location_id)
end

client.on_connection_status = function(connected, slot_name)
end

client.on_registration_complete = function()
end
```

### Implementation Approaches

**Option A: FFI-based** (Preferred if available)
```lua
local ffi = require("ffi")

ffi.cdef[[
    void* CreateFileA(const char*, unsigned long, unsigned long, void*, unsigned long, unsigned long, void*);
    int WriteFile(void*, const void*, unsigned long, unsigned long*, void*);
    int ReadFile(void*, void*, unsigned long, unsigned long*, void*);
    int CloseHandle(void*);
]]

local kernel32 = ffi.load("kernel32")
```

**Option B: io.popen** (Fallback)
```lua
-- Use named pipe via file I/O
local pipe = io.open("\\\\.\\pipe\\APFramework_default", "r+b")
```

### Named Pipes Helper (Lua)

```lua
local NamedPipe = {}

function NamedPipe:new(pipe_name)
    local obj = {
        pipe_name = pipe_name,
        handle = nil
    }
    setmetatable(obj, self)
    self.__index = self
    return obj
end

function NamedPipe:connect()
    -- Open named pipe
end

function NamedPipe:send(message_str)
    -- Write to pipe
end

function NamedPipe:receive()
    -- Non-blocking read from pipe
    -- Returns message string or nil
end

function NamedPipe:close()
    -- Close pipe handle
end
```

### Usage Example (Lua Mod)

```lua
local APClient = require("ap_client")

local client = APClient:new("MyLuaMod")

-- Set callbacks
client.on_item_received = function(item_id, location_id, player_slot)
    print("Received item: " .. item_id)
    -- Give item to player
end

client.on_registration_complete = function()
    print("All mods registered!")
end

-- Register capabilities
client:register({
    items = {
        {id = 100000, name = "MyItem", classification = "progression"}
    },
    locations = {
        {id = 200000, name = "MyLocation", region = "MyRegion"}
    },
    regions = {
        {name = "MyRegion", connects_to = {}}
    }
})

-- In game loop
RegisterHook("/Script/Engine.PlayerController:ServerAcknowledgePossession", function()
    client:poll()
end)
```

## Build System

### C++ Client Library (APClientLib.dll)

**CMakeLists.txt** (`src/client_lib/CMakeLists.txt`):
```cmake
cmake_minimum_required(VERSION 3.20)
project(APClientLib VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    src/ap_client_lib.cpp
    src/ipc_client.cpp
    src/callbacks.cpp
)

set(HEADERS
    include/ap_client_lib.h
    include/ipc_client.h
)

# Build DLL
add_library(APClientLib SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(APClientLib
    PUBLIC include
    PRIVATE ${CMAKE_SOURCE_DIR}/third_party/nlohmann
)

# Link libraries
target_link_libraries(APClientLib
    PRIVATE ws2_32  # Windows sockets for Named Pipes
)

# Output directory
set_target_properties(APClientLib PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
```

### Lua Client Library

**No build required** - Pure Lua file distributed as-is.

## Testing Strategy

### Unit Tests (C++ Library)

Test scenarios:
1. **IPC Client Tests**
   - Connect/disconnect from named pipe
   - Send messages to framework
   - Receive messages from framework
   - Handle pipe errors

2. **Callback Tests**
   - Register callbacks
   - Trigger callbacks from incoming messages
   - Multiple callbacks

3. **Thread Safety Tests**
   - Concurrent polling
   - Message queue operations

### Integration Tests (With Framework)

Test scenarios:
1. C++ mod registers with framework
2. Lua mod registers with framework
3. Framework routes item to mod
4. Mod checks location, framework receives it
5. Connection request from mod

## Dependencies

### C++ Client Library
- **Windows API**: Named Pipes (`CreateFile`, `ReadFile`, `WriteFile`)
- **nlohmann/json**: JSON serialization (reuse from Phase 1)

### Lua Client Library
- **Lua 5.4**: Base Lua (provided by UE4SS)
- **LuaJIT FFI** (optional): For better Named Pipes access
- **JSON library** (optional): Can use external or implement simple one

## Deliverables

### Phase 2 Outputs

1. ✅ **APClientLib.dll** (Windows x64)
   - C API for C++ mods
   - Named Pipes IPC client
   - Event callback system

2. ✅ **ap_client.lua**
   - Pure Lua IPC wrapper
   - JSON serialization
   - Event callback system

3. ✅ **Documentation**
   - API reference for C++ library
   - API reference for Lua library
   - Usage examples for both

4. ✅ **Example usage**
   - Simple C++ mod stub
   - Simple Lua mod stub

## Next Steps After Phase 2

With client libraries complete, Phase 3 will implement the main APFramework Lua mod that:
- Loads APFrameworkCore.dll via FFI
- Starts the IPC server
- Performs auto-discovery
- Manages the framework lifecycle

Then Phase 4 will create full example mods demonstrating real usage.

## Notes

- C++ library is optional - only needed if creating C++ mods
- Most mods will likely use Lua client (ap_client.lua)
- Both clients communicate with same framework via IPC
- Client libraries have no dependency on each other

---

## Phase 2 Status

### ✅ COMPLETE - January 1, 2026

**Build Results:**
- APClientLib.dll: 10 KB (Release build)
- ap_client.lua: Pure Lua (no build required)

**Files Created:**
- `src/client_lib/include/ap_client_lib.h` - Public C API
- `src/client_lib/include/ipc_client.h` - Internal IPC client header
- `src/client_lib/src/ap_client_lib.cpp` - C API implementation
- `src/client_lib/src/ipc_client.cpp` - Named Pipes IPC client implementation
- `src/client_lib/CMakeLists.txt` - Build configuration
- `src/lua_client/ap_client.lua` - Complete Lua client library
- `examples/cpp_mod_example.cpp` - Full C++ mod example
- `examples/lua_mod_example.lua` - Full Lua mod example

**Documentation:**
- Complete C API reference (this document)
- Complete Lua API reference (this document)
- Usage examples for both C++ and Lua
- Example mod implementations

**Testing:**
- Both libraries build successfully
- Integration testing deferred to Phase 4

**Ready for Phase 3**: Lua Framework Wrapper implementation
- Minimal dependencies to keep distribution simple