# Phase 03: IPC Communication System

**Status**: 🔴 Not Started

---

## Overview

Implement Windows Named Pipes-based IPC system for communication between APFrameworkCore and AP-enabled mods (APClientLib).

**Goals**:
- Bidirectional IPC communication using Named Pipes
- JSON-based message protocol
- Connection management and error handling
- Server (APIPCServer) and client (APIPCClient) implementations
- Thread-safe message queuing

---

## Prerequisites

- ✅ Phase 02 complete (APConfig, APLogger, error handling)
- ✅ nlohmann/json available
- ✅ Windows platform (Named Pipes API)

---

## Architecture

```
APFrameworkCore                    APClientLib (Mod DLL)
┌────────────────────┐            ┌────────────────────┐
│  APIPCServer       │            │  APIPCClient       │
│                    │            │                    │
│  - Named Pipe      │◄──────────►│  - Connect to pipe │
│  - Accept conns    │   JSON     │  - Send/recv msgs  │
│  - Route messages  │  Messages  │  - Message queue   │
└────────────────────┘            └────────────────────┘
         │                                  │
         │                                  │
    To Message Router              To Mod (Lua/C++)
```

---

## Components

### 1. APIPCServer (Framework Side)
### 2. APIPCClient (Client Library Side)
### 3. IPC Message Protocol
### 4. Connection Management

---

## Implementation Details

### Component 1: APIPCServer

**Purpose**: Server-side IPC handling, accepts connections from mods, routes messages.

**File**: `APFrameworkCore/include/ap_ipc_server.h`

```cpp
#pragma once
#include "ap_types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <map>
#include <thread>
#include <mutex>
#include <functional>
#include <windows.h>

namespace APFramework {

// IPC Message structure
struct IPCMessage {
    std::string type;           // "register", "command", "response", "notification", "ap_message"
    std::string from_mod_id;
    std::string to_mod_id;      // Empty for broadcasts
    std::string msg_id;         // For request/response correlation
    nlohmann::json data;
};

class APIPCServer {
public:
    APIPCServer();
    ~APIPCServer();

    // Initialize server with pipe name from config
    VoidResult init(const std::string& pipe_name);

    // Start accepting connections
    VoidResult start();

    // Stop server and disconnect all clients
    void stop();

    // Send message to specific mod
    VoidResult send_to_mod(const std::string& mod_id, const IPCMessage& message);

    // Broadcast message to all connected mods
    VoidResult broadcast(const IPCMessage& message);

    // Broadcast message to priority clients only
    VoidResult broadcast_to_priority_clients(const IPCMessage& message);

    // Set callback for received messages
    using MessageCallback = std::function<void(const IPCMessage&)>;
    void set_message_callback(MessageCallback callback);

    // Get list of connected mod IDs
    std::vector<std::string> get_connected_mods() const;

    // Check if specific mod is connected
    bool is_mod_connected(const std::string& mod_id) const;

private:
    struct ClientConnection {
        HANDLE pipe_handle;
        std::string mod_id;
        bool is_priority;
        std::thread read_thread;
        std::atomic<bool> should_stop;
    };

    void accept_connections();
    void handle_client(ClientConnection* client);
    bool read_message(HANDLE pipe, IPCMessage& out_message);
    bool write_message(HANDLE pipe, const IPCMessage& message);

    std::string pipe_name_;
    HANDLE listen_pipe_ = INVALID_HANDLE_VALUE;
    std::thread accept_thread_;
    std::atomic<bool> should_stop_{false};

    std::map<std::string, std::unique_ptr<ClientConnection>> clients_;
    mutable std::mutex clients_mutex_;

    MessageCallback message_callback_;
};

} // namespace APFramework
```

**Key Implementation Notes**:
- Uses overlapped I/O for async operations
- Each client connection gets its own read thread
- Messages are JSON-encoded strings with newline delimiter
- Max message size enforced (MAX_IPC_MESSAGE_SIZE_KB from config)

**Acceptance Criteria**:
- ✅ Creates Named Pipe with proper security attributes
- ✅ Accepts multiple client connections
- ✅ Routes messages to specific mods by mod_id
- ✅ Supports broadcast to all clients
- ✅ Supports broadcast to priority clients only
- ✅ Thread-safe client management
- ✅ Proper cleanup on disconnect

---

### Component 2: APIPCClient

**Purpose**: Client-side IPC handling, connects to APIPCServer, sends/receives messages.

**File**: `APClientLib/include/ap_ipc_client.h`

