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
```lua
-- APFrameworkMod/Scripts/main.lua
local APFramework = require("APFramework")

-- Initialize framework when mod loads
local function init_framework()
    print("[APFrameworkMod] Initializing Archipelago Framework...")

    -- Load framework config
    local config_path = "framework_config.json"

    -- Initialize APFrameworkCore
    local success, err = pcall(function()
        APFramework:init(config_path)
        APFramework:start()
    end)

    if not success then
        print("[APFrameworkMod] ERROR: " .. tostring(err))
        return
    end

    print("[APFrameworkMod] Framework initialized successfully!")
end

-- Hook into UE4SS lifecycle
RegisterInitGameStateHook(function()
    init_framework()
end)

-- Clean up on unload
RegisterUnrealEngineShutdownCallback(function()
    print("[APFrameworkMod] Shutting down framework...")
    local manager = APManager.instance()
    manager:shutdown()
end)
```

**Lua Error Handling**:
- All C++ exceptions caught and converted to Lua errors
- Lua pcall() used for safe execution
- Error messages logged via APLogger

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

**Last Updated**: 2026-01-09
**Status**: 🔴 Not Started
