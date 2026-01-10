# IPC Message Flow Reference

This document clarifies the bidirectional message flow between mods, the framework, and the AP server.

---

## Overview

The framework uses different IPC message types for different flow directions:

- **`notification`**: Client → Server (game events like location checks)
- **`request`**: Client → Server (requesting information, expects response)
- **`response`**: Server → Client (responding to requests)
- **`ap_message`**: Server → Client (AP protocol messages from AP server)
- **`command`**: Priority Client → Server (framework commands)
- **`register`**: Client → Server (mod registration)

---

## Message Flow: Location Check (Mod → AP Server)

**Scenario**: Player opens a chest in-game, mod needs to report this to AP server.

```
┌─────────────┐      ┌──────────────┐      ┌──────────┐      ┌───────────┐
│  Game Event │──1──>│     Mod      │──2──>│Framework │──3──>│ AP Server │
│(Chest Open) │      │              │      │          │      │           │
└─────────────┘      └──────────────┘      └──────────┘      └───────────┘
```

### Step-by-Step Flow:

1. **Game Event Occurs**
   - Player opens chest with ID `chest_42`
   - Mod's hook detects event: `OnChestOpened(chest_42)`

2. **Mod Sends IPC NOTIFICATION to Framework**
   ```json
   {
     "type": "notification",
     "from_mod_id": "author.palworld.chestshuffle",
     "data": {
       "event": "LOCATION_CHECK",
       "location_id": 12345
     }
   }
   ```

3. **Framework Processes Message**
   - `APIPCServer` receives IPC message
   - Routes to `APManager`
   - `APManager` forwards to `APClient`

4. **APClient Sends to AP Server**
   - `APClient::send_location_checks([12345])`
   - WebSocket packet `LocationChecks` sent to AP server

5. **AP Server Processes**
   - Determines which player should receive the item at location 12345
   - Sends `ReceivedItems` packet to that player's client
   - Sends `PrintJSON` to all players with notification

### Key Points:

- **Mod Responsibility**: The mod that declared location 12345 in its capabilities is **solely responsible** for:
  - Detecting when the chest is opened
  - Sending the LOCATION_CHECK notification
  - The framework does NOT detect game events - only the mod can do this

- **Message Type**: Uses `notification` type, NOT `ap_message`
  - `ap_message` is reserved for messages FROM the AP server

---

## Message Flow: Item Receipt (AP Server → Mod)

**Scenario**: Another player checked a location, AP server sends an item to this player.

```
┌───────────┐      ┌──────────┐      ┌──────────────┐      ┌──────────┐
│ AP Server │──1──>│Framework │──2──>│ Mod (Owner)  │──3──>│ Game     │
│           │      │          │      │              │      │(Apply)   │
└───────────┘      └──────────┘      └──────────────┘      └──────────┘
```

### Step-by-Step Flow:

1. **AP Server Sends Item**
   - WebSocket packet: `ReceivedItems`
   ```json
   {
     "cmd": "ReceivedItems",
     "items": [
       {
         "item": 1001,
         "location": 2001,
         "player": 1,
         "flags": 1,
         "index": 42
       }
     ]
   }
   ```

2. **APClient Receives (Polling Thread)**
   - `APPollingThread` calls `APClient::poll()` (60fps)
   - `APClient::get_messages()` retrieves buffered messages
   - Returns `APMessage` with type `ReceivedItems`

3. **APMessageRouter Routes Message**
   - Looks up item 1001 ownership from `APCapabilities`
   - Determines item belongs to mod `author.palworld.techshuffle`
   - Builds IPC message for that mod

4. **Framework Sends IPC AP_MESSAGE to Mod**
   ```json
   {
     "type": "ap_message",
     "to_mod_id": "author.palworld.techshuffle",
     "data": {
       "ap_type": "ReceivedItems",
       "payload": {
         "index": 42,
         "items": [
           {
             "item": 1001,
             "location": 2001,
             "player": 1,
             "flags": 1
           }
         ],
         "item_name": "Advanced Workbench",
         "location_name": "Forest Boss",
         "player_name": "Player2"
       }
     }
   }
   ```

5. **Mod Receives and Applies**
   - `APIPCClient::get_messages()` or callback receives message
   - Mod validates item index (to avoid duplicates on reload)
   - Mod applies item effect: `UnlockTechnology("Advanced Workbench")`
   - Mod saves item index to prevent re-application

### Key Points:

- **Message Type**: Uses `ap_message` type
  - Only used for messages originating from AP server
  - Framework routes to specific mod based on capabilities

- **Item Ownership**: Determined by capabilities
  - Mod that declared item 1001 receives the message
  - APMessageRouter uses `APCapabilities::get_mods_owning_item()` for routing

---

## Message Flow: Location Scout (Mod ↔ AP Server)

**Scenario**: Mod wants to preview what items are at certain locations.

