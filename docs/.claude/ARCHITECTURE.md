# Palworld Archipelago Framework - Architecture

## Table of Contents
1. [Overview](#overview)
2. [System Architecture](#system-architecture)
3. [Component Details](#component-details)
4. [Data Flow](#data-flow)
5. [Communication Protocol](#communication-protocol)
6. [Capabilities System](#capabilities-system)
7. [AP World Integration (Python)](#ap-world-integration-python)
8. [Lifecycle & State Management](#lifecycle--state-management)
9. [Error Handling & Validation](#error-handling--validation)
10. [Implementation Considerations](#implementation-considerations)

---

## Overview

The Palworld Archipelago Framework is a modular system enabling UE4SS mods to participate in Archipelago multiworld sessions. It provides a bridge between game mods (via UE4SS) and the Archipelago server, with a focus on extensibility and conflict-free mod coexistence.

### Design Principles
1. **Modularity**: Clear separation between framework core, client library, and mods
2. **Zero-conflict Policy**: Automatic detection and prevention of capability conflicts
3. **Flexibility**: Dynamic capability schema allowing community-driven features
4. **Extensibility**: Support for both built-in and third-party mods

### Key Terminology
- **APFrameworkCore**: C++ middleware coordinating all system components
- **APClientLib**: Lightweight C++ IPC client library for mods
- **APFrameworkMod**: Lua entry point, dual-purpose framework + client mod
- **AP Client Mod**: Any UE4SS mod that is "AP-enabled" via an `AP_Config.json` file. There are two types:
  - **Priority Client**: AP Client Mod with `mod_id` matching `archipelago.<game_name>.*` pattern (e.g., `archipelago.palworld.framework`, `archipelago.palworld.framework_ui`). These provide framework extensions/utilities and **DO NOT** have a `capabilities` field in their config. They have special messaging privileges and access to framework commands.
  - **Regular Client**: AP Client Mod with any other `mod_id` pattern (e.g., `myauthor.palworld.techshuffle`, `community.palworld.chestshuffle`). These **MUST** have a `capabilities` field and contribute to AP World generation.
- **UE4SS Mod Types**:
  - **Lua Mod**: Contains `Scripts/main.lua`
  - **C++ Mod**: Contains `dlls/main.dll` (UE4SS toolchain-built)
  - **Blueprint Logic Mod**: `.pak` file in `Content/Paks/LogicMods/` (loaded by BPModLoaderMod)

---

## System Architecture

### Component Hierarchy

```
┌─────────────────────────────────────────────────────────────────┐
│                         UE4SS Runtime                           │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │              APFrameworkMod (Lua)                          │ │
│  │  ┌──────────────────────────────────────────────────────┐  │ │
│  │  │         APFrameworkCore (C++ DLL)                    │  │ │
│  │  │  ┌────────────┬─────────────┬──────────────────┐    │  │ │
│  │  │  │ APManager  │  APClient   │  APIPCServer     │    │  │ │
│  │  │  │ (Singleton)│ (AP Server) │  (Mod Comms)     │    │  │ │
│  │  │  └────────────┴─────────────┴──────────────────┘    │  │ │
│  │  │  ┌────────────┬─────────────┬──────────────────┐    │  │ │
│  │  │  │APCapabil.  │APModRegistry│ APMessageRouter  │    │  │ │
│  │  │  └────────────┴─────────────┴──────────────────┘    │  │ │
│  │  │  ┌────────────┬─────────────────────────────────┐   │  │ │
│  │  │  │ APConfig   │  APPollingThread (60fps)        │   │  │ │
│  │  │  └────────────┴─────────────────────────────────┘   │  │ │
│  │  └──────────────────────────────────────────────────────┘  │ │
│  │                                                             │ │
│  │  Lua Bindings: APFramework.lua + sol2                      │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                                                                   │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │    AP Client Mods (Lua/C++/BP)                              │ │
│  │  ┌──────────────────────────────────────────────────────┐  │ │
│  │  │  APClientLib (C++ DLL)                               │  │ │
│  │  │    ┌──────────────────────────────────────────┐      │  │ │
│  │  │    │  APIPCClient (connects to APIPCServer)   │      │  │ │
│  │  │    └──────────────────────────────────────────┘      │  │ │
│  │  └──────────────────────────────────────────────────────┘  │ │
│  │  Lua Bindings: APClient.lua + sol2                         │ │
│  └─────────────────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────────┘
                              │
                              │ WebSocket (via apclientpp)
                              ▼
                  ┌────────────────────────┐
                  │  Archipelago Server    │
                  │  (Python MultiWorld)   │
                  └────────────────────────┘
```

### Technology Stack

**APFrameworkCore (C++):**
- `apclientpp` - Archipelago server communication (WebSocket)
  - `wswrap` - WebSocket wrapper
  - `asio` (1.12 standalone) - Async I/O
  - `websocketpp` - WebSocket implementation
  - `valijson` - JSON schema validation
- `nlohmann::json` - JSON parsing/serialization
- `sol2` - Lua bindings
- `lua-5.4.7` - Lua runtime (static library)

**APClientLib (C++):**
- IPC mechanism (implementation-defined, see [Communication Protocol](#communication-protocol))
- `nlohmann::json` - JSON serialization
- `sol2` - Lua bindings
- Message size limit: Configurable via `#define MAX_IPC_MESSAGE_SIZE_KB 1024` (1MB default)

**Lua Layer:**
- `lunajson` - Lua JSON codec
- `APFramework.lua` / `APClient.lua` - High-level Lua wrappers

---

## Component Details

### APFrameworkCore Components

#### APManager
**Type:** Singleton
**Purpose:** Global orchestrator managing lifecycle and component coordination

**Responsibilities:**
- Initialize/shutdown all framework components
- Coordinate transitions between lifecycle states
- Manage global state (current phase, connection status)
- Own and coordinate all other manager components
- Handle global error conditions

**Key Methods:**
```cpp
class APManager {
public:
    static APManager& instance();

    // Lifecycle
    void initialize(const std::string& config_path);
    void shutdown();
    void reset();  // For CMD_RESYNC

    // State access
    LifecyclePhase get_current_phase() const;
    bool is_connected_to_ap_server() const;

    // Component access
    APConfig& config();
    APClient& ap_client();
    APIPCServer& ipc_server();
    APCapabilities& capabilities();
    APModRegistry& mod_registry();
    APMessageRouter& message_router();
    APPollingThread& polling_thread();

private:
    APManager() = default;

    std::unique_ptr<APConfig> config_;
    std::unique_ptr<APClient> ap_client_;
    std::unique_ptr<APIPCServer> ipc_server_;
    std::unique_ptr<APCapabilities> capabilities_;
    std::unique_ptr<APModRegistry> mod_registry_;
    std::unique_ptr<APMessageRouter> message_router_;
    std::unique_ptr<APPollingThread> polling_thread_;

    LifecyclePhase current_phase_;
    mutable std::mutex phase_mutex_;
};

enum class LifecyclePhase {
    UNINITIALIZED,
    DISCOVERING_MODS,
    AWAITING_PRIORITY_REGISTRATION,
    AWAITING_REGULAR_REGISTRATION,
    VALIDATING_CAPABILITIES,
    GENERATING_CAPABILITIES,
    READY_FOR_CONNECTION,
    CONNECTING_TO_AP,
    CONNECTED_AND_SYNCING,
    RUNNING,
    ERROR_STATE
};
```

**Thread Safety:** All public methods must be thread-safe as they may be called from the polling thread, IPC handlers, or Lua bindings.

---

#### APClient
**Type:** Component
**Purpose:** Wrapper around `apclientpp` for AP server communication

**Responsibilities:**
- Establish/maintain WebSocket connection to AP server
- Handle connection handshake (RoomInfo → GetDataPackage → Connect → Connected)
- Expose clean interface for sending AP protocol packets
- Receive and buffer incoming packets for polling thread
- Manage connection state and auto-reconnect (if configured)

**Key Methods:**
```cpp
class APClient {
public:
    // Connection management
    bool connect(const std::string& server, int port,
                 const std::string& slot_name,
                 const std::string& password = "");
    void disconnect();
    bool is_connected() const;

    // Protocol operations
    void poll();  // Called by APPollingThread, processes pending packets
    std::vector<APMessage> get_messages();  // Retrieve buffered messages

    void send_location_checks(const std::vector<int64_t>& location_ids);
    void send_location_scouts(const std::vector<int64_t>& location_ids,
                              int create_as_hint = 0);
    void send_status_update(ClientStatus status);
    void send_sync();

    // Data access
    const RoomInfo& get_room_info() const;
    const DataPackage& get_data_package() const;
    int get_player_slot() const;
    int get_player_team() const;

private:
    std::unique_ptr<APClientPP::APClient> apclientpp_;
    std::vector<APMessage> message_buffer_;
    mutable std::mutex message_mutex_;

    RoomInfo room_info_;
    DataPackage data_package_;
    ConnectionState conn_state_;
};

struct APMessage {
    MessageType type;  // ReceivedItems, LocationInfo, PrintJSON, etc.
    nlohmann::json data;
};

enum class ClientStatus {
    CLIENT_UNKNOWN = 0,
    CLIENT_CONNECTED = 5,
    CLIENT_READY = 10,
    CLIENT_PLAYING = 20,
    CLIENT_GOAL = 30
};
```

**Threading Model:**
- Connection operations are asynchronous with callback for completion (timeout configurable via APConfig)
- `poll()` is called exclusively by APPollingThread
- `get_messages()` is thread-safe and can be called from APPollingThread

**Connection Timeout:**
- `APClient::connect()` must support asynchronous connection with timeout
- Timeout value configurable via `framework_config.json` (field: `ap_server.connection_timeout_ms`)
- Default timeout: 30000ms (30 seconds)
- On timeout, connection attempt fails and error is reported

---

#### APIPCServer
**Type:** Component
**Purpose:** IPC "mailbox" system for mod communication

**Responsibilities:**
- Accept IPC connections from APClientLib instances
- Route incoming messages to appropriate handlers
- Send messages to specific mods or broadcast to all
- Maintain client connection state
- Handle client disconnections gracefully

**IPC Message Format:**
```json
{
  "type": "request|response|notification|command",
  "msg_id": "UUID-for-request-response-tracking",
  "from_mod_id": "author.game.mod_name",
  "to_mod_id": "target.mod.id (optional for broadcasts)",
  "cmd": "REGISTER|LOCATION_CHECK|ITEM_RECEIVED|...",
  "data": { /* command-specific payload */ },
  "timestamp": 1234567890
}
```

**Key Methods:**
```cpp
class APIPCServer {
public:
    void start(const std::string& endpoint);  // e.g., "\\\\.\\pipe\\APFramework"
    void stop();

    // Sending
    void send_to_mod(const std::string& mod_id, const IPCMessage& msg);
    void broadcast_to_all(const IPCMessage& msg);
    void broadcast_to_priority_clients(const IPCMessage& msg);
    void broadcast_to_regular_clients(const IPCMessage& msg);

    // Registration
    bool is_mod_connected(const std::string& mod_id) const;
    std::vector<std::string> get_connected_mod_ids() const;

private:
    void handle_client_connection(ClientConnection* client);
    void handle_client_message(ClientConnection* client, const IPCMessage& msg);
    void handle_client_disconnection(ClientConnection* client);

    void route_message(const IPCMessage& msg);

    std::unique_ptr<IPCServerImpl> impl_;  // Platform-specific
    std::map<std::string, ClientConnection*> connected_mods_;
    mutable std::mutex connections_mutex_;
};

struct IPCMessage {
    MessageType type;
    std::string msg_id;
    std::string from_mod_id;
    std::string to_mod_id;  // Empty for broadcast
    std::string cmd;
    nlohmann::json data;
    int64_t timestamp;
};
```

**IPC Mechanism Options:**
The design document doesn't specify the IPC mechanism. Recommended approaches for Windows:

1. **Named Pipes (Recommended)**
   - Native Windows IPC
   - Message framing built-in
   - Easy access control
   - Example endpoint: `\\\\.\\pipe\\APFramework_<game_name>`

2. **TCP/IP Localhost Sockets**
   - Cross-platform
   - Simple implementation
   - Port discovery needed
   - Example: `localhost:37240`

3. **Memory-Mapped Files**
   - Lowest latency
   - Requires synchronization primitives
   - More complex implementation

**Decision:** Named Pipes are recommended for Windows-first implementation with potential cross-platform support via TCP/IP fallback.

---

#### APCapabilities
**Type:** Component
**Purpose:** Capability validation and aggregation

**Responsibilities:**
- Load capabilities from each mod's `AP_Config.json`
- Validate capabilities against schema
- Detect conflicts between mods
- Aggregate capabilities into final output
- Generate `APCapabilities_<slot_name>.json`

**Capability Schema Structure:**
```json
{
  "schema_version": "1.0.0",
  "capabilities": {
    "items": {
      "<item_category>": {
        "type": "progressive|toggle|counter|...",
        "items": [
          {
            "id": "unique_item_id",
            "name": "Display Name",
            "description": "What this item does",
            "classification": "progression|useful|filler|trap",
            "metadata": { /* game-specific */ }
          }
        ]
      }
    },
    "locations": {
      "<location_category>": {
        "type": "static|dynamic|conditional",
        "locations": [
          {
            "id": "unique_location_id",
            "name": "Display Name",
            "description": "Where/how to check this",
            "region": "region_id",
            "metadata": { /* game-specific */ }
          }
        ]
      }
    },
    "regions": [
      {
        "id": "region_id",
        "name": "Region Name",
        "entrances": ["entrance_id_1", "entrance_id_2"]
      }
    ],
    "entrances": [
      {
        "id": "entrance_id",
        "source_region": "region_id",
        "target_region": "region_id",
        "requirements": { /* access rules */ }
      }
    ],
    "options": {
      "<option_name>": {
        "type": "toggle|range|choice",
        "default": "...",
        "description": "..."
      }
    }
  }
}
```

**Key Methods:**
```cpp
class APCapabilities {
public:
    // Loading & Validation
    bool load_mod_capabilities(const std::string& mod_id,
                               const std::filesystem::path& config_path);
    ValidationResult validate_mod_capabilities(const std::string& mod_id);

    // Conflict detection
    ConflictReport detect_conflicts() const;
    bool has_conflicts() const;

    // Generation
    bool generate_capabilities_file(const std::string& slot_name,
                                    const std::filesystem::path& output_path);

    // Query
    const ModCapabilities& get_mod_capabilities(const std::string& mod_id) const;
    std::vector<std::string> get_mods_owning_item(const std::string& item_name) const;
    std::vector<std::string> get_mods_owning_location(const std::string& location_name) const;

private:
    std::map<std::string, ModCapabilities> mod_capabilities_;
    std::map<std::string, std::string> item_to_mod_;  // For routing
    std::map<std::string, std::string> location_to_mod_;

    bool validate_schema(const nlohmann::json& capabilities);
    ConflictReport check_capability_overlaps() const;
};

struct ConflictReport {
    bool has_conflicts;
    std::vector<ConflictDetail> conflicts;
};

struct ConflictDetail {
    ConflictType type;  // ITEM_OVERLAP, LOCATION_OVERLAP, INCOMPATIBLE_MODS
    std::vector<std::string> involved_mod_ids;
    std::string description;
    std::vector<std::string> conflicting_items_or_locations;
};
```

**Validation Rules:**
1. No two regular client mods can claim the same item name/ID
2. No two regular client mods can claim the same location name/ID
3. Mods listed in `incompatible` fields cannot both be loaded
4. Version constraints in `incompatible` must be satisfied
5. Capabilities must conform to schema
6. Priority clients cannot have `capabilities` field

---

#### APModRegistry
**Type:** Component
**Purpose:** Mod discovery, registration tracking, and promise-based coordination

**Responsibilities:**
- Discover all `AP_Config.json` files in UE4SS mods directory
- Track expected mods vs. registered mods
- Differentiate priority vs. regular clients
- Provide promise/future-based registration waiting
- Timeout handling for missing mods

**Mod Discovery Process:**
```
1. Scan <game_root>/<game_name>/Binaries/Win64/ue4ss/Mods/**/AP_Config.json
2. Parse each config to extract:
   - mod_id
   - name
   - version
   - description
   - incompatible list
   - capabilities (if not priority client)
3. Categorize as priority vs. regular client based on mod_id pattern
4. Store in expected_mods_ registry
5. Create registration promises for each mod
```

**Key Methods:**
```cpp
class APModRegistry {
public:
    // Discovery
    void discover_mods(const std::filesystem::path& mods_directory);

    // Registration
    bool register_mod(const std::string& mod_id, const ModInfo& info);
    bool unregister_mod(const std::string& mod_id);

    // Waiting for registration (promise-based)
    // Timeout values should be retrieved from APConfig, not hardcoded
    std::future<bool> wait_for_priority_registrations();
    std::future<bool> wait_for_all_registrations();

    // Query
    bool is_mod_expected(const std::string& mod_id) const;
    bool is_mod_registered(const std::string& mod_id) const;
    bool is_priority_client(const std::string& mod_id) const;

    std::vector<std::string> get_expected_mods() const;
    std::vector<std::string> get_registered_mods() const;
    std::vector<std::string> get_unregistered_mods() const;
    std::vector<std::string> get_priority_clients() const;
    std::vector<std::string> get_regular_clients() const;

    const ModInfo& get_mod_info(const std::string& mod_id) const;

private:
    struct ModEntry {
        ModInfo info;
        bool is_priority;
        bool is_registered;
        std::promise<bool> registration_promise;
        std::chrono::steady_clock::time_point discovery_time;
    };

    std::map<std::string, ModEntry> mods_;
    mutable std::mutex mods_mutex_;

    bool is_priority_pattern(const std::string& mod_id) const;
};

struct ModInfo {
    std::string mod_id;           // "author.game.mod_name"
    std::string name;             // "Friendly Mod Name"
    std::string version;          // "1.0.0"
    std::string description;
    std::vector<IncompatibleEntry> incompatible;
    std::filesystem::path config_path;
};

struct IncompatibleEntry {
    std::string id;                          // Other mod ID
    std::optional<std::string> versions;     // Version constraint or list
};
```

**Priority Client Pattern:** `archipelago.<game_name>.*`
- Example: `archipelago.palworld.framework` (the Lua framework itself)
- Example: `archipelago.palworld.framework_ui` (hypothetical UI mod)

---

#### APMessageRouter
**Type:** Component
**Purpose:** Route AP server messages to appropriate mods

**Responsibilities:**
- Receive messages from APClient (via APPollingThread)
- Determine which mod should receive each message based on item/location ownership
- Route ReceivedItems, LocationInfo, PrintJSON, etc. to appropriate mods
- Handle broadcasts (messages relevant to all mods)

**Routing Logic:**
```
ReceivedItems message:
  For each item in message:
    - Look up item ownership from APCapabilities
    - Route to owning mod via APIPCServer

LocationInfo message:
  For each location in message:
    - Look up location ownership from APCapabilities
    - Route to owning mod via APIPCServer

PrintJSON, RoomUpdate, etc.:
  - Broadcast to all connected mods (they can filter client-side)
```

**Key Methods:**
```cpp
class APMessageRouter {
public:
    void route_ap_message(const APMessage& message);

    void set_capabilities_ref(APCapabilities* capabilities);
    void set_ipc_server_ref(APIPCServer* ipc_server);

private:
    void route_received_items(const APMessage& message);
    void route_location_info(const APMessage& message);
    void route_print_json(const APMessage& message);
    void route_room_update(const APMessage& message);

    APCapabilities* capabilities_;  // Non-owning
    APIPCServer* ipc_server_;       // Non-owning
};
```

---

#### APPollingThread
**Type:** Component
**Purpose:** Background thread for continuous AP server polling

**Responsibilities:**
- Poll AP server at configurable interval (default: 16ms ≈ 60fps)
- Call `APClient::poll()` to process WebSocket events
- Retrieve messages via `APClient::get_messages()`
- Route messages via `APMessageRouter::route_ap_message()`
- Remain non-blocking for main thread

**Key Methods:**
```cpp
class APPollingThread {
public:
    void start(std::chrono::milliseconds poll_interval = std::chrono::milliseconds(16));
    void stop();
    bool is_running() const;

    void set_ap_client_ref(APClient* ap_client);
    void set_message_router_ref(APMessageRouter* message_router);
    void set_ap_manager_ref(APManager* ap_manager);

private:
    void polling_loop();

    std::thread thread_;
    std::atomic<bool> should_stop_;
    std::chrono::milliseconds poll_interval_;

    APClient* ap_client_;                // Non-owning
    APMessageRouter* message_router_;    // Non-owning
    APManager* ap_manager_;              // Non-owning (for lifecycle state checking)
};
```

**Polling Loop Pseudocode:**
```cpp
void APPollingThread::polling_loop() {
    auto last_poll = std::chrono::steady_clock::now();

    while (!should_stop_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_poll);

        // Only poll when the configured interval has elapsed
        if (elapsed >= poll_interval_) {
            last_poll = now;

            try {
                // Check lifecycle state - only poll when in appropriate states
                auto current_phase = ap_manager_->get_current_phase();

                // Only poll and route messages when connected to AP server
                if (current_phase == LifecyclePhase::CONNECTED_AND_SYNCING ||
                    current_phase == LifecyclePhase::RUNNING) {

                    // Poll the AP client (processes WebSocket events)
                    ap_client_->poll();

                    // Get any new messages
                    auto messages = ap_client_->get_messages();

                    // Route each message
                    for (const auto& msg : messages) {
                        message_router_->route_ap_message(msg);
                    }
                }
                // In other states (VALIDATING_CAPABILITIES, GENERATING_CAPABILITIES, etc.),
                // skip polling to avoid processing messages during inconsistent framework state

            } catch (const std::exception& e) {
                // Log error but continue polling
                // APManager may need to be notified of critical errors
            }
        }

        // Small sleep to prevent busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
```

---

#### APConfig
**Type:** Component
**Purpose:** Configuration management

**Responsibilities:**
- Load and parse `framework_config.json`
- Provide access to all configuration values
- Support hot-reloading (optional)
- Validate configuration on load

**Configuration Schema:**
```json
{
  "ap_server": {
    "server": "archipelago.gg",
    "port": 38281,
    "slot_name": "Player1",
    "password": "",
    "auto_reconnect": true,
    "reconnect_delay_ms": 5000,
    "connection_timeout_ms": 30000
  },
  "framework": {
    "polling_interval_ms": 16,
    "registration_timeout_ms": 180000,
    "priority_registration_timeout_ms": 60000,
    "mods_directory": "Mods",
    "game_name": "Palworld"
  },
  "logging": {
    "enabled": true,
    "level": "info",
    "file": "APFramework.log",
    "console": true
  },
  "ipc": {
    "endpoint": "\\\\.\\pipe\\APFramework",
    "timeout_ms": 5000
  }
}
```

**Configuration Field Details:**

- `ap_server.connection_timeout_ms`: Timeout for initial connection to AP server (default: 30000ms = 30 seconds). Used by `APClient::connect()` to determine when to abort connection attempts.
- `framework.priority_registration_timeout_ms`: Timeout for priority client registration (default: 60000ms = 1 minute). This is the time allowed for all priority clients (mods with `archipelago.<game_name>.*` pattern) to connect and register via IPC.
- `framework.registration_timeout_ms`: Timeout for all regular client registration (default: 180000ms = 3 minutes). This is the time allowed for all regular clients (non-priority mods) to connect and register after priority clients have finished.
- `framework.game_name`: The name of the game this framework is running for (e.g., "Palworld"). This field is used during capability generation to include game information in the aggregated capabilities file, helping the AP World identify which game each player's capabilities belong to.
- `logging.console`:
  - When `false`: All logs are written to the file specified in `logging.file`
  - When `true`: **CRITICAL** - Logs are sent as IPC messages to **PRIORITY CLIENTS ONLY**, not all clients. Standard framework messages (lifecycle notifications, command responses) still go to relevant mods as usual. Only internal framework logging (filtered by `logging.level`) is routed to priority clients. This allows priority clients (such as the Lua framework mod itself) to print received log messages to the UE4SS console and subsequently to `ue4ss/UE4SS.log`
  - Use `false` for permanent file logging without cluttering IPC traffic during debugging
  - Use `true` for real-time console output via priority client forwarding
- `logging.file`: Can be either an absolute path or a relative path. If relative, it will be resolved relative to the working directory of `APFrameworkCore.dll`

**Key Methods:**
```cpp
class APConfig {
public:
    bool load(const std::filesystem::path& config_path);
    bool validate() const;

    // AP Server settings
    const std::string& get_server() const;
    int get_port() const;
    const std::string& get_slot_name() const;
    const std::string& get_password() const;
    bool get_auto_reconnect() const;
    std::chrono::milliseconds get_connection_timeout() const;

    // Framework settings
    std::chrono::milliseconds get_polling_interval() const;
    std::chrono::milliseconds get_registration_timeout() const;
    std::chrono::milliseconds get_priority_registration_timeout() const;
    std::filesystem::path get_mods_directory() const;
    const std::string& get_game_name() const;

    // Logging settings
    bool is_logging_enabled() const;
    LogLevel get_log_level() const;
    std::filesystem::path get_log_file_path() const;
    bool should_log_to_console() const;  // Returns logging.console value

    // IPC settings
    const std::string& get_ipc_endpoint() const;

private:
    nlohmann::json config_;
    std::filesystem::path resolve_log_file_path(const std::string& path_str) const;
};
```

---

### APClientLib Components

#### APIPCClient
**Type:** Component (in client library)
**Purpose:** Connect to APFrameworkCore's IPC server

**Responsibilities:**
- Establish IPC connection to APIPCServer
- Send messages to framework
- Receive messages from framework
- Handle connection failures and reconnection
- Provide synchronous and asynchronous message sending

**Key Methods:**
```cpp
class APIPCClient {
public:
    bool connect(const std::string& endpoint,
                 const std::string& mod_id);
    void disconnect();
    bool is_connected() const;

    // Asynchronous send with callback for response
    // Note: UE4SS runs all Lua mods on a shared thread. A blocking send_sync call
    // would block all other mods from executing. Instead, use send_async with a
    // callback that will be invoked when the response is received during poll_messages().
    using ResponseCallback = std::function<void(const IPCResponse&)>;
    void send_async(const IPCMessage& message, ResponseCallback callback = nullptr);

    // Message receiving
    void poll_messages();  // Called regularly by mod
    std::vector<IPCMessage> get_pending_messages();

    // Convenience methods for common operations
    void register_mod(const ModInfo& info, ResponseCallback callback = nullptr);
    void send_location_check(int64_t location_id);
    void send_location_checks(const std::vector<int64_t>& location_ids);

    // Callback registration
    using MessageCallback = std::function<void(const IPCMessage&)>;
    void set_message_callback(MessageCallback callback);

private:
    std::unique_ptr<IPCClientImpl> impl_;  // Platform-specific
    std::string mod_id_;
    MessageCallback message_callback_;

    // Pending response callbacks keyed by msg_id
    std::map<std::string, ResponseCallback> response_callbacks_;
    mutable std::mutex response_callbacks_mutex_;  // Thread safety for callback map

    std::vector<IPCMessage> pending_messages_;
    mutable std::mutex messages_mutex_;
};

struct IPCResponse {
    bool success;
    nlohmann::json data;
    std::string error_message;
};
```

**Usage Pattern (Lua Mod):**
```lua
local APClient = require("APClient")
local client = APClient.new()

-- Connect and register
client:connect("\\\\.\\pipe\\APFramework", "myauthor.palworld.mymod")
client:register_mod({
    mod_id = "myauthor.palworld.mymod",
    name = "My Cool Mod",
    version = "1.0.0",
    description = "Does cool things"
})

-- Set callback for received messages
client:set_message_callback(function(message)
    if message.cmd == "ITEM_RECEIVED" then
        -- Handle item receipt
        give_player_item(message.data.item_name)
    end
end)

-- In game loop (UE4SS Tick)
RegisterCustomEvent("Tick", function(deltaTime)
    client:poll_messages()
end)

-- Send location check when player opens chest
function OnChestOpened(chest_id)
    local location_id = get_location_id_for_chest(chest_id)
    client:send_location_check(location_id)
end
```

---

## Data Flow

### Initialization Flow

```
1. UE4SS loads APFrameworkMod (Lua)
   ↓
2. APFrameworkMod calls APFramework.initialize()
   ↓
3. APFrameworkCore initializes (C++)
   - APManager::initialize()
     - Load APConfig from framework_config.json
     - Initialize logging
     - Create all components (APClient, APIPCServer, etc.)
     - Start APIPCServer
     - Discover mods via APModRegistry
   ↓
4. APFrameworkMod registers itself as a client
   - Calls APClient.register_mod() via Lua bindings
   - Sends REGISTER message via APIPCServer loopback
   ↓
5. Framework waits for priority clients
   - APModRegistry::wait_for_priority_registrations()
   - Timeout: 5s (configurable)
   ↓
6. Priority clients connect and register
   - Each loads APClientLib.dll
   - Connects to APIPCServer
   - Sends REGISTER message
   ↓
7. Framework validates priority clients
   - APCapabilities validates (priority clients have no capabilities)
   - Check incompatible mods
   ↓
8. Framework waits for regular clients
   - APModRegistry::wait_for_all_registrations()
   - Timeout: 10s (configurable)
   ↓
9. Regular clients connect and register
   - Same as step 6
   ↓
10. Framework validates all capabilities
    - APCapabilities::detect_conflicts()
    - If conflicts: send REGISTRATION_FAILED to conflicting mods, enter ERROR_STATE
    - If valid: proceed
    ↓
11. Framework generates capabilities file
    - APCapabilities::generate_capabilities_file()
    - Output: APCapabilities_<slot_name>.json
    - Send REGISTRATION_SUCCESS to all mods
    ↓
12. Framework enters READY_FOR_CONNECTION state
    - Mods can send CMD_CONNECT to initiate AP server connection
    - Or wait for user action (if UI mod exists)
```

### Connection & Sync Flow

```
1. User triggers connection (via CMD_CONNECT from priority client or auto-start)
   ↓
2. APClient::connect()
   - WebSocket handshake with AP server
   - Send Connect packet with slot_name, password, game, version
   ↓
3. Receive Connected packet
   - Extract slot, team, players, missing_locations, checked_locations, slot_data
   ↓
4. APClient::send_sync()
   - Request ReceivedItems to synchronize inventory
   ↓
5. Receive ReceivedItems
   - APPollingThread polls and gets messages
   - APMessageRouter routes items to owning mods
   - Each mod compares index with saved state, applies new items
   ↓
6. Framework enters RUNNING state
   - Send LIFECYCLE_RUNNING notification to all mods
   - Start APPollingThread (if not already running)
   ↓
7. Gameplay begins
```

### Runtime Message Flow (Item Received)

```
1. Another player checks a location in their game
   ↓
2. Their client sends LocationCheck to AP server
   ↓
3. AP server determines item belongs to our slot
   ↓
4. AP server sends ReceivedItems packet via WebSocket
   ↓
5. APPollingThread (60fps loop)
   - Calls APClient::poll() → processes WebSocket events
   - Calls APClient::get_messages() → [{type: ReceivedItems, data: {...}}]
   ↓
6. APMessageRouter::route_received_items()
   - For each item in packet:
     - Look up item ownership: APCapabilities::get_mods_owning_item(item_name)
     - Build IPC message: {cmd: "ITEM_RECEIVED", data: {item, location, player, flags}}
     - Call APIPCServer::send_to_mod(mod_id, message)
   ↓
7. APIPCServer sends message over IPC to mod
   ↓
8. Mod's APIPCClient receives message
   - APIPCClient::poll_messages() → retrieves from IPC queue
   - Calls registered callback: message_callback_(message)
   ↓
9. Mod's callback processes item
   - Lua: give_player_item(item_name)
   - C++: apply_item_effect(item_id)
   - Save item index to prevent re-processing on reload
```

### Runtime Message Flow (Location Checked)

```
1. Player performs action in game (opens chest, defeats boss, etc.)
   ↓
2. Mod detects the action
   - Lua: OnChestOpened(chest_id)
   ↓
3. Mod sends location check to framework
   - APIPCClient::send_location_check(location_id)
   - IPC message: {cmd: "LOCATION_CHECK", data: {location_id: 12345}}
   ↓
4. APIPCServer receives message, routes to APManager
   ↓
5. APManager forwards to APClient
   - APClient::send_location_checks([location_id])
   ↓
6. APClient sends LocationChecks packet to AP server via WebSocket
   ↓
7. AP server processes location check
   - Determines which player should receive the item at that location
   - Sends ReceivedItems to that player's client
   - Sends PrintJSON to all players ("Player1 found Sword for Player2")
   ↓
8. APPollingThread receives PrintJSON
   - APMessageRouter broadcasts to all mods
   - Mods can display the message or ignore it
```

---

## Communication Protocol

### IPC Message Types

#### Client → Framework

**REGISTER** - Mod registration
```json
{
  "type": "request",
  "msg_id": "uuid-1234",
  "from_mod_id": "author.game.mod",
  "cmd": "REGISTER",
  "data": {
    "mod_id": "author.game.mod",
    "name": "My Mod",
    "version": "1.0.0",
    "description": "Does cool things"
  }
}
```

**LOCATION_CHECK** - Player checked a location
```json
{
  "type": "notification",
  "from_mod_id": "author.game.mod",
  "cmd": "LOCATION_CHECK",
  "data": {
    "location_id": 12345
  }
}
```

**Important**: Mods that declare locations in their capabilities are **solely responsible** for:
1. Detecting when that location is checked in-game (e.g., chest opened, boss defeated)
2. Sending this LOCATION_CHECK notification to the framework
3. The framework routes this to APClient, which forwards to the AP server

**LOCATION_SCOUT** - Query items at locations
```json
{
  "type": "request",
  "msg_id": "uuid-5678",
  "from_mod_id": "author.game.mod",
  "cmd": "LOCATION_SCOUT",
  "data": {
    "location_ids": [12345, 12346, 12347],
    "create_as_hint": 0
  }
}
```

**STATUS_UPDATE** - Client status change
```json
{
  "type": "notification",
  "from_mod_id": "author.game.mod",
  "cmd": "STATUS_UPDATE",
  "data": {
    "status": "CLIENT_PLAYING"  // or CLIENT_READY, CLIENT_GOAL, etc.
  }
}
```

**CMD_CONNECT** - (Priority clients only) Request AP server connection
```json
{
  "type": "command",
  "from_mod_id": "archipelago.palworld.framework_ui",
  "cmd": "CMD_CONNECT",
  "data": {
    "server": "archipelago.gg",  // Optional overrides
    "port": 38281,
    "slot_name": "Player1"
  }
}
```

**CMD_RESYNC** - (Priority clients only) Trigger full resynchronization
```json
{
  "type": "command",
  "from_mod_id": "archipelago.palworld.framework_ui",
  "cmd": "CMD_RESYNC",
  "data": {}
}
```

**CMD_GENERATE** - (Priority clients only) Trigger capability generation
```json
{
  "type": "command",
  "from_mod_id": "archipelago.palworld.framework_ui",
  "cmd": "CMD_GENERATE",
  "data": {}
}
```

**CMD_DISCONNECT** - (Priority clients only) Request graceful disconnect from AP server
```json
{
  "type": "command",
  "from_mod_id": "archipelago.palworld.framework_ui",
  "cmd": "CMD_DISCONNECT",
  "data": {}
}
```

**Note:** CMD_DISCONNECT only works during runtime when the connection is active. It cannot handle disconnection during game crashes or unexpected exits. In those cases, the server will detect the connection loss through normal timeout mechanisms.

#### Framework → Client

**REGISTRATION_SUCCESS** - Mod successfully registered
```json
{
  "type": "response",
  "msg_id": "uuid-1234",
  "to_mod_id": "author.game.mod",
  "cmd": "REGISTRATION_SUCCESS",
  "data": {
    "is_priority": false,
    "registered_at": 1234567890
  }
}
```

**REGISTRATION_FAILED** - Mod registration failed
```json
{
  "type": "response",
  "msg_id": "uuid-1234",
  "to_mod_id": "author.game.mod",
  "cmd": "REGISTRATION_FAILED",
  "data": {
    "reason": "CAPABILITY_CONFLICT",
    "details": "Item 'Sword' already claimed by other.mod.id",
    "conflicting_mods": ["other.mod.id"]
  }
}
```

**ITEM_RECEIVED** - Player received an item
```json
{
  "type": "notification",
  "to_mod_id": "author.game.mod",
  "cmd": "ITEM_RECEIVED",
  "data": {
    "index": 42,  // Item index for tracking
    "item": {
      "item": 1001,  // Item ID
      "location": 2001,  // Location ID where it was found
      "player": 1,  // Player who found it (slot number)
      "flags": 1  // ItemClassification flags
    },
    "item_name": "Sword",  // Resolved from data package
    "location_name": "Boss Chest",
    "player_name": "Player1"
  }
}
```

**LOCATION_INFO** - Response to LOCATION_SCOUT
```json
{
  "type": "response",
  "msg_id": "uuid-5678",
  "to_mod_id": "author.game.mod",
  "cmd": "LOCATION_INFO",
  "data": {
    "locations": [
      {
        "item": 1001,
        "location": 12345,
        "player": 2,
        "flags": 1
      }
    ]
  }
}
```

**LIFECYCLE_STATE** - Framework lifecycle state changed
```json
{
  "type": "notification",
  "cmd": "LIFECYCLE_STATE",
  "data": {
    "state": "RUNNING",  // DISCOVERING_MODS, VALIDATING_CAPABILITIES, READY_FOR_CONNECTION, RUNNING, ERROR_STATE, etc.
    "previous_state": "CONNECTING_TO_AP"
  }
}
```

**RESYNC** - Framework is resyncing, mods should reset
```json
{
  "type": "notification",
  "cmd": "RESYNC",
  "data": {
    "reason": "User requested resync"
  }
}
```

**PRINT_MESSAGE** - Message to display to player
```json
{
  "type": "notification",
  "cmd": "PRINT_MESSAGE",
  "data": {
    "type": "ItemSend",
    "receiving": 1,
    "item": {...},
    "message": "Player2 found Sword for you!"
  }
}
```

### Message Routing Architecture

**Two-Way Message Flow**:

**Client → Framework → AP Server** (Game events):
- Mods detect game events (location checks, status updates)
- Mods send NOTIFICATION IPC messages to framework
- Framework routes to APClient
- APClient sends to AP server via WebSocket

**AP Server → Framework → Client** (AP protocol messages):
- AP server sends protocol packets via WebSocket
- APClient receives and buffers packets
- APMessageRouter translates to IPC messages
- Framework sends to appropriate mod(s) via IPC

### AP Protocol Message Mapping

The APMessageRouter translates AP protocol packets to IPC messages:

| Direction | AP Packet / Game Event | IPC Type | IPC Command | Routing |
|-----------|------------------------|----------|-------------|---------|
| **Server → Client** | ReceivedItems | ap_message | ITEM_RECEIVED | To item owner (per item) |
| **Server → Client** | LocationInfo | ap_message | LOCATION_INFO | To requesting mod |
| **Server → Client** | PrintJSON | ap_message | PRINT_MESSAGE | Broadcast to all |
| **Server → Client** | RoomUpdate | ap_message | ROOM_UPDATE | Broadcast to all |
| **Server → Client** | Connected | ap_message | CONNECTED | Broadcast to all |
| **Server → Client** | ConnectionRefused | ap_message | CONNECTION_REFUSED | Broadcast to all |
| **Server → Client** | DataPackage | (Cached internally) | N/A | N/A |
| **Client → Server** | Location Check | notification | LOCATION_CHECK | From mod → APClient → AP server |
| **Client → Server** | Location Scout | request | LOCATION_SCOUT | From mod → APClient → AP server |
| **Client → Server** | Status Update | notification | STATUS_UPDATE | From mod → APClient → AP server |

**Key Distinction**:
- `ap_message` type: Used **only** for messages originating from AP server being routed to mods
- `notification` type: Used for game events from mods being sent to framework/AP server
- `request` type: Used for mods requesting information from framework/AP server (with response expected)

---

## Capabilities System

### Capability Schema Design

The capabilities schema must be:
1. **Flexible** - Support diverse game mechanics (items, locations, regions, rules)
2. **Extensible** - Allow game-specific metadata
3. **Validatable** - Schema validation to catch errors early
4. **Composable** - Multiple mods contribute non-conflicting pieces

### Core Capability Types

#### Items
```json
{
  "capabilities": {
    "items": {
      "technologies": {
        "type": "progressive",
        "description": "Technology tree unlocks",
        "items": [
          {
            "id": "tech_berry_farm",
            "name": "Berry Farm",
            "classification": "progression",
            "progressive_chain": ["tech_berry_farm_1", "tech_berry_farm_2", "tech_berry_farm_3"],
            "metadata": {
              "tech_level": 5,
              "required_tech_points": 1
            }
          }
        ]
      },
      "resources": {
        "type": "counter",
        "items": [
          {
            "id": "wood",
            "name": "Wood",
            "classification": "filler",
            "count_per_receipt": 50
          }
        ]
      }
    }
  }
}
```

**Item Types:**
- `progressive` - Items that unlock in stages (e.g., sword → sword+1 → sword+2)
- `toggle` - Binary items (have it or don't)
- `counter` - Stackable items (resources, currency)
- `trap` - Negative items

#### Locations
```json
{
  "capabilities": {
    "locations": {
      "chests": {
        "type": "static",
        "description": "Fixed overworld chests",
        "locations": [
          {
            "id": "loc_chest_starting_area_1",
            "name": "Starting Area Chest 1",
            "region": "starting_area",
            "classification": "default",
            "metadata": {
              "coordinates": {"x": 100, "y": 200, "z": 50},
              "chest_type": "wooden"
            }
          }
        ]
      },
      "boss_drops": {
        "type": "conditional",
        "description": "Boss defeat rewards",
        "locations": [
          {
            "id": "loc_boss_ragnahawk",
            "name": "Ragnahawk Defeat",
            "region": "ragnahawk_arena",
            "classification": "priority",
            "metadata": {
              "boss_level": 30
            }
          }
        ]
      }
    }
  }
}
```

**Location Types:**
- `static` - Fixed locations with known, unchanging positions (e.g., overworld chests at specific coordinates that are the same in every playthrough)
- `dynamic` - Locations whose positions are determined by game seed/RNG during world generation, but the set of possible locations is still declared in capabilities at generation time (e.g., dungeon chest locations that change per world seed, but the total number and types of chests are known). **Important**: This does NOT mean locations can be registered at runtime.
- `conditional` - Locations that may or may not exist in a particular playthrough based on player options selected during multiworld generation (e.g., a location that only exists if "Boss Rush Mode" option is enabled)

**CRITICAL NOTE**: All location types must be fully declared in mod capabilities **BEFORE** generation. Mods **CANNOT** register new locations at runtime after the capabilities file has been generated and the multiworld has been created. The "dynamic" type refers to procedural positioning within the game world, not dynamic registration in the AP framework.

**Location Classifications:**
- `default` - Standard location
- `priority` - Encouraged for progression items
- `excluded` - Should not contain progression items

#### Regions & Entrances
```json
{
  "capabilities": {
    "regions": [
      {
        "id": "starting_area",
        "name": "Starting Area",
        "exits": ["exit_to_forest"]
      },
      {
        "id": "forest",
        "name": "Forest",
        "exits": ["exit_to_starting_area", "exit_to_mountain"]
      }
    ],
    "entrances": [
      {
        "id": "exit_to_forest",
        "source_region": "starting_area",
        "target_region": "forest",
        "requirements": {
          "items": ["wooden_axe"]
        }
      }
    ]
  }
}
```

#### Options
```json
{
  "capabilities": {
    "options": {
      "shuffle_chests": {
        "type": "toggle",
        "default": true,
        "description": "Shuffle chest contents into the multiworld"
      },
      "boss_difficulty": {
        "type": "range",
        "default": 1.0,
        "min": 0.5,
        "max": 2.0,
        "description": "Boss health and damage multiplier"
      },
      "starting_technology": {
        "type": "choice",
        "default": "none",
        "choices": ["none", "basic", "advanced"],
        "description": "Technologies available at start"
      }
    }
  }
}
```

### Conflict Detection

**Conflict Types:**

1. **Item Name Conflict** - Two mods claim the same item name
   ```
   Mod A: items.weapons.items[{id: "sword", ...}]
   Mod B: items.tools.items[{id: "sword", ...}]
   → CONFLICT: Both mods own item "sword"
   ```

2. **Location ID Conflict** - Two mods claim the same location ID
   ```
   Mod A: locations.chests.locations[{id: "loc_chest_1", ...}]
   Mod B: locations.bosses.locations[{id: "loc_chest_1", ...}]
   → CONFLICT: Both mods own location "loc_chest_1"
   ```

3. **Incompatible Mod** - Mods explicitly marked incompatible
   ```
   Mod A: incompatible: [{id: "author.game.modB"}]
   Mod B detected
   → CONFLICT: Mod A is incompatible with Mod B
   ```

4. **Version Incompatibility**
   ```
   Mod A: incompatible: [{id: "author.game.modB", versions: "<=1.0.0"}]
   Mod B (version 0.9.0) detected
   → CONFLICT: Mod A incompatible with Mod B version 0.9.0
   ```

**Conflict Resolution:**
- **Zero-tolerance policy**: Any conflict results in REGISTRATION_FAILED for all involved mods
- User must manually resolve (disable one mod, update version, etc.)
- Framework enters ERROR_STATE until conflicts resolved
- Conflicts reported to both framework logs and involved mods

### Capability Aggregation

After validation, APCapabilities aggregates all mod capabilities:

```json
{
  "schema_version": "1.0.0",
  "game": "Palworld",
  "slot_name": "Player1",
  "generated_at": "2026-01-08T12:34:56Z",
  "mods": [
    {
      "mod_id": "author.palworld.techshuffle",
      "name": "Tech Shuffle",
      "version": "1.2.0"
    },
    {
      "mod_id": "author.palworld.chestshuffle",
      "name": "Chest Shuffle",
      "version": "2.0.1"
    }
  ],
  "capabilities": {
    "items": {
      "technologies": [...],  // From techshuffle mod
      "resources": [...]      // From chestshuffle mod
    },
    "locations": {
      "chests": [...],        // From chestshuffle mod
      "tech_tree": [...]      // From techshuffle mod
    },
    "regions": [...],
    "entrances": [...],
    "options": {
      // Merged from all mods
      "shuffle_chests": {...},
      "shuffle_technologies": {...},
      "boss_difficulty": {...}
    }
  },
  "metadata": {
    "total_items": 150,
    "total_locations": 200,
    "total_regions": 25
  }
}
```

This file is provided to the AP World (Python) for generation.

---

## AP World Integration (Python)

### Overview

The AP World (Python side) is a critical component that dynamically adapts to the capabilities files generated by potentially multiple players, each with their own UE4SS mod ecosystem. The World must be flexible enough to handle varying combinations of mods across different player slots.

### Key Concepts

**Multi-Slot Support:**
- Each player PC with the framework generates its own `APCapabilities_<slot_name>.json`
- Player 1 on PC A: `APCapabilities_Player1.json` (mods: techshuffle, chestshuffle)
- Player 2 on PC B: `APCapabilities_Player2.json` (mods: techshuffle, bossshuffle, regionshuffle)
- Player 3 on PC C: `APCapabilities_Player3.json` (mods: techshuffle, chestshuffle) - same as Player 1
- The AP World must handle all three capability files during generation

**Dynamic Adaptation:**
- World logic is NOT hard-coded for specific mods
- World reads capability files to determine what items/locations exist per slot
- World generates multiworld based on what each player's mods promise to handle

### Implementation Structure

```
worlds/palworld/
├── __init__.py           # Main PalworldWorld class
├── items.py              # Dynamic item generation from capabilities
├── locations.py          # Dynamic location generation from capabilities
├── regions.py            # Dynamic region generation from capabilities
├── rules.py              # Dynamic access rule application
├── options.py            # Player options (mod-independent)
├── capabilities.py       # Capability file parsing and validation
└── docs/
    ├── en_Palworld.md    # Game info page
    └── setup_en.md       # Setup tutorial
```

### PalworldWorld Class

```python
# worlds/palworld/__init__.py

from typing import Dict, Any, List, Set
from BaseClasses import World, Region, Item, Location, ItemClassification
from .capabilities import CapabilityManager
from .items import create_items_from_capabilities
from .locations import create_locations_from_capabilities
from .regions import create_regions_from_capabilities
from .rules import apply_rules_from_capabilities
from .options import PalworldOptions

class PalworldWorld(World):
    """
    Palworld AP World with dynamic capability-based generation.
    Each player slot can have a different set of mods installed.
    """
    game = "Palworld"
    options_dataclass = PalworldOptions
    options: PalworldOptions

    # Populated dynamically from capabilities
    item_name_to_id: Dict[str, int] = {}
    location_name_to_id: Dict[str, int] = {}

    def __init__(self, multiworld, player: int):
        super().__init__(multiworld, player)
        self.capability_manager = CapabilityManager(self.player)

    def generate_early(self) -> None:
        """
        Load and validate the player's capability file.
        This determines what items/locations are available for this slot.
        """
        # Load capability file for this slot
        # Expected location: user-provided or default path
        capability_file = self.get_capability_file_path()

        if not capability_file.exists():
            raise FileNotFoundError(
                f"Capability file not found for {self.player_name}. "
                f"Expected: {capability_file}\\n"
                f"Please generate capabilities by running the game with the framework."
            )

        # Parse and validate
        self.capability_manager.load_from_file(capability_file)
        validation_result = self.capability_manager.validate()

        if not validation_result.is_valid:
            raise ValueError(
                f"Invalid capability file for {self.player_name}:\\n" +
                "\\n".join(validation_result.errors)
            )

        # Build ID mappings from capabilities
        self.item_name_to_id = self.capability_manager.get_item_id_mapping()
        self.location_name_to_id = self.capability_manager.get_location_id_mapping()

    def create_regions(self) -> None:
        """
        Dynamically create regions based on this player's capabilities.
        """
        regions_data = self.capability_manager.get_regions()

        # Create regions
        for region_data in regions_data:
            region = Region(
                region_data["name"],
                self.player,
                self.multiworld
            )
            self.multiworld.regions.append(region)

            # Add locations to region
            location_ids = self.capability_manager.get_locations_for_region(region_data["id"])
            for loc_id in location_ids:
                loc_data = self.capability_manager.get_location_data(loc_id)
                location = PalworldLocation(
                    self.player,
                    loc_data["name"],
                    self.location_name_to_id[loc_data["name"]],
                    region
                )

                # Set location classification
                if loc_data.get("classification") == "excluded":
                    location.progress_type = LocationProgressType.EXCLUDED
                elif loc_data.get("classification") == "priority":
                    location.progress_type = LocationProgressType.PRIORITY

                region.locations.append(location)

        # Connect regions via entrances
        entrances_data = self.capability_manager.get_entrances()
        for entrance_data in entrances_data:
            source = self.multiworld.get_region(entrance_data["source_region"], self.player)
            target = self.multiworld.get_region(entrance_data["target_region"], self.player)
            source.connect(target, entrance_data.get("name", f"{source.name} -> {target.name}"))

    def create_items(self) -> None:
        """
        Dynamically create item pool based on this player's capabilities.
        """
        items_data = self.capability_manager.get_items()

        for item_data in items_data:
            classification = self._get_item_classification(item_data)

            # Handle different item types
            item_type = item_data.get("type", "toggle")

            if item_type == "progressive":
                # Create progressive items
                chain = item_data.get("progressive_chain", [])
                for stage in chain:
                    item = self.create_item(item_data["name"])
                    self.multiworld.itempool.append(item)

            elif item_type == "counter":
                # Create multiple copies for counter items
                count = item_data.get("count", 1)
                for _ in range(count):
                    item = self.create_item(item_data["name"])
                    self.multiworld.itempool.append(item)

            else:  # toggle or trap
                item = self.create_item(item_data["name"])
                self.multiworld.itempool.append(item)

        # Balance itempool and locations
        total_locations = len(self.multiworld.get_locations(self.player))
        items_created = len([i for i in self.multiworld.itempool if i.player == self.player])

        if items_created < total_locations:
            # Fill with filler items
            filler_count = total_locations - items_created
            for _ in range(filler_count):
                filler_item = self.create_filler()
                self.multiworld.itempool.append(filler_item)

    def set_rules(self) -> None:
        """
        Apply access rules from capabilities.
        """
        entrances_data = self.capability_manager.get_entrances()

        for entrance_data in entrances_data:
            requirements = entrance_data.get("requirements", {})
            if not requirements:
                continue

            entrance_name = entrance_data.get("name")
            if entrance_name:
                entrance = self.multiworld.get_entrance(entrance_name, self.player)
                entrance.access_rule = self._build_access_rule(requirements)

        # Set victory condition
        victory_event = self.capability_manager.get_victory_event()
        if victory_event:
            self.multiworld.completion_condition[self.player] = lambda state: state.has(victory_event, self.player)

    def create_item(self, name: str) -> Item:
        """Create an item by name."""
        item_data = self.capability_manager.get_item_data(name)
        classification = self._get_item_classification(item_data)

        return PalworldItem(
            name,
            classification,
            self.item_name_to_id[name],
            self.player
        )

    def fill_slot_data(self) -> Dict[str, Any]:
        """
        Send capability-derived data to the client.
        The framework will use this to validate consistency.
        """
        return {
            "capabilities_checksum": self.capability_manager.get_checksum(),
            "item_name_to_id": self.item_name_to_id,
            "location_name_to_id": self.location_name_to_id,
            "mods": self.capability_manager.get_mod_list(),
            "options": self.options.as_dict()
        }

    def _get_item_classification(self, item_data: Dict) -> ItemClassification:
        """Convert capability classification to AP classification."""
        classification_str = item_data.get("classification", "filler")

        if classification_str == "progression":
            return ItemClassification.progression
        elif classification_str == "useful":
            return ItemClassification.useful
        elif classification_str == "trap":
            return ItemClassification.trap
        else:
            return ItemClassification.filler

    def _build_access_rule(self, requirements: Dict) -> Callable:
        """Build an access rule lambda from requirements dict."""
        required_items = requirements.get("items", [])

        def rule(state):
            return all(state.has(item, self.player) for item in required_items)

        return rule

    def get_capability_file_path(self) -> Path:
        """
        Determine where to find this player's capability file.
        Could be from a config, user-specified path, or default location.
        """
        # Check for player-specific override
        player_cap_path = self.multiworld.player_name[self.player] + "_capabilities.json"

        # Check multiple locations
        search_paths = [
            Path(self.multiworld.output_path) / player_cap_path,
            Path.cwd() / "capabilities" / player_cap_path,
            Path.cwd() / player_cap_path,
        ]

        for path in search_paths:
            if path.exists():
                return path

        # Return expected default path (will error if not found)
        return search_paths[0]


class PalworldItem(Item):
    game = "Palworld"


class PalworldLocation(Location):
    game = "Palworld"
```

### Capability Manager

```python
# worlds/palworld/capabilities.py

import json
import hashlib
from pathlib import Path
from typing import Dict, List, Any, Optional
from dataclasses import dataclass

@dataclass
class ValidationResult:
    is_valid: bool
    errors: List[str]

class CapabilityManager:
    """
    Manages loading, parsing, and querying capability files.
    """

    def __init__(self, player: int):
        self.player = player
        self.capabilities: Optional[Dict] = None

    def load_from_file(self, filepath: Path) -> None:
        """Load capability JSON from file."""
        with open(filepath, 'r', encoding='utf-8') as f:
            self.capabilities = json.load(f)

    def validate(self) -> ValidationResult:
        """
        Validate capability file structure and content.
        """
        errors = []

        if not self.capabilities:
            errors.append("Capability file not loaded")
            return ValidationResult(False, errors)

        # Check required top-level fields
        required_fields = ["schema_version", "game", "slot_name", "capabilities"]
        for field in required_fields:
            if field not in self.capabilities:
                errors.append(f"Missing required field: {field}")

        # Validate game matches
        if self.capabilities.get("game") != "Palworld":
            errors.append(f"Game mismatch: expected Palworld, got {self.capabilities.get('game')}")

        # Validate schema version
        schema_version = self.capabilities.get("schema_version", "")
        if not schema_version.startswith("1."):
            errors.append(f"Unsupported schema version: {schema_version}")

        # Validate capabilities structure
        caps = self.capabilities.get("capabilities", {})

        if "items" not in caps or not isinstance(caps["items"], dict):
            errors.append("Capabilities must contain 'items' dictionary")

        if "locations" not in caps or not isinstance(caps["locations"], dict):
            errors.append("Capabilities must contain 'locations' dictionary")

        # Check for item/location ID uniqueness
        item_ids = set()
        for category in caps.get("items", {}).values():
            for item in category.get("items", []):
                item_id = item.get("id")
                if item_id in item_ids:
                    errors.append(f"Duplicate item ID: {item_id}")
                item_ids.add(item_id)

        location_ids = set()
        for category in caps.get("locations", {}).values():
            for location in category.get("locations", []):
                loc_id = location.get("id")
                if loc_id in location_ids:
                    errors.append(f"Duplicate location ID: {loc_id}")
                location_ids.add(loc_id)

        return ValidationResult(len(errors) == 0, errors)

    def get_checksum(self) -> str:
        """Calculate checksum of capability file for validation."""
        if not self.capabilities:
            return ""

        # Canonicalize JSON and hash
        canonical = json.dumps(self.capabilities, sort_keys=True)
        return hashlib.sha256(canonical.encode()).hexdigest()[:16]

    def get_item_id_mapping(self) -> Dict[str, int]:
        """Build item name -> ID mapping."""
        mapping = {}
        base_id = 1000  # Offset for Palworld items

        caps = self.capabilities.get("capabilities", {})
        for category in caps.get("items", {}).values():
            for item in category.get("items", []):
                mapping[item["name"]] = base_id
                base_id += 1

        return mapping

    def get_location_id_mapping(self) -> Dict[str, int]:
        """Build location name -> ID mapping."""
        mapping = {}
        base_id = 2000  # Offset for Palworld locations

        caps = self.capabilities.get("capabilities", {})
        for category in caps.get("locations", {}).values():
            for location in category.get("locations", []):
                mapping[location["name"]] = base_id
                base_id += 1

        return mapping

    def get_items(self) -> List[Dict]:
        """Get all items from capabilities."""
        items = []
        caps = self.capabilities.get("capabilities", {})

        for category, category_data in caps.get("items", {}).items():
            for item in category_data.get("items", []):
                items.append({
                    **item,
                    "category": category,
                    "type": category_data.get("type", "toggle")
                })

        return items

    def get_locations(self) -> List[Dict]:
        """Get all locations from capabilities."""
        locations = []
        caps = self.capabilities.get("capabilities", {})

        for category, category_data in caps.get("locations", {}).items():
            for location in category_data.get("locations", []):
                locations.append({
                    **location,
                    "category": category,
                    "type": category_data.get("type", "static")
                })

        return locations

    def get_regions(self) -> List[Dict]:
        """Get all regions from capabilities."""
        caps = self.capabilities.get("capabilities", {})
        return caps.get("regions", [])

    def get_entrances(self) -> List[Dict]:
        """Get all entrances from capabilities."""
        caps = self.capabilities.get("capabilities", {})
        return caps.get("entrances", [])

    def get_locations_for_region(self, region_id: str) -> List[str]:
        """Get all location IDs for a specific region."""
        return [
            loc["id"] for loc in self.get_locations()
            if loc.get("region") == region_id
        ]

    def get_item_data(self, name: str) -> Dict:
        """Get item data by name."""
        for item in self.get_items():
            if item["name"] == name:
                return item
        raise ValueError(f"Item not found: {name}")

    def get_location_data(self, location_id: str) -> Dict:
        """Get location data by ID."""
        for location in self.get_locations():
            if location["id"] == location_id:
                return location
        raise ValueError(f"Location not found: {location_id}")

    def get_mod_list(self) -> List[Dict]:
        """Get list of mods that contributed to capabilities."""
        return self.capabilities.get("mods", [])

    def get_victory_event(self) -> Optional[str]:
        """Get the victory event item name if defined."""
        # Look for a special victory event in capabilities
        caps = self.capabilities.get("capabilities", {})
        victory = caps.get("victory_event")
        return victory.get("item_name") if victory else None
```

### Ecosystem Mismatch Detection

When a player connects to the AP server, the framework should verify that their current mod ecosystem matches what was used during generation:

```python
# In PalworldWorld
def verify_slot_data(self, client_capabilities_checksum: str) -> bool:
    """
    Called when client connects. Verify their capability checksum matches
    what was used during generation.
    """
    generated_checksum = self.capability_manager.get_checksum()

    if client_capabilities_checksum != generated_checksum:
        # Mismatch detected
        return False

    return True
```

**Framework Side (C++):**

When connecting, the framework sends its current capabilities checksum in slot_data. Upon receiving Connected packet, it compares:

```cpp
// In APClient::handle_connected()
auto slot_data = connected_packet["slot_data"];
std::string expected_checksum = slot_data["capabilities_checksum"];
std::string current_checksum = calculate_current_checksum();

if (expected_checksum != current_checksum) {
    // Send warning to all mods
    broadcast_ecosystem_mismatch_warning(expected_checksum, current_checksum);

    // Optionally: prevent gameplay until resolved
    // enter_error_state("Mod ecosystem changed since generation");
}
```

### Pre-Gameplay Generation Workflow

**Critical Requirement:** Capabilities must be generated **before** the player starts actual gameplay (past the main menu).

**Workflow:**

```
1. Player starts game → Main menu appears
   ↓
2. UE4SS loads APFrameworkMod at game startup
   ↓
3. Framework initializes while at main menu
   - IPC server starts
   - Mod discovery happens
   - Mods register (can happen without world loaded)
   ↓
4. Framework validates and generates capabilities
   - All happens at main menu
   - Output: APCapabilities_<slot_name>.json
   ↓
5. Player STAYS at main menu
   - Can now exit game
   - Provide capability file to host
   ↓
6. Host uses Archipelago launcher
   - Generates multiworld with all player capability files
   - Creates AP_*.zip file
   - Hosts the multiworld
   ↓
7. Players restart game
   - Framework re-initializes
   - Framework validates mod ecosystem hasn't changed
   - Framework connects to AP server
   - Framework syncs state with server
   ↓
8. Framework reaches RUNNING state
   - Player can NOW start actual gameplay
   - Load save / start new game
```

**UE4SS Timing:**

UE4SS loads mods based on load order in `mods.txt`/`mods.json`. This happens early in game startup, typically before the main menu is fully rendered. Mods can:
- Initialize C++/Lua code
- Set up IPC connections
- Register with framework
- Generate capabilities

All of this can occur while the game is at the main menu, without requiring a world to be loaded.

### Summary

The AP World (Python) integration provides:

1. **Dynamic Multi-Slot Support**: Each player can have different mods
2. **Capability-Based Generation**: World logic adapts to what mods promise
3. **Validation**: Checksums ensure mod ecosystem consistency
4. **Pre-Gameplay Generation**: Capabilities generated at main menu, before gameplay
5. **Flexible Architecture**: Easy to extend for new games/mechanics

---

## Lifecycle & State Management

### State Diagram

```
                    ┌────────────────┐
                    │ UNINITIALIZED  │
                    └────────┬───────┘
                             │ initialize()
                             ▼
                    ┌────────────────┐
                    │DISCOVERING_MODS│
                    └────────┬───────┘
                             │ discover_mods()
                             ▼
              ┌──────────────────────────┐
              │AWAITING_PRIORITY_         │
              │REGISTRATION              │
              └──────────┬───────────────┘
                         │ priority timeout or all registered
                         ▼
              ┌──────────────────────────┐
              │AWAITING_REGULAR_          │
              │REGISTRATION              │
              └──────────┬───────────────┘
                         │ regular timeout or all registered
                         ▼
              ┌──────────────────────────┐
              │VALIDATING_CAPABILITIES   │
              └──────────┬───────────────┘
                         │ validate()
                    ┌────┴────┐
                    │         │
         conflicts? ▼         ▼ no conflicts
          ┌──────────────┐   ┌──────────────────────┐
          │ ERROR_STATE  │   │GENERATING_CAPABILITIES│
          └──────────────┘   └──────────┬───────────┘
                                        │ generate()
                                        ▼
                             ┌────────────────────────┐
                             │READY_FOR_CONNECTION    │
                             └──────────┬─────────────┘
                                        │ CMD_CONNECT
                                        ▼
                             ┌────────────────────────┐
                             │CONNECTING_TO_AP        │
                             └──────────┬─────────────┘
                                        │ Connected packet received
                                        ▼
                             ┌────────────────────────┐
                             │CONNECTED_AND_SYNCING   │
                             └──────────┬─────────────┘
                                        │ sync complete
                                        ▼
                             ┌────────────────────────┐
                             │     RUNNING            │◄─┐
                             └────────┬───────────────┘  │
                                      │                  │
                                      │ CMD_RESYNC       │
                                      └──────────────────┘
```

### State Transitions

**UNINITIALIZED → DISCOVERING_MODS**
- Trigger: `APManager::initialize()` called
- Actions:
  - Load configuration
  - Initialize logging
  - Create all components
  - Start IPC server

**DISCOVERING_MODS → AWAITING_PRIORITY_REGISTRATION**
- Trigger: `APModRegistry::discover_mods()` completes
- Actions:
  - Scan mods directory for AP_Config.json files
  - Parse and categorize mods (priority vs. regular)
  - Create registration promises
  - Broadcast LIFECYCLE_STATE to inform mods (if any connected early)

**AWAITING_PRIORITY_REGISTRATION → AWAITING_REGULAR_REGISTRATION**
- Trigger: All priority clients registered OR timeout (default: 5s)
- Actions:
  - Validate priority client registrations
  - Check incompatible mods among priority clients
  - If validation fails: → ERROR_STATE
  - If validation passes: proceed to await regular clients

**AWAITING_REGULAR_REGISTRATION → VALIDATING_CAPABILITIES**
- Trigger: All regular clients registered OR timeout (default: 10s)
- Actions:
  - Log any unregistered mods as warnings
  - Proceed to capability validation

**VALIDATING_CAPABILITIES → GENERATING_CAPABILITIES or ERROR_STATE**
- Trigger: `APCapabilities::detect_conflicts()` completes
- Actions:
  - If conflicts detected:
    - Send REGISTRATION_FAILED to all conflicting mods
    - Log detailed conflict report
    - → ERROR_STATE
  - If no conflicts:
    - → GENERATING_CAPABILITIES

**GENERATING_CAPABILITIES → READY_FOR_CONNECTION**
- Trigger: `APCapabilities::generate_capabilities_file()` completes
- Actions:
  - Write APCapabilities_<slot_name>.json to disk
  - Send REGISTRATION_SUCCESS to all registered mods
  - Broadcast LIFECYCLE_STATE: READY_FOR_CONNECTION

**READY_FOR_CONNECTION → CONNECTING_TO_AP**
- Trigger: CMD_CONNECT received (from priority client or auto-start)
- Actions:
  - Call APClient::connect() with provided or configured server info
  - Initiate WebSocket handshake
  - Broadcast LIFECYCLE_STATE: CONNECTING_TO_AP

**CONNECTING_TO_AP → CONNECTED_AND_SYNCING**
- Trigger: Connected packet received from AP server
- Actions:
  - Extract slot info, team, players, etc.
  - Send Sync packet to request ReceivedItems
  - Start APPollingThread (if not already running)
  - Broadcast LIFECYCLE_STATE: CONNECTED_AND_SYNCING

**CONNECTED_AND_SYNCING → RUNNING**
- Trigger: Initial ReceivedItems processed
- Actions:
  - Route items to mods
  - Send StatusUpdate to AP server: CLIENT_PLAYING
  - Broadcast LIFECYCLE_STATE: RUNNING

**RUNNING → RUNNING (resync)**
- Trigger: CMD_RESYNC received (from priority client)
- Actions:
  - Broadcast RESYNC to all mods (mods should reset state)
  - Re-discover mods (in case user added/removed mods)
  - Repeat registration → validation → generation flow
  - Reconnect to AP server with fresh sync
  - Prevents need for full game restart when modifying mods

**Any State → ERROR_STATE**
- Trigger: Critical error (capability conflict, connection failure, etc.)
- Actions:
  - Log error details
  - Broadcast LIFECYCLE_STATE: ERROR_STATE with error info
  - Stop polling thread (if running)
  - Close AP server connection (if connected)
  - Keep IPC server running for error reporting to mods
- Recovery: User must resolve issue and trigger resync or restart

---

## Error Handling & Validation

### Error Categories

1. **Configuration Errors**
   - Missing framework_config.json
   - Invalid JSON syntax
   - Missing required fields
   - Invalid values (e.g., negative port)

2. **Mod Discovery Errors**
   - Mods directory not found
   - AP_Config.json parse errors
   - Invalid mod_id format
   - Missing required fields in AP_Config.json

3. **Registration Errors**
   - Mod timeout (didn't register in time)
   - Duplicate mod_id
   - Invalid capabilities schema

4. **Capability Validation Errors**
   - Item/location name conflicts
   - Incompatible mods
   - Version incompatibilities
   - Schema validation failures

5. **Connection Errors**
   - Cannot reach AP server
   - Authentication failure (wrong password, invalid slot)
   - Protocol version mismatch
   - Connection dropped during gameplay

6. **IPC Errors**
   - IPC server failed to start (port in use, permissions)
   - Mod disconnected unexpectedly
   - Message parse errors

### Error Handling Strategies

**Configuration Errors:**
- Log error with specific issue
- Provide default values where safe
- Fatal: Exit initialization if critical fields missing

**Mod Discovery Errors:**
- Log warning for each problematic mod
- Continue with valid mods
- User can fix and trigger resync

**Registration Errors:**
- Send REGISTRATION_FAILED to mod
- Provide specific error message
- Allow retry after user fixes issue

**Capability Validation Errors:**
- Send REGISTRATION_FAILED to ALL conflicting mods
- Enter ERROR_STATE
- Require user intervention (disable mod, update version)
- Allow CMD_RESYNC to retry after fix

**Connection Errors:**
- Authentication failures: Display error, allow retry with new credentials
- Connection drops: Auto-reconnect if configured, preserve state
- Protocol mismatch: Fatal, log error, inform user to update

**IPC Errors:**
- Log errors but continue operation
- If mod disconnects: Mark as unregistered, inform other mods if needed
- Parse errors: Log and discard message, don't crash

### Logging

**Log Levels:**
- `ERROR` - Critical errors requiring attention
- `WARN` - Non-critical issues (missing optional fields, unregistered mods)
- `INFO` - State transitions, connections, registrations
- `DEBUG` - Detailed message tracing, polling loop iterations

**Log Format:**
```
[2026-01-08 12:34:56.789] [LEVEL] [Component] Message
```

**Examples:**
```
[2026-01-08 12:34:56.123] [INFO] [APManager] Initializing framework core
[2026-01-08 12:34:56.234] [INFO] [APModRegistry] Discovered 5 mods (2 priority, 3 regular)
[2026-01-08 12:34:57.345] [WARN] [APModRegistry] Mod 'old.mod.id' did not register within timeout
[2026-01-08 12:34:58.456] [ERROR] [APCapabilities] Conflict detected: Item 'Sword' claimed by both 'mod.a' and 'mod.b'
[2026-01-08 12:35:00.567] [INFO] [APClient] Connected to AP server at archipelago.gg:38281
[2026-01-08 12:35:00.678] [DEBUG] [APPollingThread] Poll iteration, 3 messages received
```

---

## Implementation Considerations

### Threading Model

**Main Thread (UE4SS):**
- APFrameworkMod Lua execution
- Lua bindings calls into APFrameworkCore
- IPC message sending (synchronous)

**IPC Server Thread(s):**
- Accept client connections
- Receive messages from mods
- Send messages to mods
- Message routing to handlers

**Polling Thread:**
- 60fps loop (16ms)
- Calls APClient::poll() (WebSocket event processing)
- Retrieves messages
- Routes via APMessageRouter
- **No blocking operations** in polling loop

**Thread Safety Considerations:**
- All APManager component accessors must be thread-safe
- Message buffers require mutexes (APClient::message_buffer_, APIPCClient::pending_messages_)
- APModRegistry::mods_ requires mutex (registration can happen from IPC thread)
- APCapabilities read-only after generation (no mutex needed during RUNNING state)

### UE4SS Lua Integration

**Correct UE4SS API Usage:**

UE4SS provides specific hooks for mod initialization. It's critical to use the **correct** API functions:

**❌ INCORRECT (Hallucinated APIs):**
- `RegisterInitGameStateHook` - Does NOT exist
- `RegisterUnrealEngineShutdownCallback` - Does NOT exist

**✅ CORRECT APIs:**
- `RegisterInitGamePreStateHook()` - Called before game state initialization (may be called multiple times)
- `RegisterInitGamePostStateHook()` - Called after game state initialization (may be called multiple times)
- `RegisterCustomEvent("Tick", callback)` - Called continuously during game loop (**RECOMMENDED**)

**IMPORTANT**: Game state hooks (`RegisterInitGamePreStateHook` / `RegisterInitGamePostStateHook`) may:
1. Be called multiple times if game state changes
2. Not be called at all if the game doesn't use that initialization pattern
3. Not be guaranteed to fire at startup

**Recommended: Use Tick Event for Initialization**

The `RegisterCustomEvent("Tick", ...)` approach is **nearly guaranteed** to work across all games and provides:
- Continuous execution regardless of game state
- Early startup opportunity (fires as soon as blueprint Tick functions run)
- Reliable cross-game compatibility

**Lua Tick Example:**
```lua
-- APFrameworkMod/Scripts/main.lua
local current_time = os.clock()
local last_time = current_time
local is_initialized = false

-- Operations here run in the shared external UE4SS thread (does not impact game thread)
-- Execution for all other UE4SS mods are blocked until we return the execution
-- Can spawn other asynchronous operations here (like inside a C++ lib), but Lua state will change if we call LoopAsync or ExecuteWithDelay here

RegisterCustomEvent("Tick", function()
    -- Operations here run in the game thread (likely within a blueprint)
    -- Can check/set is_initialized here, perform any kind of periodic updates, or simply return if already initialized and nothing else needs to be done
    -- Note that this may be called multiple times per frame since it could be from multiple sources of blueprint Tick functions, which is why we manage our own delta_time via os.clock()
    -- To ensure we aren't locking up the game thread too much, operations here should either be kept to a minimum, or spawn other asynchronous operations
    current_time = os.clock()
    local delta_time = (current_time - last_time)

    -- Initialize framework once
    if not is_initialized then
        print("[APFrameworkMod] Initializing Archipelago Framework...")
        local success, err = pcall(function()
            local APFramework = require("APFramework")
            APFramework:init("framework_config.json")
            APFramework:start()
        end)

        if success then
            print("[APFrameworkMod] Framework initialized successfully!")
            is_initialized = true
        else
            print("[APFrameworkMod] ERROR: " .. tostring(err))
            -- Retry on next tick
        end

        last_time = current_time
        return
    end

    -- Periodic operations (once per second after initialization)
    if delta_time >= 1.0 then
        last_time = current_time
        -- Optional: periodic health checks, statistics, etc.
    end
end)
```

**Lifecycle Management & Memory Safety:**

⚠️ **CRITICAL**: There is **NO reliable shutdown hook** in UE4SS!

**Why This Matters:**
- No guaranteed callback when mods unload
- Shutdown order between mods is undefined
- Game/UE4SS crashes may bypass all cleanup
- Objects accessed during shutdown may already be destroyed

**Design Requirements:**
1. **Use Smart Pointers Everywhere**: `std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr`
2. **Self-Managed Lifecycles**: Components must clean up automatically via destructors
3. **No Manual shutdown() Dependency**: Framework must be safe even if shutdown() is never called
4. **Timeout-Based Resource Management**: Use timeouts to detect and clean up stale connections
5. **RAII (Resource Acquisition Is Initialization)**: All resources must be tied to object lifetime

**APManager::shutdown() is for convenience only** - it should NOT be required for memory safety!

**Implementation Checklist:**
- ✅ All heap allocations use smart pointers
- ✅ All threads are joined in destructors (with timeouts)
- ✅ All file handles use RAII wrappers (ofstream, etc.)
- ✅ All IPC connections detect disconnection and clean up automatically
- ✅ Polling thread checks lifecycle state and stops gracefully
- ✅ No dangling pointers or memory leaks even if shutdown() is never called
- ✅ Timeouts prevent indefinite waits during cleanup

**Example - APPollingThread Lifecycle:**
```cpp
class APPollingThread {
private:
    std::thread polling_thread_;
    std::atomic<bool> should_stop_{false};

public:
    ~APPollingThread() {
        // Automatic cleanup in destructor
        should_stop_ = true;

        if (polling_thread_.joinable()) {
            // Use timeout to avoid indefinite blocking
            auto future = std::async(std::launch::async, [this]() {
                polling_thread_.join();
            });

            if (future.wait_for(std::chrono::seconds(2)) == std::future_status::timeout) {
                // Thread didn't exit gracefully - detach and log warning
                polling_thread_.detach();
                // This is acceptable since we're shutting down anyway
            }
        }
    }
};
```

### Performance Optimization

**Polling Frequency:**
- Default: 16ms (60fps) matches typical game loop
- Configurable via framework_config.json
- Higher frequency = more responsive but more CPU
- Lower frequency = more latency for item receipt

**Message Batching:**
- LocationChecks should be batched if multiple checks happen in one frame
- ReceivedItems already batched by AP server

**IPC Performance:**
- Named Pipes: Async I/O for non-blocking operation
- Message size limit: Reasonable max (e.g., 1MB) to prevent abuse
- Connection pooling: Reuse connections, don't recreate

**Capability Caching:**
- After generation, capabilities are read-only
- Build lookup tables (item_to_mod_, location_to_mod_) once
- Use maps/hash tables for O(1) routing

### Memory Management

**Component Ownership:**
- APManager owns all components via `std::unique_ptr`
- Components hold non-owning pointers to each other (set via set_*_ref methods)
- Clear ownership hierarchy prevents circular references

**Message Lifetime:**
- IPCMessages copied into buffers, not held by reference
- APMessages copied from apclientpp, processed, then discarded
- Lua bindings: Lua owns its objects, C++ holds weak references

**Resource Cleanup:**
- APManager::shutdown() calls component destructors in reverse dependency order
- IPC server closes all client connections
- Polling thread joins before APClient destruction
- WebSocket closed before apclientpp destruction

### Cross-Platform Considerations

**IPC Mechanism:**
- Windows: Named Pipes (`\\.\\pipe\\APFramework`)
- Linux/Mac: Unix Domain Sockets (`/tmp/APFramework.sock`) or TCP localhost
- Abstraction layer: `IPCServerImpl` and `IPCClientImpl` with platform-specific subclasses

**File Paths:**
- Use `std::filesystem::path` for cross-platform path handling
- Normalize path separators
- Handle case sensitivity differences

**Threading:**
- Use C++ standard library threading (`std::thread`, `std::mutex`, `std::atomic`)
- No platform-specific threading APIs

**JSON:**
- nlohmann::json is cross-platform
- Use UTF-8 encoding consistently

### Security Considerations

**IPC Security:**
- Named Pipes: Use appropriate ACLs to restrict access to local user
- Validate all incoming messages (schema validation)
- Rate limiting to prevent DoS from malicious mods

**Input Validation:**
- Validate mod_id format (alphanumeric + dots)
- Validate version strings (semver)
- Sanitize file paths (prevent directory traversal)
- Validate JSON schema for capabilities

**Error Information:**
- Don't expose internal paths in error messages sent to mods
- Log sensitive info locally, send sanitized errors over IPC

### Extensibility Hooks

**Custom Commands:**
- Priority clients can define custom CMD_* commands
- Framework can route custom commands to registered handlers
- Example: CMD_DEBUG_DUMP, CMD_RELOAD_CONFIG

**Lifecycle Hooks:**
- Mods can register callbacks for lifecycle events
- Example: on_capability_generation_complete, on_ap_connection_lost

**Custom Metadata:**
- Capabilities schema allows arbitrary `metadata` fields
- Mods can store game-specific data
- Framework doesn't validate metadata, passes through to AP World

---

## Appendix: Message Sequence Diagrams

### Initialization Sequence

```
UE4SS          APFrameworkMod   APFrameworkCore   APIPCServer   OtherMod
  │                  │                  │                │           │
  ├─LoadMod─────────>│                  │                │           │
  │                  ├─initialize()────>│                │           │
  │                  │                  ├─start()───────>│           │
  │                  │                  │<───started─────┤           │
  │                  │                  ├─discover_mods()│           │
  │                  │                  │                │           │
  │                  ├─register_mod()──>│                │           │
  │                  │<─REGISTER────────┤                │           │
  │                  │                  │                │           │
  ├─LoadMod────────────────────────────────────────────────────────>│
  │                  │                  │                │<─connect()─┤
  │                  │                  │                │           │
  │                  │                  │<───────────────┼─REGISTER──┤
  │                  │                  │                │           │
  │                  │                  ├─validate()     │           │
  │                  │                  │                │           │
  │                  │<─REGISTRATION_SUCCESS─────────────┼───────────┤
  │                  │                  ├─generate()     │           │
  │                  │                  │                │           │
  │                  │<─LIFECYCLE:READY_FOR_CONNECTION───┼───────────┤
  │                  │                  │                │           │
```

### Runtime Item Receipt Sequence

```
APServer    APClient(poll)  MessageRouter  IPCServer    Mod
   │              │              │             │          │
   ├─ReceivedItems>│              │             │          │
   │              ├─poll()       │             │          │
   │              ├─get_messages>│             │          │
   │              │              ├─route()     │          │
   │              │              ├─(lookup)    │          │
   │              │              ├─send_to_mod>│          │
   │              │              │             ├─IPC───> │
   │              │              │             │          ├─callback()
   │              │              │             │          ├─apply_item()
   │              │              │             │          │
```

### Location Check Sequence

```
GameEvent    Mod        IPCClient   IPCServer   APClient   APServer
   │          │              │           │           │          │
   ├─OnChest─>│              │           │           │          │
   │          ├─check_loc()  │           │           │          │
   │          ├─send()──────>│           │           │          │
   │          │              ├─IPC──────>│           │          │
   │          │              │           ├─forward──>│          │
   │          │              │           │           ├─send────>│
   │          │              │           │           │          ├─process()
   │          │              │           │           │<────────┤
   │          │              │           │           │          │
```

---

## Summary

This architecture provides a robust, extensible framework for integrating UE4SS mods with Archipelago. Key strengths:

1. **Clear Component Separation**: Each component has a well-defined responsibility
2. **Flexible IPC**: Mods communicate via a clean message-passing interface
3. **Dynamic Capabilities**: Mods declare what they can do, framework validates and aggregates
4. **Conflict Prevention**: Zero-tolerance policy prevents incompatible mods from breaking generation
5. **Graceful Degradation**: Errors are handled at appropriate levels, allowing recovery where possible
6. **Performance**: 60fps polling, async I/O, efficient routing
7. **Extensibility**: Priority clients can extend framework without modifying core

**Next Steps for Implementation:**
1. Implement APManager singleton and basic lifecycle
2. Implement APConfig and configuration loading
3. Implement APIPCServer with Named Pipes (Windows)
4. Implement APClientLib and basic Lua bindings
5. Implement APModRegistry and discovery logic
6. Implement APCapabilities schema and validation
7. Implement APClient wrapper around apclientpp
8. Implement APMessageRouter
9. Implement APPollingThread
10. Create APFrameworkMod Lua entry point
11. Write comprehensive tests for each component
12. Create example AP Client Mod
13. Document API for mod developers