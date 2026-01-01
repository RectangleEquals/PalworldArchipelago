# Phase 4 Implementation Plan

## Overview

Implement two critical features for Phase 4:
1. **Continuous Polling** - Using `LoopAsync()` to keep connection alive
2. **Connection Config Redesign** - Move `ap_connection` to framework-level config

## Priority: Connection Config Redesign FIRST

**Why first?**
- APMenuMod depends on centralized config
- Cleaner architecture before adding complexity
- Easier to test polling with proper config structure
- Only affects a few files

## Step 1: Connection Config Redesign

### 1.1 Create APFramework/config.json

```json
{
  "framework": {
    "version": "1.0.0",
    "debug_mode": true,
    "auto_connect": true,
    "poll_interval_ms": 16
  },
  "ap_connection": {
    "enabled": true,
    "server": "localhost",
    "port": 38281,
    "slot_name": "TP1",
    "password": "",
    "auto_reconnect": true
  },
  "ui": {
    "show_notifications": true,
    "show_debug_overlay": false
  }
}
```

**Location**: `C:\Users\micha\Desktop\AP\CC\APFramework\config.json`

**Note**: `poll_interval_ms` is framework-level to allow users to adjust polling performance globally. 16ms ≈ 60fps, increase if performance issues occur.

### 1.2 Update ConfigManager.lua

**Changes needed**:
```lua
-- Change config path to framework root
local CONFIG_FILE = "APFramework/config.json"  -- Was: APFramework/Scripts/config.json

-- Add getter for connection config
function ConfigManager:GetConnectionConfig()
    if not self.config or not self.config.ap_connection then
        return nil
    end
    return self.config.ap_connection
end

-- Add setter for connection config (for APMenuMod)
function ConfigManager:SetConnectionConfig(connection_config)
    if not self.config then
        self.config = {}
    end
    self.config.ap_connection = connection_config
end

-- Add save method
function ConfigManager:Save()
    local json = require("lib.lunajson")
    local file = io.open(CONFIG_FILE, "w")
    if file then
        file:write(json.encode(self.config))
        file:close()
        return true
    end
    return false
end
```

### 1.3 Update main.lua Connection Logic

**Replace** (lines 56-80):
```lua
-- OLD: Loop through mods looking for ap_connection
local mods = ModRegistry:GetMods()
for _, mod in ipairs(mods) do
    if mod.ap_connection then
        -- ... connect
    end
end
```

**With**:
```lua
-- NEW: Read connection from ConfigManager
local connection_config = ConfigManager:GetConnectionConfig()

if connection_config and connection_config.enabled then
    print(string.format("[APFramework] Found AP connection config: %s:%d",
        connection_config.server, connection_config.port))
    print(string.format("[APFramework] Slot: %s", connection_config.slot_name))

    local connected = APFrameworkCore:ConnectToServer(
        connection_config.server,
        connection_config.port,
        connection_config.slot_name,
        connection_config.password or ""
    )

    if connected then
        print("[APFramework] Successfully connected to AP server")
    else
        print("[APFramework] Warning: Failed to connect to AP server")
    end
else
    print("[APFramework] No AP connection configured or connection disabled")
end
```

### 1.4 Update APTest/ap_config.json

**Remove** `ap_connection` section:
```json
{
  "ap_enabled": true,
  "mod_info": {
    "id": "aptest",
    "name": "AP Test Mod",
    "version": "1.0.0",
    "author": "Test",
    "description": "Minimal test mod to verify Archipelago connection"
  },
  "capabilities": {
    "items": [
      {
        "id": "test_item",
        "name": "Test Item",
        "category": "test"
      }
    ],
    "locations": [
      {
        "id": "test_location",
        "name": "Test Location",
        "region": "test_region"
      }
    ],
    "regions": [
      {
        "name": "test_region",
        "connects_to": []
      }
    ]
  }
}
```

### 1.5 Update ModRegistry.lua

**Optional**: Remove `ap_connection` parsing if it exists. Since we're not using it anymore, this is just cleanup.

### 1.6 Test Config Redesign

1. Create `config.json` as above
2. Update files as specified
3. Copy to game directory
4. Launch Palworld
5. Verify connection still works
6. Check logs show "Found AP connection config" not "Found AP connection info"

## Step 2: Continuous Polling

### 2.1 Add LoopAsync Polling to main.lua

**Add after successful connection** (after line ~73):

```lua
if connected then
    print("[APFramework] Successfully connected to AP server")

    -- Start continuous polling loop
    local framework_config = ConfigManager:GetConfig().framework
    local poll_interval = (framework_config and framework_config.poll_interval_ms) or 16
    print(string.format("[APFramework] Starting continuous polling (interval: %dms)", poll_interval))

    LoopAsync(poll_interval, function()
        -- Only poll if we have APClient
        if APFrameworkCore and APFrameworkCore.GetAPClient then
            local apclient = APFrameworkCore:GetAPClient()
            if apclient then
                apclient:Poll()
            end
        end

        return false -- Loop forever
    end)

    print("[APFramework] Continuous polling started")
else
    print("[APFramework] Warning: Failed to connect to AP server")
end
```

### 2.2 Add GetAPClient method to APFramework.lua

**Add to APFramework.lua**:
```lua
-- Expose APClient for continuous polling
function APFramework:GetAPClient()
    return APClient
end
```

### 2.3 Thread Safety Considerations

**CRITICAL - UE4SS Threading Model**:
- `LoopAsync` runs on **ASYNC THREAD** (NOT game thread!)
- UE4 operations (inventory, spawning, etc.) MUST run on game thread
- **ALWAYS use `ExecuteInGameThread()`** for any UE4 calls

