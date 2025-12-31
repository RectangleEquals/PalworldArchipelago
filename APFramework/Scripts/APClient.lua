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

    -- Update package.cpath to find lua-apclientpp.dll
    -- Working directory is Binaries/Win64
    local lib_dir = "ue4ss/Mods/APFramework/Scripts/lib/"
    package.cpath = package.cpath .. ";" .. lib_dir .. "?.dll"

    print("[APClient] DEBUG: package.cpath = " .. package.cpath)

    -- Try to load lua-apclientpp
    local success, apclientpp = pcall(require, "lua-apclientpp")

    if not success then
        print("[APClient] Warning: lua-apclientpp not available")
        print("[APClient] Error: " .. tostring(apclientpp))
        print("[APClient] Running in stub mode (no actual server connection)")
        self.initialized = true
        return true
    end

    -- Create client instance
    -- lua-apclientpp.new() requires: uuid, game_name, server
    local uuid = self.connection.uuid or ""
    local game_name = self.connection.game or "Palworld"
    local server = string.format("%s:%d", host, port)

    print(string.format("[APClient] Creating client (game=%s, server=%s)", game_name, server))
    self.client = apclientpp.new(uuid, game_name, server)

    if not self.client then
        print("[APClient ERROR] Failed to create AP client instance")
        return false
    end

    print("[APClient] Successfully loaded lua-apclientpp!")
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

    -- Set up event handlers (including room_info handler that calls ConnectSlot)
    self:SetupEventHandlers()

    print("[APClient] Starting connection process...")
    print("[APClient] Polling to establish connection...")

    -- Poll to process network events and trigger handlers
    -- The room_info handler will call ConnectSlot when ready
    for i = 1, 100 do
        self:Poll()
        if i % 20 == 0 then
            local state = self.client:get_state()
            print(string.format("[APClient] Poll iteration %d (state=%d, connected=%s, authenticated=%s)",
                i, state, tostring(self.connected), tostring(self.authenticated)))
        end
    end

    print("[APClient] Initial polling complete")
    local final_state = self.client:get_state()
    print(string.format("[APClient] Final state: %d (connected=%s, authenticated=%s)",
        final_state, tostring(self.connected), tostring(self.authenticated)))
    print("[APClient] Note: Continuous polling needed for real-time events")

    return true
end

---Set up event handlers for the AP client
function APClient:SetupEventHandlers()
    if not self.client then
        return
    end

    print("[APClient] Setting up event handlers...")

    -- Socket connected event
    self.client:set_socket_connected_handler(function()
        print("[APClient] Socket connected to server")
        self.connected = true
    end)

    -- Room info handler - called when connected, triggers authentication
    self.client:set_room_info_handler(function()
        print("[APClient] Received room info, authenticating...")

        local items_handling = 7  -- 0b111 = receive all items
        local tags = {"Lua-APClientPP", "Palworld"}
        local client_version = {0, 5, 0}

        print(string.format("[APClient] Calling ConnectSlot (slot=%s)", self.connection.slot_name))
        self.client:ConnectSlot(
            self.connection.slot_name,
            self.connection.password or "",
            items_handling,
            tags,
            client_version
        )
    end)

    -- Socket disconnected event
    self.client:set_socket_disconnected_handler(function()
        print("[APClient] Socket disconnected from server")
        self.connected = false
        self.authenticated = false

        if self.callbacks.onDisconnected then
            self.callbacks.onDisconnected()
        end
    end)

    -- Socket error event
    self.client:set_socket_error_handler(function(error_msg)
        print(string.format("[APClient] Socket error: %s", tostring(error_msg)))
    end)

    -- Slot connected event (authenticated successfully)
    self.client:set_slot_connected_handler(function(slot_data)
        print("[APClient] Slot connected! Authenticated successfully")
        self.authenticated = true

        if self.callbacks.onConnected then
            self.callbacks.onConnected()
        end
    end)

    -- Slot refused event (authentication failed)
    self.client:set_slot_refused_handler(function(reasons)
        print(string.format("[APClient] Slot refused: %s", tostring(reasons)))
        self.authenticated = false
    end)

    -- Items received event
    self.client:set_items_received_handler(function(items)
        for _, item_data in ipairs(items) do
            print(string.format("[APClient] Item received: %s", tostring(item_data.item)))

            if self.callbacks.onItemReceived then
                self.callbacks.onItemReceived(item_data)
            end
        end
    end)

    -- Location checked event
    self.client:set_location_checked_handler(function(locations)
        for _, location_id in ipairs(locations) do
            print(string.format("[APClient] Location checked: %d", location_id))

            if self.callbacks.onLocationChecked then
                self.callbacks.onLocationChecked(location_id)
            end
        end
    end)

    -- Print handler for server messages
    self.client:set_print_handler(function(message)
        print(string.format("[AP Server] %s", message))
    end)

    print("[APClient] Event handlers registered")
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
        self.client:poll()
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