```cpp
#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <functional>
#include <atomic>
#include <windows.h>

namespace APClientLib {

struct IPCMessage {
    std::string type;
    std::string from_mod_id;
    std::string to_mod_id;
    std::string msg_id;
    nlohmann::json data;
};

struct VoidResult {
    bool success;
    std::string error_message;
    static VoidResult success_result() { return {true, ""}; }
    static VoidResult failure(const std::string& msg) { return {false, msg}; }
};

class APIPCClient {
public:
    APIPCClient();
    ~APIPCClient();

    // Connect to framework's Named Pipe
    VoidResult connect(const std::string& pipe_name, std::chrono::milliseconds timeout);

    // Disconnect from server
    void disconnect();

    // Send message to framework/other mods
    VoidResult send(const IPCMessage& message);

    // Get pending messages (non-blocking)
    std::vector<IPCMessage> get_messages();

    // Set callback for received messages (optional, alternative to polling)
    using MessageCallback = std::function<void(const IPCMessage&)>;
    void set_message_callback(MessageCallback callback);

    // Check if connected
    bool is_connected() const;

private:
    void read_loop();
    bool read_message(IPCMessage& out_message);
    bool write_message(const IPCMessage& message);

    HANDLE pipe_handle_ = INVALID_HANDLE_VALUE;
    std::thread read_thread_;
    std::atomic<bool> should_stop_{false};

    std::vector<IPCMessage> pending_messages_;
    mutable std::mutex messages_mutex_;

    MessageCallback message_callback_;
};

} // namespace APClientLib
```

**Key Implementation Notes**:
- Connects to existing Named Pipe
- Runs read thread to continuously receive messages
- Queues received messages for polling or triggers callback
- Thread-safe message queue

**Acceptance Criteria**:
- ✅ Connects to Named Pipe with timeout
- ✅ Sends messages to server
- ✅ Receives messages asynchronously
- ✅ Supports both polling and callback patterns
- ✅ Graceful disconnect and cleanup
- ✅ Reconnection support (optional)

---

### Component 3: IPC Message Protocol

**Message Format**: JSON with newline delimiter

```json
{
  "type": "register|command|response|notification|ap_message",
  "from_mod_id": "archipelago.palworld.framework",
  "to_mod_id": "author.palworld.mod",
  "msg_id": "uuid-or-empty",
  "data": {
    // Type-specific payload
  }
}
```

**Message Types**:

1. **REGISTER** - Client → Server
   ```json
   {
     "type": "register",
     "from_mod_id": "author.palworld.mod",
     "data": {
       "mod_name": "My Mod",
       "capabilities": { /* mod capabilities */ }
     }
   }
   ```

2. **COMMAND** - Priority Client → Server
   ```json
   {
     "type": "command",
     "from_mod_id": "archipelago.palworld.framework_ui",
     "cmd": "CMD_CONNECT|CMD_GENERATE|CMD_DISCONNECT",
     "data": {}
   }
   ```

3. **RESPONSE** - Server → Client (in response to command/request)
   ```json
   {
     "type": "response",
     "msg_id": "original-request-msg-id",
     "to_mod_id": "author.palworld.mod",
     "data": {
       "success": true,
       "message": "Operation successful"
     }
   }
   ```

4. **NOTIFICATION** - Server → Client(s)
   ```json
   {
     "type": "notification",
     "to_mod_id": "",
     "data": {
       "event": "LIFECYCLE_CHANGE",
       "phase": "RUNNING"
     }
   }
   ```

5. **AP_MESSAGE** - Server ↔ Client (Archipelago server messages)
   ```json
   {
     "type": "ap_message",
     "to_mod_id": "subscribed.mod.id",
     "data": {
       "ap_type": "LocationScout|ItemSend|etc",
       "payload": { /* AP-specific data */ }
     }
   }
   ```

**Size Limits**:
- Max message size: Configurable via `MAX_IPC_MESSAGE_SIZE_KB` (default 1MB)
- Messages exceeding limit are rejected with error

**Acceptance Criteria**:
- ✅ All message types documented and implemented
- ✅ JSON serialization/deserialization working
- ✅ Size limit enforced
- ✅ msg_id correlation for request/response

---

### Component 4: Connection Management

**Server-Side Connection Tracking**:
```cpp
struct ClientInfo {
    std::string mod_id;
    bool is_priority;
    std::chrono::steady_clock::time_point connected_at;
    std::chrono::steady_clock::time_point last_message_at;
};
```

