# Changes Summary - IPC Branch Phase 3 Completion

**Date**: 2026-01-01
**Status**: Ready for Testing

## Overview

Completed Phase 3 of the IPC branch implementation, adding Lua C API bindings to replace FFI, fixing file paths, adding comprehensive logging, and preparing for testing.

## Major Changes

### 1. Removed FFI Bindings ✓

**Files Removed**:
- `src/framework_core/include/ffi_bindings.h`
- `src/framework_core/src/ffi_bindings.cpp`
- Updated `src/framework_core/CMakeLists.txt` to remove FFI references

**Reason**: UE4SS uses Lua 5.4 (not LuaJIT), so FFI is not available. Replaced with native Lua C API bindings.

### 2. Added Lua C API Bindings ✓

**Files Created**:
- `src/framework_core/include/lua_bindings.h` - Header for Lua module
- `src/framework_core/src/lua_bindings.cpp` - Implementation with logging

**Key Features**:
- Native Lua C API using userdata with metatables
- Automatic garbage collection via `__gc` metamethod
- Module exports `luaopen_APFrameworkCore()` for UE4SS to load
- All FrameworkCore methods bound to Lua
- Logging integrated into all key operations

**API**:
```lua
local APFrameworkCore = require("APFrameworkCore")

-- Initialize logger (must be first)
APFrameworkCore.init_logger("path/to/log.txt")

-- Create framework instance
local handle = APFrameworkCore.create("pipe_name")

-- Call methods
handle:start_ipc()
handle:discover_mods("directory")
handle:connect_ap(server, port, slot, password)
-- etc...
```

### 3. Added File Logging System ✓

**Files Created**:
- `src/framework_core/include/logger.h` - Thread-safe file logger
- `src/framework_core/src/logger.cpp` - Implementation

**Features**:
- Thread-safe logging to file
- Timestamped log entries (YYYY-MM-DD HH:MM:SS.mmm)
- Log levels: DEBUG, INFO, WARNING, ERROR
- Singleton pattern for global access
- Convenience macros: `LOG_INFO()`, `LOG_ERROR()`, etc.
- Fixed Windows.h ERROR macro conflict

**Log Location**: `ue4ss\Mods\APFramework\framework.log`

### 4. Updated Lua Framework Scripts ✓

**Files Updated**:
- `APFramework/Scripts/main.lua` - Complete rewrite with:
  - Correct file paths (`ue4ss\\Mods` instead of `Mods`)
  - Logger initialization before framework creation
  - Verbose logging at every lifecycle stage
  - Correct UE4SS hooks (`RegisterCustomEvent("Tick")` instead of invalid hooks)
  - Lifecycle state tracking and reporting
  - Detailed error messages and status updates

- `APFramework/Scripts/framework_wrapper.lua` - Updated to:
  - Use Lua C API (`require("APFrameworkCore")`)
  - Call `init_logger()` before creating framework
  - Simplified interface (no manual memory management)
  - Direct method calls on userdata objects

- `APFramework/Scripts/config.lua` - Added:
  - `registration_timeout` field (configurable, default 180s)
  - JSON parsing for timeout config

### 5. Fixed File Paths ✓

**All paths now use Windows-style backslashes**:
- Config: `ue4ss\\Mods\\APFramework\\framework_config.json`
- Log: `ue4ss\\Mods\\APFramework\\framework.log`
- Capabilities: `ue4ss\\Mods\\APFramework\\APCapabilities.json`
- Mod discovery: `ue4ss\\Mods\\`

### 6. Fixed UE4SS Hooks ✓

**Correct Hooks**:
```lua
-- CORRECT: Tick event (runs every frame)
RegisterCustomEvent("Tick", function(deltaTime)
    if lifecycle ~= "RUNNING" then
        lifecycle_count = lifecycle_count + 1
        if lifecycle_count % 60 == 0 then
            update_lifecycle()
        end
    end
end)

-- CORRECT: Shutdown (though UE4SS may not fire this in Palworld)
RegisterHook("/Script/Engine.GameInstance:ReceiveShutdown", function()
    shutdown()
end)
```

**Removed Invalid Hooks**:
- `/Script/Engine.PlayerController:ServerAcknowledgePossession` - Not reliable in single-player
- `/Script/Engine.PlayerController:PlayerTick` - Incorrect syntax

### 7. Configuration Enhancements ✓

**Files Created**:
- `APFramework/framework_config.json` - Sample configuration

**New Fields**:
```json
{
  "server": "archipelago.gg",
  "port": 38281,
  "slot_name": "Player1",
  "password": "",
  "autoconnect": false,
  "registration_timeout": 180
}
```

