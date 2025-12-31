# APTest - Test Submodule for APFramework

A minimal test mod to verify APFramework functionality.

## Architecture

APTest is a **submodule** of APFramework, located at:
```
APFramework/
└── Mods/
    └── APTest/
        ├── ap_config.json  # Capability manifest
        ├── main.lua        # Optional initialization script
        └── README.md       # This file
```

This mod is automatically discovered by APFramework during initialization.

## Installation

APTest is included with APFramework. No separate installation needed.

If APFramework is installed at:
```
E:\SteamLibrary\steamapps\common\Palworld\Pal\Binaries\Win64\ue4ss\Mods\APFramework\
```

Then APTest should already be at:
```
E:\SteamLibrary\steamapps\common\Palworld\Pal\Binaries\Win64\ue4ss\Mods\APFramework\Mods\APTest\
```

## Configuration

Edit `ap_config.json` to configure connection settings:

```json
{
  "ap_enabled": true,
  "mod_info": {
    "id": "aptest",
    "name": "AP Test Mod",
    "version": "1.0.0"
  },
  "ap_connection": {
    "server": "localhost",
    "port": 38281,
    "slot_name": "Test Player 1",
    "password": ""
  }
}
```

## Testing

1. Start Archipelago server
2. Launch Palworld with APFramework installed
3. Check UE4SS.log for:
   ```
   [ModRegistry] Found AP config in: APTest
   [ModRegistry] Discovered 1 mods
   [APTest] Test mod initialized
   ```

## For Mod Developers

This is an example of how to create AP-compatible mods as APFramework submodules.

### Minimal Requirements

1. Create folder in `APFramework/Mods/<your_mod>/`
2. Add `ap_config.json` with:
   - `"ap_enabled": true`
   - `mod_info` section
   - `capabilities` section (items, locations, regions)
3. (Optional) Add `main.lua` for game integration hooks

APFramework will discover and register your mod automatically.