```
┌──────┐      ┌──────────┐      ┌───────────┐      ┌──────────┐      ┌──────┐
│ Mod  │──1──>│Framework │──2──>│ AP Server │──3──>│Framework │──4──>│ Mod  │
│      │      │          │      │           │      │          │      │      │
└──────┘      └──────────┘      └───────────┘      └──────────┘      └──────┘
```

### Step-by-Step Flow:

1. **Mod Sends IPC REQUEST**
   ```json
   {
     "type": "request",
     "msg_id": "uuid-5678",
     "from_mod_id": "author.palworld.chestshuffle",
     "data": {
       "event": "LOCATION_SCOUT",
       "location_ids": [12345, 12346, 12347],
       "create_as_hint": 0
     }
   }
   ```

2. **Framework Forwards to AP Server**
   - `APClient::send_location_scouts([12345, 12346, 12347], 0)`
   - WebSocket packet sent

3. **AP Server Responds**
   - WebSocket packet: `LocationInfo`
   ```json
   {
     "cmd": "LocationInfo",
     "locations": [
       {
         "item": 1001,
         "location": 12345,
         "player": 2,
         "flags": 1
       },
       ...
     ]
   }
   ```

4. **Framework Sends IPC AP_MESSAGE Response**
   ```json
   {
     "type": "ap_message",
     "msg_id": "uuid-5678",
     "to_mod_id": "author.palworld.chestshuffle",
     "data": {
       "ap_type": "LocationInfo",
       "payload": {
         "locations": [
           {
             "item": 1001,
             "location": 12345,
             "player": 2,
             "flags": 1,
             "item_name": "Advanced Workbench",
             "location_name": "Forest Chest 1",
             "player_name": "Player2"
           },
           ...
         ]
       }
     }
   }
   ```

5. **Mod Uses Location Info**
   - Displays tooltip: "This chest contains Advanced Workbench for Player2"
   - Or implements hint system based on scouted information

---

## Message Flow: Lifecycle Notifications (Framework → All Mods)

**Scenario**: Framework state changes, all mods need to know.

```
┌──────────┐      ┌────────────┐
│Framework │──1──>│  All Mods  │
│          │      │ (Broadcast)│
└──────────┘      └────────────┘
```

### Example: RUNNING State

```json
{
  "type": "notification",
  "to_mod_id": "",
  "data": {
    "event": "LIFECYCLE_STATE",
    "state": "RUNNING",
    "previous_state": "CONNECTED_AND_SYNCING"
  }
}
```

**Broadcast Types**:
- All connected mods: `APIPCServer::broadcast()`
- Priority clients only: `APIPCServer::broadcast_to_priority_clients()`

---

## Message Flow: Priority Client Commands

**Scenario**: Priority client (like a UI mod) wants to trigger framework action.

```
┌──────────────────┐      ┌──────────┐
│ Priority Client  │──1──>│Framework │
│ (UI Mod)         │      │          │
└──────────────────┘      └──────────┘
```

### Example: CMD_CONNECT

```json
{
  "type": "command",
  "from_mod_id": "archipelago.palworld.framework_ui",
  "msg_id": "uuid-9999",
  "data": {
    "cmd": "CMD_CONNECT",
    "server": "archipelago.gg",
    "port": 38281,
    "slot_name": "Player1"
  }
}
```

**Response**:
```json
{
  "type": "response",
  "msg_id": "uuid-9999",
  "to_mod_id": "archipelago.palworld.framework_ui",
  "data": {
    "success": true,
    "message": "Connection initiated"
  }
}
```

---

## Complete Message Type Reference

| IPC Type | Direction | Purpose | Example Use Case |
|----------|-----------|---------|------------------|
| `register` | Client → Server | Mod registration | Mod connects and identifies itself |
| `command` | Priority Client → Server | Framework control | UI mod triggers connection/generation |
| `request` | Client → Server | Information request | Location scout, data query |
| `response` | Server → Client | Response to request | Registration result, command result |
| `notification` | Bidirectional | Events/status updates | Location check (C→S), lifecycle state (S→C) |
| `ap_message` | Server → Client ONLY | AP protocol messages | ReceivedItems, LocationInfo, PrintJSON |

---

## Key Architectural Principles

1. **Mod Ownership and Responsibility**
   - Mods that declare locations/items in capabilities OWN those entities
   - Mods are solely responsible for detecting when their locations are checked
   - Framework routes based on ownership declared in capabilities

2. **Message Type Semantics**
   - `ap_message`: Reserved for messages FROM AP server
   - `notification`: Used for game events FROM mods
   - Never confuse these two - they flow in opposite directions

3. **Routing Logic**
   - Outbound (Mod → AP): Direct forwarding (mod knows what to send)
   - Inbound (AP → Mod): Capability-based routing (framework determines recipient)

4. **Thread Safety**
   - APPollingThread runs at 60fps
   - All IPC message queues are mutex-protected
   - Mods poll for messages or use callbacks

---

**Last Updated**: 2026-01-09