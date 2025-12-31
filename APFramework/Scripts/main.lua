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

        -- Connect to AP server if mods have connection info
        local mods = ModRegistry:GetMods()
        for _, mod in ipairs(mods) do
            if mod.ap_connection then
                print(string.format("[APFramework] Found AP connection info: %s:%d",
                    mod.ap_connection.server, mod.ap_connection.port))
                print(string.format("[APFramework] Slot: %s", mod.ap_connection.slot_name))

                -- Connect using APClient
                local connected = APFrameworkCore:ConnectToServer(
                    mod.ap_connection.server,
                    mod.ap_connection.port,
                    mod.ap_connection.slot_name,
                    mod.ap_connection.password
                )

                if connected then
                    print("[APFramework] Successfully connected to AP server")
                else
                    print("[APFramework] Warning: Failed to connect to AP server")
                end

                break -- Only connect once
            end
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
