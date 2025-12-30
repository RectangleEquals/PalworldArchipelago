--[[
    Palworld Archipelago Framework
    Module: APClient

    Wrapper for lua-apclientpp library to handle Archipelago server connections.
    Manages authentication, location checks, item receipts, and connection state.
]]

local APClient = {}

-- Module state
APClient.initialized = false
APClient.connected = false
APClient.authenticated = false

-- Connection details
APClient.connection = {
    host = nil,
    port = nil,
    slot_name = nil,
    password = nil,
    game = "Palworld",
    uuid = nil -- Generated or loaded
}

-- AP client instance (from lua-apclientpp)
APClient.client = nil

-- Event callbacks
APClient.callbacks = {
    onConnected = nil,
    onDisconnected = nil,
    onLocationChecked = nil,
    onItemReceived = nil,
    onGoalComplete = nil,
    onBounce = nil
}

-- Message queue for offline mode
APClient.message_queue = {
    location_checks = {},
    scout_locations = {}
}

---Initialize the AP client
---@param host string Server hostname
---@param port integer Server port
---@return boolean success
function APClient:Initialize(host, port)
    if self.initialized then
        print("[APClient] Warning: Already initialized")
        return true
    end

    self.connection.host = host
    self.connection.port = port

    -- Try to load lua-apclientpp
    local success, apclientpp = pcall(require, "apclientpp")

    if not success then
        print("[APClient] Warning: lua-apclientpp not available")
        print("[APClient] Running in stub mode (no actual server connection)")
        self.initialized = true
        return true
    end

    -- Create client instance
    self.client = apclientpp.new()

    if not self.client then
        print("[APClient ERROR] Failed to create AP client instance")
        return false
    end

    self.initialized = true
    print(string.format("[APClient] Initialized for %s:%d", host, port))

    return true
end

---Connect and authenticate with the AP server
---@param slot_name string Player slot name
---@param password string|nil Server password
---@param game_name string|nil Game name (defaults to "Palworld")
---@return boolean success
function APClient:Authenticate(slot_name, password, game_name)
    if not self.initialized then
        print("[APClient ERROR] Not initialized")
        return false
    end

    self.connection.slot_name = slot_name
    self.connection.password = password
    self.connection.game = game_name or "Palworld"

    -- If running in stub mode (no lua-apclientpp), simulate connection
    if not self.client then
        print(string.format("[APClient] STUB: Would connect to %s:%d as '%s'",
              self.connection.host, self.connection.port, slot_name))
        self.connected = true
        self.authenticated = true

        -- Fire connected callback if set
        if self.callbacks.onConnected then
            self.callbacks.onConnected()
        end

        return true
    end

    -- Real connection with lua-apclientpp
    local success, err = pcall(function()
        self.client:Connect({
            hostname = self.connection.host,
            port = self.connection.port,
            game = self.connection.game,
            name = slot_name,
            password = password,
            uuid = self.connection.uuid,
            items_handling = 7, -- All items (binary 0b111 = 7)
            tags = {"AP", "Palworld"}
        })
    end)

    if not success then
        print(string.format("[APClient ERROR] Connection failed: %s", tostring(err)))
        return false
    end

    self.connected = true
    self.authenticated = true

    -- Set up event handlers
    self:SetupEventHandlers()

    print(string.format("[APClient] Connected as '%s'", slot_name))

    return true
end

---Set up event handlers for the AP client
function APClient:SetupEventHandlers()
    if not self.client then
        return
    end

    -- Connected event
    self.client:SetCallback("Connected", function()
        print("[APClient] Connected to server")
        self.connected = true

        if self.callbacks.onConnected then
            self.callbacks.onConnected()
        end
    end)

    -- Disconnected event
    self.client:SetCallback("Disconnected", function()
        print("[APClient] Disconnected from server")
        self.connected = false
        self.authenticated = false

        if self.callbacks.onDisconnected then
            self.callbacks.onDisconnected()
        end
    end)

    -- Location checked event
    self.client:SetCallback("LocationChecked", function(location_id)
        print(string.format("[APClient] Location checked: %d", location_id))

        if self.callbacks.onLocationChecked then
            self.callbacks.onLocationChecked(location_id)
        end
    end)

    -- Item received event
    self.client:SetCallback("ItemReceived", function(item_index, item_id, item_name, player_name)
        local item_data = {
            index = item_index,
            item_id = item_id,
            item_name = item_name,
            player_name = player_name
        }

        print(string.format("[APClient] Item received: %s from %s", item_name, player_name))

        if self.callbacks.onItemReceived then
            self.callbacks.onItemReceived(item_data)
        end
    end)

    -- Goal complete event
    self.client:SetCallback("GoalComplete", function()
        print("[APClient] Goal completed!")

        if self.callbacks.onGoalComplete then
            self.callbacks.onGoalComplete()
        end
    end)
end