**Correct Implementation**:
```lua
LoopAsync(poll_interval, function()
    -- This runs on async thread
    if APFrameworkCore and APFrameworkCore.GetAPClient then
        local apclient = APFrameworkCore:GetAPClient()
        if apclient then
            -- poll() is thread-safe, can call from async thread
            apclient:Poll()
        end
    end

    return false
end)
```

**Event Handler Thread Safety**:
- Handlers fire on **same thread as poll()** (async thread)
- Any UE4 operations in handlers MUST use `ExecuteInGameThread()`:

```lua
self.client:set_items_received_handler(function(items)
    -- This handler runs on ASYNC thread
    print("[APClient] Received items") -- Logging is safe

    -- UE4 operations MUST use ExecuteInGameThread
    ExecuteInGameThread(function()
        -- NOW we're on game thread, safe for UE4 calls
        for _, item in ipairs(items) do
            GrantItemToPlayer(item)  -- UE4 call, needs game thread
        end
    end)
end)
```

### 2.4 Performance Monitoring

Add optional debug logging:
```lua
local poll_count = 0
local last_log_time = os.clock()

LoopAsync(poll_interval, function()
    if APFrameworkCore and APFrameworkCore.GetAPClient then
        local apclient = APFrameworkCore:GetAPClient()
        if apclient then
            apclient:Poll()

            -- Debug: Log poll rate every 5 seconds
            poll_count = poll_count + 1
            local now = os.clock()
            if now - last_log_time >= 5.0 then
                local rate = poll_count / (now - last_log_time)
                print(string.format("[APFramework] Poll rate: %.1f Hz", rate))
                poll_count = 0
                last_log_time = now
            end
        end
    end

    return false
end)
```

### 2.5 Test Continuous Polling

1. Implement changes
2. Copy to game directory
3. Launch Palworld
4. Connect to AP server
5. Wait >10 seconds (previous polling duration)
6. Use AP web interface to send an item
7. Check UE4SS.log for `items_received` handler firing
8. Verify item received after startup window

## Step 3: Test Item Receiving

### 3.1 Send Test Item via AP

**Using AP web interface or Python**:
```python
# If server has web interface
# Navigate to localhost:38281
# Click "Send Item" to TP1
# Select any item

# OR use Python:
from CommonClient import CommonContext, server_loop
# ... send item to slot
```

### 3.2 Verify Handler Fires

Check UE4SS.log for:
```
[APClient] HANDLER FIRED: items_received
[APClient] Item count: 1
```

### 3.3 Implement Basic Item Logging

In `APClient.lua`, update `set_items_received_handler`:
```lua
self.client:set_items_received_handler(function(items)
    print(string.format("[APClient] Received %d items", #items))

    for i, item in ipairs(items) do
        print(string.format("[APClient] Item %d: {item=%d, location=%d, player=%d, flags=%d, index=%s}",
            i, item.item, item.location, item.player, item.flags, tostring(item.index)))

        -- TODO: Grant item to player
    end

    if self.callbacks.onItemsReceived then
        self.callbacks.onItemsReceived(items)
    end
end)
```

## Step 4: Implement State Persistence

### 4.1 Extend StateManager Schema

Add to `StateManager.lua`:
```lua
function StateManager:Initialize(save_file)
    self.save_file = save_file

    -- Default state structure
    self.state = {
        seed_loaded = false,
        seed_metadata = nil,
        ap_state = {
            last_received_index = -1,  -- NEW
            checked_locations = {},     -- NEW
            connection_info = {         -- NEW
                server = nil,
                port = nil,
                slot_name = nil
            }
        }
    }

    self:Load()
end
```

### 4.2 Add AP State Methods

```lua
function StateManager:GetLastReceivedIndex()
    return self.state.ap_state.last_received_index
end

function StateManager:SetLastReceivedIndex(index)
    self.state.ap_state.last_received_index = index
    self:Save()
end

function StateManager:AddCheckedLocation(location_id)
    table.insert(self.state.ap_state.checked_locations, location_id)
    self:Save()
end

function StateManager:IsLocationChecked(location_id)
    for _, id in ipairs(self.state.ap_state.checked_locations) do
        if id == location_id then
            return true
        end
    end
    return false
end
```

### 4.3 Use State in Item Handler

```lua
self.client:set_items_received_handler(function(items)
    local last_index = StateManager:GetLastReceivedIndex()

    for _, item in ipairs(items) do
        if item.index and item.index > last_index then
            print(string.format("[APClient] NEW item received: %d", item.item))

            -- Grant item to player
            -- TODO: Implement item granting

            -- Update state
            StateManager:SetLastReceivedIndex(item.index)
        end
    end
end)
```

## Step 5: End-to-End Test

1. Start game with all changes
2. Verify connection
3. Verify continuous polling (check logs after >10 seconds)
4. Send test item via AP interface
5. Verify item received and logged
6. Verify state saved (`APSaveState.json` updated)
7. Restart game
8. Verify state loaded (no duplicate item grants)

## Testing Checklist

- [ ] Config redesign: Connection from `config.json` works
- [ ] Config redesign: APTest has no `ap_connection` field
- [ ] Continuous polling: Connection stays alive >10 seconds
- [ ] Continuous polling: No performance issues
- [ ] Item receiving: Handler fires when item sent
- [ ] Item receiving: Correct item data logged
- [ ] State persistence: `last_received_index` saved
- [ ] State persistence: No duplicate items after restart
- [ ] Integration: Full flow works end-to-end

## Next Steps After This

1. **Item Granting** - Actually spawn items in player inventory
2. **Location Checking** - Hook game events and send checks
3. **APMenuMod** - UI for connection configuration
4. **Restore AP World Logic** - Full progression rules

---

**Estimated Time**: 2-3 hours of focused work
**Risk Level**: Low - all changes are additive and well-tested patterns