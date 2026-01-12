-- ExampleClientMod/Scripts/main.lua
-- Example AP-enabled mod demonstrating APClientLib usage
--
-- This mod shows how to:
-- - Connect to the APFramework
-- - Register callbacks for received items and location checks
-- - Send location checks when game events occur
-- - Handle AP server messages

local APClient = require("APClient")

-- Mod state
local client = nil
local is_initialized = false
local is_registered = false

-- Initialize APClient and connect to framework
local function init_ap_client()
    print("[ExampleMod] Initializing AP client...")

    -- Create client instance
    client = APClient:new("example.palworld.testmod")

    -- Connect to framework
    local success, err = client:init()
    if not success then
        print("[ExampleMod] Failed to connect to APFramework: " .. tostring(err))
        return false
    end

    print("[ExampleMod] Connected to APFramework successfully!")

    -- Register callbacks
    client:on_received_items(function(item_data)
        print("[ExampleMod] Received items from AP server!")
        -- TODO: Add item to player inventory via UE4SS
        -- Example: local item_id = item_data.item_id
        --          local item_name = item_data.item_name
        --          GiveItemToPlayer(item_id)
    end)

    client:on_location_info(function(location_data)
        print("[ExampleMod] Received location info update")
        -- TODO: Update UI or game state based on location status
    end)

    client:on_lifecycle_change(function(msg)
        local phase = msg.data.phase or "UNKNOWN"
        print("[ExampleMod] Framework lifecycle changed: " .. phase)

        -- Auto-register when framework is ready
        if phase == "AWAITING_REGULAR_REGISTRATION" and not is_registered then
            register_with_framework()
        end
    end)

    return true
end

-- Register mod with framework (loads capabilities from AP_Config.json)
local function register_with_framework()
    print("[ExampleMod] Registering with framework...")

    -- In a real mod, capabilities would be loaded from AP_Config.json
    -- For this example, we'll create them manually
    local capabilities = {
        items = {
            { id = 5000, name = "TestItem_ExampleReward", classification = "filler" }
        },
        locations = {
            { id = 6000, name = "TestLocation_ExampleCheck", type = "static", region = "TestRegion" }
        },
        regions = {
            { name = "TestRegion", entrances = {} }
        }
    }

    local success, err = client:register(capabilities, false)  -- false = not a priority client
    if not success then
        print("[ExampleMod] Failed to register: " .. tostring(err))
        return
    end

    print("[ExampleMod] Registered with framework successfully!")
    is_registered = true
end

-- Example: Send a location check when player opens a chest
local function on_chest_opened(chest_id)
    if not client or not client:is_connected() then
        return
    end

    -- Map chest_id to AP location_id (this would be done based on your AP_Config.json)
    local location_id = 6000  -- Example: TestLocation_ExampleCheck

    print("[ExampleMod] Sending location check for chest " .. tostring(chest_id))

    local success, err = client:send_location_check(location_id)
    if not success then
        print("[ExampleMod] Failed to send location check: " .. tostring(err))
    end
end

-- Poll for messages from framework
local function poll_ap_client()
    if client and client:is_connected() then
        client:poll()
    end
end

-- Initialize on first tick using RegisterCustomEvent
-- Note: RegisterInitGameStateHook does NOT exist - use RegisterCustomEvent("Tick") instead
local current_time = os.clock()
local last_time = current_time

print("[ExampleClientMod] About to register Tick handler\n")

RegisterCustomEvent("Tick", function()
    print("[ExampleClientMod]: Tick handler called!\n")
    if not is_initialized then
        print("[ExampleClientMod]: Initializing...\n")
        local success = init_ap_client()
        if success then
            print("[ExampleClientMod]: Initialized!\n")
            is_initialized = true
        end
        last_time = os.clock()
        return
    end

    -- Poll for messages every tick
    print("[ExampleClientMod]: Polling...\n")
    poll_ap_client()

    -- Optional: Periodic status logging (once per second)
    current_time = os.clock()
    local delta_time = (current_time - last_time)
    if delta_time >= 1.0 then
        last_time = current_time
        print("[ExampleClientMod]: Tick...\n")
        -- Periodic operations can go here
    end
end)

-- Example: Hook into game events to send location checks
-- In a real mod, you would hook into actual game events
-- Example with UE4SS:
-- RegisterHook("/Script/Pal.PalPlayerController:OnOpenChest", function(self, chest)
--     local chest_id = chest:GetChestID()
--     on_chest_opened(chest_id)
-- end)

print("[ExampleMod] Loaded - will initialize on first tick\n")