---Check a location (send to server)
---@param location_id integer AP location ID
function APClient:CheckLocation(location_id)
    if not self.initialized then
        print("[APClient ERROR] Not initialized")
        return
    end

    -- Queue if not connected
    if not self.connected then
        table.insert(self.message_queue.location_checks, location_id)
        print(string.format("[APClient] Queued location check: %d (offline)", location_id))
        return
    end

    -- Stub mode
    if not self.client then
        print(string.format("[APClient] STUB: Would check location %d", location_id))
        return
    end

    -- Real check
    local success, err = pcall(function()
        self.client:LocationChecked({location_id})
    end)

    if not success then
        print(string.format("[APClient ERROR] Failed to check location: %s", tostring(err)))
    end
end

---Scout locations (query what items are at locations)
---@param location_ids table Array of location IDs
---@param callback function|nil Callback(location_id, item_id, player_name)
function APClient:ScoutLocations(location_ids, callback)
    if not self.initialized then
        print("[APClient ERROR] Not initialized")
        return
    end

    if not self.connected then
        print("[APClient] Cannot scout while offline")
        return
    end

    if not self.client then
        print("[APClient] STUB: Would scout locations")
        return
    end

    -- Real scout
    local success, err = pcall(function()
        self.client:LocationScouts(location_ids, function(locations)
            if callback then
                for _, loc_info in ipairs(locations) do
                    callback(loc_info.location, loc_info.item, loc_info.player)
                end
            end
        end)
    end)

    if not success then
        print(string.format("[APClient ERROR] Failed to scout: %s", tostring(err)))
    end
end

---Send a message to the server/other players
---@param message string Message text
function APClient:Say(message)
    if not self.connected or not self.client then
        print("[APClient] Cannot send message (not connected)")
        return
    end

    local success, err = pcall(function()
        self.client:Say(message)
    end)

    if not success then
        print(string.format("[APClient ERROR] Failed to send message: %s", tostring(err)))
    end
end

---Update/poll the AP client (call regularly in game loop)
function APClient:Poll()
    if not self.client then
        return
    end

    local success, err = pcall(function()
        self.client:Poll()
    end)

    if not success then
        print(string.format("[APClient ERROR] Poll failed: %s", tostring(err)))
    end
end

---Disconnect from the server
function APClient:Disconnect()
    if not self.connected then
        return
    end

    if self.client then
        local success, err = pcall(function()
            self.client:Disconnect()
        end)

        if not success then
            print(string.format("[APClient ERROR] Disconnect failed: %s", tostring(err)))
        end
    end

    self.connected = false
    self.authenticated = false

    print("[APClient] Disconnected")
end

---Set callback for connected event
---@param callback function Callback()
function APClient:OnConnected(callback)
    self.callbacks.onConnected = callback
end

---Set callback for disconnected event
---@param callback function Callback()
function APClient:OnDisconnected(callback)
    self.callbacks.onDisconnected = callback
end

---Set callback for location checked event
---@param callback function Callback(location_id)
function APClient:OnLocationChecked(callback)
    self.callbacks.onLocationChecked = callback
end

---Set callback for item received event
---@param callback function Callback(item_data)
function APClient:OnItemReceived(callback)
    self.callbacks.onItemReceived = callback
end

---Set callback for goal complete event
---@param callback function Callback()
function APClient:OnGoalComplete(callback)
    self.callbacks.onGoalComplete = callback
end

---Get connection status
---@return table status Connection status info
function APClient:GetStatus()
    return {
        initialized = self.initialized,
        connected = self.connected,
        authenticated = self.authenticated,
        host = self.connection.host,
        port = self.connection.port,
        slot_name = self.connection.slot_name,
        stub_mode = self.client == nil
    }
end

---Flush queued messages (send all queued location checks)
function APClient:FlushQueue()
    if not self.connected then
        print("[APClient] Cannot flush queue (not connected)")
        return
    end

    local count = #self.message_queue.location_checks

    if count == 0 then
        return
    end

    print(string.format("[APClient] Flushing %d queued location checks", count))

    for _, location_id in ipairs(self.message_queue.location_checks) do
        self:CheckLocation(location_id)
    end

    self.message_queue.location_checks = {}
end

---Print status information
function APClient:PrintStatus()
    local status = self:GetStatus()

    print("[APClient] === Status ===")
    print(string.format("  Initialized: %s", status.initialized))
    print(string.format("  Connected: %s", status.connected))
    print(string.format("  Authenticated: %s", status.authenticated))

    if status.host then
        print(string.format("  Server: %s:%d", status.host, status.port))
    end

    if status.slot_name then
        print(string.format("  Slot: %s", status.slot_name))
    end

    print(string.format("  Stub Mode: %s", status.stub_mode))
    print(string.format("  Queued Checks: %d", #self.message_queue.location_checks))
    print("[APClient] ================")
end

return APClient
