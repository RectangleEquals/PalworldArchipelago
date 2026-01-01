-- Example Lua Mod Using ap_client.lua
-- This demonstrates how to use ap_client.lua in a Lua UE4SS mod

local APClient = require("ap_client")

-- Create client instance
local client = APClient:new("MyLuaMod")
local items_given = {}

-- Callback: Item received from Archipelago
client.on_item_received = function(item_id, location_id, player_slot)
    print(string.format("[MyLuaMod] Received item: %d from location: %d (player slot: %d)",
                        item_id, location_id, player_slot))

    -- Example: Give specific item to player (only once)
    if item_id == 100000 and not items_given[item_id] then
        print("[MyLuaMod] Giving Super Pickaxe to player!")

        -- TODO: Game-specific code to spawn item in player inventory
        -- Example (pseudo-code for UE4SS):
        -- local player = FindFirstOf("PalPlayerCharacter")
        -- if player then
        --     player:AddItemToInventory("SuperPickaxe", 1)
        -- end

        items_given[item_id] = true
    elseif item_id == 100001 and not items_given[item_id] then
        print("[MyLuaMod] Giving Mega Sword to player!")
        -- Give sword to player...
        items_given[item_id] = true
    end
end

-- Callback: Location checked confirmation
client.on_location_checked = function(location_id)
    print(string.format("[MyLuaMod] Location checked confirmed: %d", location_id))
end

-- Callback: Connection status changed
client.on_connection_status = function(connected, slot_name)
    if connected then
        print("[MyLuaMod] Connected to Archipelago as: " .. slot_name)
    else
        print("[MyLuaMod] Disconnected from Archipelago")
    end
end

-- Callback: All mods registered
client.on_registration_complete = function()
    print("[MyLuaMod] All mods registered! Framework is ready.")

    -- Optional: Auto-request connection
    -- client:request_connection("localhost", 38281, "Player1", "")
end

-- Register mod capabilities with framework
print("[MyLuaMod] Registering with APFramework...")
local success = client:register({
    items = {
        {id = 100000, name = "Super Pickaxe", classification = "useful"},
        {id = 100001, name = "Mega Sword", classification = "progression"}
    },
    locations = {
        {id = 200000, name = "Chest in Cave", region = "Starting Area"},
        {id = 200001, name = "Boss Chest", region = "Boss Arena"}
    },
    regions = {
        {name = "Starting Area", connects_to = {"Boss Arena"}},
        {name = "Boss Arena", connects_to = {}}
    }
})

if success then
    print("[MyLuaMod] Registered successfully!")
else
    print("[MyLuaMod] ERROR: Failed to register with framework!")
end

-- Hook into game tick to poll messages
RegisterHook("/Script/Engine.PlayerController:PlayerTick", function()
    client:poll()
end)

-- Example: Check a location when player opens a specific chest
-- This would be a custom hook or event in your game
local function on_chest_opened(chest_id)
    -- Map game chest IDs to AP location IDs
    if chest_id == 42 then  -- Example: Chest #42 in game
        print("[MyLuaMod] Checking location: Chest in Cave")
        client:check_location(200000)
    elseif chest_id == 99 then  -- Boss chest
        print("[MyLuaMod] Checking location: Boss Chest")
        client:check_location(200001)
    end
end

-- Example: Hook for chest opening (game-specific)
-- RegisterHook("/Script/Pal.PalPlayerCharacter:OnOpenChest", function(self, ChestID)
--     on_chest_opened(ChestID:get())
-- end)

-- Cleanup on mod unload
RegisterHook("/Script/Engine.GameInstance:Shutdown", function()
    print("[MyLuaMod] Shutting down APClient...")
    client:shutdown()
end)

print("[MyLuaMod] Initialization complete!")
