# Phase 08: Lua Bindings & APFrameworkMod

**Status**: 🔴 Not Started

---

## Overview

Create Lua bindings for APFrameworkCore and implement APFrameworkMod (the priority client Lua mod that initializes the framework).

**Goals**:
- sol2 bindings for APFrameworkCore
- APFramework.lua high-level Lua wrapper
- main.lua UE4SS entry point
- Lua error handling and logging
- Example Lua scripts

---

## Prerequisites

- ✅ Phase 07 complete (APManager and all core components)
- ✅ sol2 library available
- ✅ UE4SS runtime for testing

---

## Components

### 1. sol2 C++ Bindings
### 2. APFramework.lua Wrapper
### 3. main.lua Entry Point
### 4. Lua Error Handling

---

## Key Implementation Points

**sol2 Bindings** (`lua_bindings.cpp`):
```cpp
void register_apframework_bindings(sol::state& lua) {
    // Register APManager
    lua.new_usertype<APManager>("APManager",
        "init", &APManager::init,
        "start", &APManager::start,
        "shutdown", &APManager::shutdown,
        "get_current_phase", &APManager::get_current_phase,
        "handle_command", &APManager::handle_command
    );

    // Register enums
    lua.new_enum<LifecyclePhase>("LifecyclePhase",
        {{"UNINITIALIZED", LifecyclePhase::UNINITIALIZED},
         {"RUNNING", LifecyclePhase::RUNNING},
         // ... all states
        });

    // Register logger
    lua.new_usertype<APLogger>("APLogger",
        "info", &APLogger::info,
        "warn", &APLogger::warn,
        "error", &APLogger::error
    );
}
```

**APFramework.lua Wrapper**:
```lua
local APFramework = {}

function APFramework:init(config_path)
    local manager = APManager.instance()
    local result = manager:init(config_path)
    if not result.success then
        error("Failed to initialize APFramework: " .. result.error_message)
    end
    return manager
end

function APFramework:start()
    local manager = APManager.instance()
    manager:start()
end

function APFramework:send_command(cmd, data)
    local manager = APManager.instance()
    return manager:handle_command(cmd, data or {})
end

function APFramework:get_phase()
    local manager = APManager.instance()
    return manager:get_current_phase()
end

return APFramework
```

**main.lua Entry Point**:

⚠️ **CRITICAL UE4SS API NOTES**:
- `RegisterInitGameStateHook` - **DOES NOT EXIST** (hallucinated)
- `RegisterUnrealEngineShutdownCallback` - **DOES NOT EXIST** (hallucinated)
- Use `RegisterCustomEvent("Tick", ...)` instead - nearly guaranteed to work across all games

See [ARCHITECTURE.md - UE4SS Lua Integration](../../ARCHITECTURE.md#ue4ss-lua-integration) for full details.

```lua
-- APFrameworkMod/Scripts/main.lua
local current_time = os.clock()
local last_time = current_time
local is_initialized = false

-- IMPORTANT: Use RegisterCustomEvent("Tick") - it's the most reliable initialization method
-- Game state hooks may not fire in all games or may fire multiple times

RegisterCustomEvent("Tick", function()
    -- Operations here run in the game thread (likely within a blueprint)
    -- Keep operations minimal to avoid blocking the game thread
    current_time = os.clock()
    local delta_time = (current_time - last_time)

    -- Initialize framework once on first tick
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
            -- Will retry on next tick
        end

        last_time = current_time
        return
    end

    -- Optional: Periodic operations after initialization (once per second)
    if delta_time >= 1.0 then
        last_time = current_time
        -- Periodic health checks, statistics, etc. can go here
    end
end)

-- NOTE: There is NO reliable shutdown hook in UE4SS!
-- APManager::shutdown() is for CONVENIENCE ONLY and may never be called.
-- The framework MUST use smart pointers and RAII to ensure no memory leaks
-- even if shutdown() is never called (game crash, UE4SS termination, etc.)
```

**Lua Error Handling**:
- All C++ exceptions caught and converted to Lua errors
- Lua pcall() used for safe execution
- Error messages logged via APLogger

**Lifecycle Management Requirements**:

⚠️ **CRITICAL**: The framework MUST be safe even if `shutdown()` is never called!

**Why This Matters**:
- UE4SS has NO reliable shutdown hook
- Shutdown order between mods is undefined
- Game crashes bypass all cleanup code
- Objects may be destroyed in arbitrary order

**Design Requirements**:
1. ✅ **Smart Pointers Everywhere**: Use `std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr` for all heap allocations
2. ✅ **RAII for All Resources**: File handles, sockets, threads must clean up in destructors
3. ✅ **Thread Safety**: All threads must be joinable in destructors with timeouts
4. ✅ **No Dangling Pointers**: Components must not hold raw pointers to potentially destroyed objects
5. ✅ **Timeout-Based Cleanup**: Use timeouts to prevent indefinite waits during destruction
6. ✅ **Connection Detection**: IPC connections must detect disconnection and clean up automatically

**Implementation Checklist** (verify during Phase08):
- [ ] APPollingThread destructor joins thread with 2-second timeout
- [ ] APIPCServer destructor closes all client connections gracefully
- [ ] APClient destructor disconnects WebSocket with timeout
- [ ] APLogger destructor flushes and closes file handle
- [ ] All component destructors are exception-safe
- [ ] No memory leaks detected by Valgrind/ASan (future testing)

See [ARCHITECTURE.md - Lifecycle Management & Memory Safety](../../ARCHITECTURE.md#ue4ss-lua-integration) for complete details and code examples.

---

## Testing

- Test Lua bindings work correctly
- Test APFramework.lua wrapper
- Test main.lua initialization in UE4SS
- Test error handling for invalid calls

---

## Acceptance Criteria

- ✅ sol2 bindings for all essential APFrameworkCore classes
- ✅ APFramework.lua provides high-level Lua API
- ✅ main.lua initializes framework on game start
- ✅ Lua error handling working
- ✅ Framework runs successfully in UE4SS environment
- ✅ Example scripts documented

---

## Next Phase

[Phase 09: APClientLib Implementation](Phase09_ClientLibrary.md)

---

## Notes

- ⚠️ **CRITICAL**: `RegisterInitGameStateHook` and `RegisterUnrealEngineShutdownCallback` are hallucinated APIs that DO NOT EXIST
- Use `RegisterCustomEvent("Tick", ...)` for initialization - most reliable across all games
- Framework MUST use smart pointers and RAII - shutdown() may never be called
- See ARCHITECTURE.md for complete UE4SS Lua Integration guidelines and Lifecycle Management requirements

---

**Last Updated**: 2026-01-10
**Status**: 🔴 Not Started
