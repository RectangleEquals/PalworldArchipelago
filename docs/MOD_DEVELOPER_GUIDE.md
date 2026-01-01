# AP Mod Developer Guide

Guide for creating Archipelago-compatible mods as APFramework submodules.

## Architecture Overview

APFramework uses a **submodule architecture** where AP-compatible mods are placed inside the APFramework directory:

```
ue4ss/Mods/
└── APFramework/
    ├── Scripts/          # Framework core
    └── Mods/             # Your mods go here
        ├── YourMod/
        │   ├── ap_config.json
        │   ├── main.lua (optional)
        │   └── README.md
        └── AnotherMod/
```

**Why submodules?**
- Ensures APFramework initializes before your mod
- Allows proper UE4SS hook registration timing
- Simplifies discovery and communication
- Keeps all AP-related code together

## Quick Start

### 1. Create Your Mod Folder

```bash
cd ue4ss/Mods/APFramework/Mods/
mkdir YourMod
cd YourMod
```

### 2. Create `ap_config.json`

This file tells APFramework what your mod provides:

```json
{
  "ap_enabled": true,
  "mod_info": {
    "id": "yourmod",
    "name": "Your Mod Name",
    "version": "1.0.0",
    "author": "YourName",
    "description": "Brief description"
  },
  "capabilities": {
    "items": [
      {
        "id": "your_item_id",
        "name": "Your Item Name",
        "category": "tools"
      }
    ],
    "locations": [
      {
        "id": "your_location_id",
        "name": "Your Location Name",
        "region": "your_region"
      }
    ],
    "regions": [
      {
        "name": "your_region",
        "connects_to": []
      }
    ]
  },
  "ap_connection": {
    "server": "localhost",
    "port": 38281,
    "slot_name": "Player1",
    "password": ""
  }
}
```

### 3. (Optional) Create `main.lua`

If your mod needs to hook into game events:

```lua
--[[
    YourMod - Archipelago Integration
]]

print("[YourMod] Initializing...")

local YourMod = {}

function YourMod:Initialize()
    -- Register UE4SS hooks here
    -- Example: RegisterHook("/Script/Pal.PalCharacterBoss:OnDeath", function(self)
    --     print("[YourMod] Boss defeated!")
    --     -- Notify APFramework of location check
    -- end)

    print("[YourMod] Hooks registered")
end

YourMod:Initialize()

return YourMod
```

### 4. Test Your Mod

1. Launch Palworld
2. Check `ue4ss/UE4SS.log` for:
   ```
   [ModRegistry] Found AP config in: YourMod
   [ModRegistry] Discovered 1 mods
   [YourMod] Initializing...
   ```

## ap_config.json Reference

### Required Fields

```json
{
  "ap_enabled": true,  // REQUIRED - Must be true
  "mod_info": {
    "id": "string",       // REQUIRED - Unique snake_case ID
    "name": "string",     // REQUIRED - Display name
    "version": "X.Y.Z"    // REQUIRED - Semantic version
  },
  "capabilities": {      // REQUIRED - At least one of items/locations/regions
    "items": [...],
    "locations": [...],
    "regions": [...]
  }
}
```

### Optional Fields

```json
{
  "ap_connection": {
    "server": "localhost",
    "port": 38281,
    "slot_name": "Player1",
    "password": ""
  },
  "dependencies": {
    "requires": [
      {
        "mod_id": "other_mod",
        "min_version": "1.0.0"
      }
    ],
    "conflicts": ["incompatible_mod"]
  },
  "runtime_requirements": {
    "initialization_order": 100  // Higher = loads first
  }
}
```

## Item Definition

```json
{
  "id": "unique_item_id",           // REQUIRED - Unique identifier
  "name": "Display Name",           // REQUIRED - Shown in tracker
  "category": "tools",              // REQUIRED - Item category
  "description": "Item description",// Optional
  "progression": true,              // Optional - Is this progression?
  "useful": true,                   // Optional - Is this useful?
  "trap": false,                    // Optional - Is this a trap?
  "count": 1                        // Optional - Max count (for consumables)
}
```

