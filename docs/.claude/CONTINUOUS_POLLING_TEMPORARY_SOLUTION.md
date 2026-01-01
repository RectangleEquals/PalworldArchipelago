# Continuous Polling - Temporary Blocking Loop Solution

**Date**: December 31, 2024
**Status**: ⚠️ Temporary Implementation

## Problem

Lua-apclientpp requires continuous polling in the **same Lua state** where the APClient was created. All threading approaches failed:

1. **LoopAsync**: Creates new Lua state → "Lua state changed" error
2. **ExecuteWithDelay**: Runs in different Lua state → "Lua state changed" error
3. **ExecuteInGameThread** inside LoopAsync: Still different Lua state → Same error

## Temporary Solution: Blocking While Loop

```lua
-- Blocking polling loop in main Lua state
local processing = true
local start_time = os.clock()

while processing do
    local current_time = os.clock()
    local elapsed_time = current_time - start_time

    if elapsed_time >= poll_interval_sec then
        -- Poll the client
        if APFrameworkCore and APFrameworkCore.GetAPClient then
            local apclient = APFrameworkCore:GetAPClient()
            if apclient then
                local success, err = pcall(function()
                    apclient:Poll()
                end)
                if not success then
                    print("[APFramework] Poll error: " .. tostring(err))
                    processing = false -- Stop on error
                end
            end
        end

        start_time = current_time -- Reset timer
    end
end
```

## Why This Works

- Runs in the **same Lua state** as APClient creation
- No threading = No "Lua state changed" errors
- APClient connection stays alive
- Handlers fire correctly when items received

## Critical Limitations

### 1. **Blocks All Subsequent Mods**

UE4SS loads mods sequentially on a shared thread. The blocking loop prevents:
- Any mods loaded **after** APFramework from initializing
- Those mods from executing their code

**Example**: If load order is `Ticker → APFramework → OtherMod`:
- Ticker loads and runs
- APFramework loads and **blocks**
- OtherMod **never loads**

### 2. **No Yielding to UE4**

The while loop never yields control back to the game engine. This means:
- No UE4 operations can run during polling
- Event handlers **cannot use UE4 functions** yet

### 3. **Not Production-Ready**

This is a **temporary testing solution only**. It allows us to:
- ✅ Test item receiving
- ✅ Verify handlers fire
- ✅ Validate AP protocol communication
- ❌ Grant items to player (requires UE4 calls)
- ❌ Hook game events (requires UE4 thread)
- ❌ Use with other mods (blocks them)

## Submod Support: onFrame Callback

✅ **IMPLEMENTED**: Submods can now register frame callbacks to execute code during each polling iteration.

### Implementation

Frame callbacks are managed by `EventBus` and exposed via `APFramework.RegisterFrameCallback()`:

**EventBus.lua**:
- `EventBus:RegisterFrameCallback(mod_id, callback)` - Register a callback
- `EventBus:UnregisterFrameCallback(mod_id)` - Remove a callback
- `EventBus:ExecuteFrameCallbacks()` - Execute all callbacks (called by polling loop)

**APFramework.lua** (Public API):
```lua
APFramework.RegisterFrameCallback(mod_id, callback)
APFramework.UnregisterFrameCallback(mod_id)
```

**main.lua** (Polling loop integration):
```lua
while processing do
    -- ... polling logic ...

    -- Execute frame callbacks for submods
    if EventBus then
        EventBus:ExecuteFrameCallbacks()
    end
end
```

### Usage for Submods

Submods can register callbacks during their initialization:

```lua
-- In submod's main.lua
APFramework.RegisterFrameCallback("my_mod", function()
    -- Runs every poll iteration (~16ms by default)
    -- Can access same Lua state as APClient
    -- Can read/write mod state
    -- Still can't use UE4 functions (no game thread access)
end)
```

**Callback Capabilities**:
- ✅ Access same Lua state as APClient
- ✅ Read/write mod state in Lua
- ✅ Log messages
- ✅ Perform Lua calculations
- ❌ Cannot use UE4 functions (no game thread)
- ❌ Cannot grant items to player yet
- ❌ Cannot check locations via UE4 hooks yet

**Error Handling**:
- Callbacks are wrapped in `pcall()` for error isolation
- One callback error won't crash other callbacks
- Errors are logged with mod_id for debugging

## Future Permanent Solution

We need to find a way to:
1. **Poll from a UE4 game tick hook** (not mod loading thread)
2. **Run in same Lua state** as APClient creation
3. **Not block other mods** during initialization

Possible approaches:
- Hook into UE4 tick events (`RegisterHook` on PlayerController tick)
- Use UE4SS `RegisterConsoleCommand` + timer
- Investigate UE4SS event system for non-blocking loops
- Contact UE4SS maintainers for guidance

## Testing Plan

With this temporary solution, we can test:

1. **Item Receiving**:
   - Send item from AP web interface
   - Verify handler fires
   - Log item data

2. **Connection Stability**:
   - Keep game running for extended period
   - Verify no disconnects
   - Monitor polling performance

3. **Handler Thread Safety**:
   - Verify handlers run in correct Lua state
   - Test logging from handlers
   - Prepare for future ExecuteInGameThread usage

## Migration Path

When we find the permanent solution:

1. Move polling to proper game thread hook
2. Remove blocking while loop
3. Update submod callback system
4. Test with other UE4SS mods installed
5. Implement UE4 operations (item granting, location checking)

---

**Current Status**: Working for basic testing, not production-ready
**Next Phase**: Test item receiving, then research permanent solution