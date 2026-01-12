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
local APClient = require("APClient")

-- Initialization state
local current_time = os.clock()
local last_time = current_time
local framework_initialized = false
local client_initialized = false
local client_registered = false
local init_attempted = false
local client = nil

-- Initialize framework once on first Tick
-- Uses Tick event because it's nearly guaranteed to work across all games
RegisterCustomEvent("Tick", function()
    -- Operations here run in the game thread (likely within a blueprint)
    -- Keep operations minimal to avoid blocking the game thread
    current_time = os.clock()
    local delta_time = (current_time - last_time)

    -- Initialize framework once on first tick
    if not framework_initialized and not init_attempted then
        init_attempted = true
        print("[APFrameworkMod] Initializing Archipelago Framework...\n")

        local success, err = pcall(function()
            -- Initialize framework
            local init_ok, init_err = APFramework.init("..\\framework_config.json")
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
            print("[APFrameworkMod] Framework initialized successfully!\n")
            print("[APFrameworkMod] Current phase: " .. APFramework.get_phase_string() .. "\n")
            framework_initialized = true
        else
            print("[APFrameworkMod] ERROR: " .. tostring(err) .. "\n")
            -- Reset init_attempted to retry on next tick
            init_attempted = false
        end

        last_time = current_time
        return  -- Exit early to yield back to UE4SS
    end

    -- Initialize APClient wrapper after framework starts
    if framework_initialized and not client_initialized then
        print("[APFrameworkMod] Initializing priority client...\n")

        local success, err = pcall(function()
            -- Create APClient instance
            client = APClient:new("archipelago.palworld.framework")

            -- Connect to framework IPC
            local init_ok, init_err = client:init()
            if not init_ok then
                error("client init failed: " .. tostring(init_err))
            end

            -- Register as priority client (no capabilities needed for priority clients)
            local reg_ok, reg_err = client:register({}, true)  -- true = is_priority
            if not reg_ok then
                error("registration failed: " .. tostring(reg_err))
            end
        end)

        if success then
            print("[APFrameworkMod] Priority client registered successfully!\n")
            client_initialized = true
            client_registered = true
        else
            print("[APFrameworkMod] Priority client ERROR: " .. tostring(err) .. "\n")
        end

        last_time = current_time
        return  -- Exit early to yield back to UE4SS
    end

    -- Optional: Periodic operations after initialization (once per second)
    if framework_initialized and delta_time >= 1.0 then
        last_time = current_time
        print("[APFrameworkMod] Tick handler executing periodic tasks...\n")

        -- Only poll for IPC messages after client is fully registered
        -- This prevents blocking the UE4SS thread during early framework phases
        if client_registered and client and client:is_connected() then
            print("[APFrameworkMod] About to poll...\n")
            client:poll()
            print("[APFrameworkMod] Poll returned\n")
        end

        -- Periodic health checks, statistics, etc. can go here
        -- Example: Log current phase periodically
        -- local phase = APFramework.get_phase_string()
        -- print("[APFrameworkMod] Status: " .. phase .. "\n")
    end

    -- Had to comment this out due to log spam occurring during every tick
    -- print("[APFrameworkMod] Tick handler returning\n")
end)

-- NOTE: There is NO reliable shutdown hook in UE4SS!
-- APManager::shutdown() is for CONVENIENCE ONLY and may never be called.
-- The framework MUST use smart pointers and RAII to ensure no memory leaks
-- even if shutdown() is never called (game crash, UE4SS termination, etc.)
--
-- DO NOT attempt to call APFramework.shutdown() here - there's no hook for it!