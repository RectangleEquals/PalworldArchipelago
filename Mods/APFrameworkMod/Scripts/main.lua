-- APFrameworkMod/Scripts/main.lua
-- Entry point for the Archipelago Framework UE4SS mod
--
-- CRITICAL NOTES:
-- - RegisterInitGameStateHook DOES NOT EXIST (hallucinated API)
-- - RegisterUnrealEngineShutdownCallback DOES NOT EXIST (hallucinated API)
-- - Use RegisterCustomEvent("Tick") instead - most reliable across all games
-- - There is NO reliable shutdown hook in UE4SS!
-- - Framework MUST be memory-safe even if shutdown() is never called

local APFramework = require("APFramework")

-- Initialization state
local current_time = os.clock()
local last_time = current_time
local is_initialized = false
local init_attempted = false

-- Initialize framework once on first Tick
-- Uses Tick event because it's nearly guaranteed to work across all games
RegisterCustomEvent("Tick", function()
    -- Operations here run in the game thread (likely within a blueprint)
    -- Keep operations minimal to avoid blocking the game thread
    current_time = os.clock()
    local delta_time = (current_time - last_time)

    -- Initialize framework once on first tick
    if not is_initialized and not init_attempted then
        init_attempted = true
        print("[APFrameworkMod] Initializing Archipelago Framework...")

        local success, err = pcall(function()
            -- Initialize framework
            local init_ok, init_err = APFramework.init("framework_config.json")
            if not init_ok then
                error("init failed: " .. tostring(init_err))
            end

            -- Start framework (state machine + IPC server)
            local start_ok, start_err = APFramework.start()
            if not start_ok then
                error("start failed: " .. tostring(start_err))
            end
        end)

        if success then
            print("[APFrameworkMod] Framework initialized successfully!")
            print("[APFrameworkMod] Current phase: " .. APFramework.get_phase_string())
            is_initialized = true
        else
            print("[APFrameworkMod] ERROR: " .. tostring(err))
            -- Reset init_attempted to retry on next tick
            init_attempted = false
        end

        last_time = current_time
        return
    end

    -- Optional: Periodic operations after initialization (once per second)
    if is_initialized and delta_time >= 1.0 then
        last_time = current_time

        -- Periodic health checks, statistics, etc. can go here
        -- Example: Log current phase periodically
        -- local phase = APFramework.get_phase_string()
        -- print("[APFrameworkMod] Status: " .. phase)
    end
end)

-- NOTE: There is NO reliable shutdown hook in UE4SS!
-- APManager::shutdown() is for CONVENIENCE ONLY and may never be called.
-- The framework MUST use smart pointers and RAII to ensure no memory leaks
-- even if shutdown() is never called (game crash, UE4SS termination, etc.)
--
-- DO NOT attempt to call APFramework.shutdown() here - there's no hook for it!