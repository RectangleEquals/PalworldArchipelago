--[[
    APFramework UE4SS Entry Point

    This is the main entry point when loaded by UE4SS.
    UE4SS automatically loads Scripts/main.lua for each enabled mod.
]]

print("=== APFramework Loading ===")

-- Load core modules directly (they're all in the same Scripts folder)
print("[APFramework] Loading modules...")

local EventBus = require("EventBus")
print("[APFramework] - EventBus loaded")

local ConfigManager = require("ConfigManager")
print("[APFramework] - ConfigManager loaded")

local StateManager = require("StateManager")
print("[APFramework] - StateManager loaded")

local CapabilityManager = require("CapabilityManager")
print("[APFramework] - CapabilityManager loaded")

local ModRegistry = require("ModRegistry")
print("[APFramework] - ModRegistry loaded")

local APFrameworkCore = require("APFramework")
print("[APFramework] - APFramework core loaded")

print("[APFramework] All modules loaded successfully")

-- Initialize the framework
print("[APFramework] Initializing framework...")
local success = APFrameworkCore:Initialize()

if success then
    print("[APFramework] Framework initialized successfully!")

    -- Discover mods
    print("[APFramework] Discovering mods...")
    APFrameworkCore:DiscoverMods()

    -- Generate capability manifest
    print("[APFramework] Generating capability manifest...")
    local manifest = APFrameworkCore:GenerateCapabilityManifest()

    if manifest then
        print("[APFramework] Capability manifest generated successfully")

        -- Print summary
        local stats = ModRegistry:GetStatistics()
        print(string.format("[APFramework] Total: %d mods, %d locations, %d items",
              stats.total_mods, stats.location_count, stats.item_count))

        -- Connect to AP server if configured
        local connection_config = ConfigManager:GetConnectionConfig()

        if connection_config and connection_config.enabled then
            print(string.format("[APFramework] Found AP connection config: %s:%d",
                connection_config.server, connection_config.port))
            print(string.format("[APFramework] Slot: %s", connection_config.slot_name))

            -- Connect using APClient
            local connected = APFrameworkCore:ConnectToServer(
                connection_config.server,
                connection_config.port,
                connection_config.slot_name,
                connection_config.password or ""
            )

            if connected then
                print("[APFramework] Successfully connected to AP server")

                -- Start continuous polling loop in main thread (blocking)
                local framework_config = ConfigManager:GetConfig().framework
                local poll_interval_ms = (framework_config and framework_config.poll_interval_ms) or 16
                local poll_interval_sec = poll_interval_ms / 1000.0
                print(string.format("[APFramework] Starting continuous polling (interval: %dms)", poll_interval_ms))

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

                        -- Execute frame callbacks for submods
                        if EventBus then
                            EventBus:ExecuteFrameCallbacks()
                        end

                        start_time = current_time -- Reset timer
                    end
                end

                print("[APFramework] Continuous polling started")
            else
                print("[APFramework] Warning: Failed to connect to AP server")
            end
        else
            print("[APFramework] No AP connection configured or connection disabled")
        end
    else
        print("[APFramework] Warning: Failed to generate capability manifest")
    end

    print("[APFramework] Ready for use!")
else
    print("[APFramework] ERROR: Failed to initialize framework")
end

print("=== APFramework Loading Complete ===")

-- Return the framework for UE4SS
return APFrameworkCore
