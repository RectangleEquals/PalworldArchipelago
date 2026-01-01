# Connection Configuration Redesign

## Problem Statement

Currently, `ap_connection` is stored in each mod's `ap_config.json`:
- Multiple mods could specify conflicting connection info
- Redundant if all mods use the same connection
- Doesn't support APMenuMod UI configuration
- Not user-friendly

## Proposed Solution

Move connection configuration to **framework-level** `APFramework/config.json`.

### New File Structure

```
APFramework/
├── config.json                 # Framework configuration (NEW)
├── config.example.json         # Template (already exists)
├── Scripts/
│   └── ...
└── Mods/
    └── APTest/
        └── ap_config.json      # Capabilities only (MODIFIED)
```

### config.json Schema

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
    "slot_name": "Player1",
    "password": "",
    "auto_reconnect": true
  },
  "ui": {
    "show_notifications": true,
    "show_debug_overlay": false
  }
}
```

**Note**: `poll_interval_ms` is framework-level (not connection-level) since it affects polling performance globally.

### Mod config.json Schema (SIMPLIFIED)

```json
{
  "ap_enabled": true,
  "mod_info": {
    "id": "aptest",
    "name": "AP Test Mod",
    "version": "1.0.0",
    "author": "Test"
  },
  "capabilities": {
    "items": [...],
    "locations": [...],
    "regions": [...]
  }
  // NO ap_connection field!
}
```

## Implementation Steps

### 1. Create Default config.json

Update `APFramework/Scripts/config.example.json` to include `ap_connection` section.

### 2. Update ConfigManager

Modify `ConfigManager.lua` to:
- Load `APFramework/config.json` (not `APFramework/Scripts/config.json`)
- Parse `ap_connection` section
- Provide getter: `ConfigManager:GetConnectionConfig()`

### 3. Update ModRegistry

Modify `ModRegistry.lua` to:
- Remove `ap_connection` parsing from mod configs
- Only parse `capabilities` section

### 4. Update APFramework Connection Logic

Modify `APFramework/Scripts/main.lua` to:
- Read connection info from `ConfigManager:GetConnectionConfig()`
- Remove loop through mods looking for `ap_connection`

### 5. Migrate APTest Config

Update `APFramework/Mods/APTest/ap_config.json`:
- Remove `ap_connection` section
- Keep only `mod_info` and `capabilities`

### 6. Create config.json in Game Directory

For testing, create initial `APFramework/config.json` with working connection info.

## APMenuMod Integration

With this design, APMenuMod can:

### Read Current Config
```lua
local config = ConfigManager:GetConfig()
local connection = config.ap_connection

-- Display in UI
UI:SetServerField(connection.server)
UI:SetPortField(connection.port)
UI:SetSlotField(connection.slot_name)
```

### Update Config
```lua
function OnSaveButtonClick()
    local new_connection = {
        enabled = true,
        server = UI:GetServerField(),
        port = UI:GetPortField(),
        slot_name = UI:GetSlotField(),
        password = UI:GetPasswordField(),
        auto_reconnect = true,
        poll_interval_ms = 16
    }

    ConfigManager:SetConnectionConfig(new_connection)
    ConfigManager:Save()  -- Write to APFramework/config.json

    -- Optionally reconnect
    if APFrameworkCore:IsConnected() then
        APFrameworkCore:Disconnect()
    end
    APFrameworkCore:ConnectToServer(
        new_connection.server,
        new_connection.port,
        new_connection.slot_name,
        new_connection.password
    )
end
```

### Full Config Profiles (Future)

Could support multiple complete configuration profiles:

```json
{
  "config_profiles": [
    {
      "name": "Local Dev",
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
        "slot_name": "DevPlayer",
        "password": ""
      },
      "ui": {
        "show_notifications": true,
        "show_debug_overlay": true
      }
    },
    {
      "name": "Production",
      "framework": {
        "version": "1.0.0",
        "debug_mode": false,
        "auto_connect": true,
        "poll_interval_ms": 16
      },
      "ap_connection": {
        "enabled": true,
        "server": "archipelago.gg",
        "port": 38281,
        "slot_name": "Player1",
        "password": ""
      },
      "ui": {
        "show_notifications": true,
        "show_debug_overlay": false
      }
    }
  ],
  "active_profile": "Local Dev"
}
```

APMenuMod would provide:
- Profile dropdown
- Load/Save/Delete profile buttons
- Clone profile from current config
- Import/Export profiles

## Migration Path

**Not needed** - No public release yet, no existing users to migrate.

## Benefits Summary

✅ **Single source of truth** - One place to configure connection
✅ **User-friendly** - APMenuMod can provide UI for configuration
✅ **Flexible** - Supports profiles, auto-connect toggle, etc.
✅ **Cleaner** - Mods focus on capabilities, framework handles connection
✅ **Backward compatible** - Migration path for existing configs

## File Changes Required

### New/Modified Files
1. `APFramework/config.json` - CREATE (with connection info)
2. `APFramework/config.example.json` - MOVE from Scripts/ + UPDATE (add ap_connection)
3. `APFramework/Scripts/ConfigManager.lua` - UPDATE (change config path, parse ap_connection)
4. `APFramework/Scripts/main.lua` - UPDATE (read from ConfigManager)
5. `APFramework/Mods/APTest/ap_config.json` - UPDATE (remove ap_connection)

### Optional Enhancements
6. Add `ConfigManager:SetConnectionConfig(config)` method
7. Add `ConfigManager:Save()` method (write to disk)
8. Add migration logic for old configs

## Testing

1. Create `APFramework/config.json` with test connection
2. Remove `ap_connection` from `APTest/ap_config.json`
3. Launch game
4. Verify connection still works
5. Test reading config from APMenuMod (once implemented)

---

**Recommendation**: Implement this redesign **now**, before adding continuous polling. It will make APMenuMod integration much cleaner.