**Connection Lifecycle**:
1. Client connects to Named Pipe
2. Client sends REGISTER message
3. Server validates registration (mod_id format, capabilities)
4. Server responds with REGISTRATION_SUCCESS or REGISTRATION_FAILED
5. Client is now "connected" and tracked
6. On disconnect (pipe closed), server removes client from tracking

**Timeout Handling**:
- IPC operations have configurable timeout (from APConfig)
- Blocked operations should not deadlock framework
- Disconnected clients removed immediately

**Acceptance Criteria**:
- ✅ Server tracks all connected clients
- ✅ Clients properly register on connection
- ✅ Disconnected clients cleaned up
- ✅ Timeout enforcement prevents deadlocks

---

## Testing

### Unit Tests

**File**: `tests/unit/test_ipc.cpp`

```cpp
#include <gtest/gtest.h>
#include "ap_ipc_server.h"
#include "../APClientLib/include/ap_ipc_client.h"

TEST(IPCTest, ServerStartStop) {
    APFramework::APIPCServer server;
    auto result = server.init("\\\\.\\pipe\\APFrameworkTest");
    ASSERT_TRUE(result.is_success());

    result = server.start();
    ASSERT_TRUE(result.is_success());

    server.stop();
}

TEST(IPCTest, ClientConnect) {
    // Start server
    APFramework::APIPCServer server;
    server.init("\\\\.\\pipe\\APFrameworkTest");
    server.start();

    // Connect client
    APClientLib::APIPCClient client;
    auto result = client.connect("\\\\.\\pipe\\APFrameworkTest",
                                  std::chrono::milliseconds(5000));
    ASSERT_TRUE(result.success);

    EXPECT_TRUE(client.is_connected());

    client.disconnect();
    server.stop();
}

TEST(IPCTest, SendReceiveMessage) {
    // Setup server with callback
    APFramework::APIPCServer server;
    server.init("\\\\.\\pipe\\APFrameworkTest");
    server.start();

    bool message_received = false;
    server.set_message_callback([&](const APFramework::IPCMessage& msg) {
        EXPECT_EQ(msg.type, "test");
        EXPECT_EQ(msg.data["content"], "Hello");
        message_received = true;
    });

    // Connect client and send message
    APClientLib::APIPCClient client;
    client.connect("\\\\.\\pipe\\APFrameworkTest", std::chrono::milliseconds(5000));

    APClientLib::IPCMessage msg;
    msg.type = "test";
    msg.from_mod_id = "test.mod";
    msg.data = {{"content", "Hello"}};

    client.send(msg);

    // Wait briefly for async delivery
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(message_received);

    client.disconnect();
    server.stop();
}
```

---

## Integration Points

### With APManager
- APManager initializes APIPCServer during startup
- APManager sets message callback to route to APMessageRouter
- APManager uses APIPCServer to broadcast lifecycle notifications

### With APMessageRouter (Phase 06)
- IPC messages routed through APMessageRouter
- Router determines message destinations based on capabilities

### With APClientLib (Phase 09)
- Client mods use APClientLib to connect and communicate
- APClientLib provides Lua bindings for mod scripts

---

## Acceptance Criteria

**Phase 03 is complete when**:
- ✅ APIPCServer creates and manages Named Pipe
- ✅ APIPCServer accepts multiple client connections
- ✅ APIPCServer routes messages to specific clients
- ✅ APIPCServer supports broadcast operations
- ✅ APIPCClient connects to server
- ✅ APIPCClient sends and receives messages
- ✅ IPC message protocol fully implemented
- ✅ JSON serialization/deserialization working
- ✅ Message size limits enforced
- ✅ Connection management and cleanup working
- ✅ Unit tests pass for server and client
- ✅ Integration test demonstrates bidirectional communication

---

## Known Issues / Blockers

**Platform Limitation**: Named Pipes are Windows-only. Future cross-platform support would require Unix domain sockets implementation.

---

## Deliverables

1. ✅ `ap_ipc_server.h` and `ap_ipc_server.cpp`
2. ✅ `ap_ipc_client.h` and `ap_ipc_client.cpp`
3. ✅ IPC protocol documentation
4. ✅ Unit tests for IPC system
5. ✅ Integration test for bidirectional communication

---

## Next Phase

[Phase 04: AP Client Integration](Phase04_APClientIntegration.md)

**Prerequisites from Phase 03**:
- IPC system working for framework-mod communication
- Message routing infrastructure in place

---

**Last Updated**: 2026-01-09
**Status**: 🔴 Not Started