### 8. Documentation ✓

**Files Created**:
- `docs/BUILD.md` - Complete build instructions
  - Prerequisites
  - Build steps
  - Installation instructions
  - Troubleshooting guide

- `CHANGES.md` - This file

**Files Updated**:
- `docs/ARCHITECTURE.md` - Already comprehensive, no changes needed

### 9. Build System Updates ✓

**Files Modified**:
- `CMakeLists.txt` (root) - Added Lua subdirectory
- `src/framework_core/CMakeLists.txt` - Removed FFI, added logger, linked Lua
- `third_party/lua-5.4.7/CMakeLists.txt` - Build Lua as static library

**Build Output**:
- `build/bin/Release/APFrameworkCore.dll` - Main DLL with Lua statically linked
- `build/bin/Release/APClientLib.dll` - Client library for mods
- `build/lib/Release/lua.lib` - Lua static library

## Technical Details

### Lua C API Implementation

The bindings use proper Lua C API patterns:
- Userdata with metatables for object-oriented interface
- `__gc` metamethod for automatic cleanup
- `__index` metamethod for method dispatch
- Module table with `create()` and `init_logger()` functions

### Logger Implementation

- Singleton pattern with `Logger::instance()`
- Thread-safe with `std::mutex`
- Flush after every log write for crash safety
- ISO 8601 timestamps with milliseconds
- No external dependencies (pure C++17)

### Lifecycle State Machine

```
INIT -> START_IPC -> DISCOVER -> WAIT_REG -> CONNECT -> RUNNING
```

Each state transition is logged verbosely for debugging.

## Testing Checklist

Before deploying to game:

- [x] Clean build succeeds
- [ ] Copy DLL to game folder
- [ ] Copy Lua scripts to game folder
- [ ] Launch game and check UE4SS console
- [ ] Verify `framework.log` is created and populated
- [ ] Test mod discovery (if any AP mods present)
- [ ] Test AP connection (if autoconnect enabled)
- [ ] Monitor for errors in log file

## Next Steps

1. **Deploy to Game**:
   ```bash
   cp build/bin/Release/APFrameworkCore.dll E:/SteamLibrary/.../ue4ss/Mods/APFramework/
   cp -r APFramework/* E:/SteamLibrary/.../ue4ss/Mods/APFramework/
   ```

2. **Test Logging**:
   - Launch game
   - Check `ue4ss/Mods/APFramework/framework.log`
   - Verify log entries appear
   - Check UE4SS console for print statements

3. **Verify Lifecycle**:
   - Confirm lifecycle reaches RUNNING state
   - Check that IPC server starts
   - Verify mod discovery runs (even if no mods found)

4. **Test with AP Mod** (if available):
   - Place an AP-enabled mod in `ue4ss/Mods/`
   - Verify framework discovers it
   - Check registration completes
   - Test capabilities generation

## Known Issues

None currently. This is a fresh implementation ready for first testing.

## Breaking Changes

- **FFI completely removed** - Any code expecting FFI will break
- **File paths changed** - Now use `ue4ss\\Mods` prefix
- **Hooks changed** - Now use `RegisterCustomEvent("Tick")` instead of PlayerController hooks

## Files Modified Since Last Version

### Added:
- `src/framework_core/include/logger.h`
- `src/framework_core/src/logger.cpp`
- `src/framework_core/include/lua_bindings.h` (new version)
- `src/framework_core/src/lua_bindings.cpp` (new version)
- `docs/BUILD.md`
- `CHANGES.md`
- `APFramework/framework_config.json`
- `APFramework/enabled.txt`

### Modified:
- `APFramework/Scripts/main.lua` (complete rewrite)
- `APFramework/Scripts/framework_wrapper.lua` (complete rewrite)
- `APFramework/Scripts/config.lua` (added timeout field)
- `src/framework_core/CMakeLists.txt` (removed FFI, added logger)
- `CMakeLists.txt` (added Lua subdirectory)

### Removed:
- `src/framework_core/include/ffi_bindings.h`
- `src/framework_core/src/ffi_bindings.cpp`

## Build Verification

```bash
$ cmake --build build --config Release
...
APFrameworkCore.vcxproj -> C:\...\build\bin\Release\APFrameworkCore.dll
Building Custom Rule C:/Users/.../CMakeLists.txt
```

✓ Build succeeded with no errors
✓ All dependencies linked correctly
✓ Lua static library integrated

---

**Ready for testing!** Please review the changes, deploy to your test environment, and provide feedback on any issues encountered.