## Location Definition

```json
{
  "id": "unique_location_id",       // REQUIRED
  "name": "Location Name",          // REQUIRED
  "region": "region_name",          // REQUIRED
  "description": "How to find",     // Optional
  "difficulty": "normal",           // Optional: easy/normal/hard
  "type": "boss"                    // Optional: boss/treasure/achievement
}
```

## Region Definition

```json
{
  "name": "region_name",            // REQUIRED
  "connects_to": [                  // REQUIRED (can be empty)
    "other_region",
    "another_region"
  ]
}
```

## Game Integration (main.lua)

### Basic Template

```lua
local YourMod = {}

function YourMod:Initialize()
    self:RegisterHooks()
    print("[YourMod] Initialized")
end

function YourMod:RegisterHooks()
    -- Example: Boss defeat
    RegisterHook("/Script/Pal.PalCharacterBoss:OnDeath", function(boss)
        local bossName = boss:GetDisplayName()
        self:OnBossDefeated(bossName)
    end)

    -- Example: Item pickup
    RegisterHook("/Script/Pal.PalPlayerCharacter:AddItem", function(player, item, count)
        self:OnItemReceived(item, count)
    end)
end

function YourMod:OnBossDefeated(bossName)
    -- TODO: Notify APFramework of location check
    print(string.format("[YourMod] Boss defeated: %s", bossName))
end

function YourMod:OnItemReceived(item, count)
    -- TODO: Handle AP item receipt
    print(string.format("[YourMod] Received item: %s x%d", item, count))
end

YourMod:Initialize()
return YourMod
```

### Finding UE4SS Hooks

Use the UE4SS live view / dumper to find available hooks:
1. Press `~` in-game to open UE4SS console
2. Use `DumpBPs` to dump all blueprint classes
3. Look for relevant classes in `ue4ss/Dumps/`

Common Palworld hooks:
- `/Script/Pal.PalCharacterBoss:OnDeath` - Boss defeated
- `/Script/Pal.PalPlayerCharacter:AddItem` - Item added to inventory
- `/Script/Pal.PalPlayerCharacter:OnLevelUp` - Player leveled up

## Communication with APFramework

### Sending Location Checks (TODO - Not Yet Implemented)

```lua
-- Future API (not yet available)
local APFramework = require("APFramework")
APFramework:SendLocationCheck("your_location_id")
```

### Receiving Items (TODO - Not Yet Implemented)

```lua
-- Future API (not yet available)
local APFramework = require("APFramework")
APFramework:RegisterItemHandler(function(item_id, count)
    -- Give item to player
end)
```

## Testing Checklist

- [ ] `ap_config.json` has valid JSON syntax
- [ ] `"ap_enabled": true` is present
- [ ] All required fields are filled in
- [ ] Mod ID is unique snake_case
- [ ] Version follows X.Y.Z format
- [ ] At least one item/location/region defined
- [ ] UE4SS.log shows mod discovered
- [ ] No errors in UE4SS.log

## Common Issues

### "Discovered 0 mods"
- Check `ap_config.json` is at `APFramework/Mods/YourMod/ap_config.json`
- Verify `"ap_enabled": true` exists
- Validate JSON syntax (use a JSON validator)

### "Invalid mod config: Missing mod_info"
- Ensure `mod_info` section exists with `id`, `name`, `version`

### "mod_info.id must be snake_case alphanumeric"
- Use only lowercase letters, numbers, and underscores
- Example: `yourmod`, `boss_tracker`, `item_mod_v2`

### Hooks not firing
- Check hook path is correct (case-sensitive)
- Verify hook is registered during initialization
- Use UE4SS live view to confirm object paths

## Example: Boss Tracker Mod

Complete example in `APFramework/Mods/APTest/`

## Support

- Check [APFramework/Mods/APTest/](../APFramework/Mods/APTest/) for working example
- See [ARCHITECTURE.md](../ARCHITECTURE.md) for system design
- Report issues at [GitHub Issues](https://github.com/your-repo/issues)
