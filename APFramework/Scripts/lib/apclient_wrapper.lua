--[[
    APClient Wrapper for lua-apclientpp

    This wrapper provides debugging and a cleaner API around the lua-apclientpp DLL.
    It logs all API calls to help diagnose connection issues.
]]

local APClientWrapper = {}

-- Try to load the native lua-apclientpp module
local function load_native_module()
    local lib_dir = "ue4ss/Mods/APFramework/Scripts/lib/"
    package.cpath = package.cpath .. ";" .. lib_dir .. "?.dll"

    local success, apclientpp = pcall(require, "lua-apclientpp")

    if not success then
        print("[APClientWrapper] ERROR: Failed to load lua-apclientpp")
        print("[APClientWrapper] Error: " .. tostring(apclientpp))
        return nil
    end

    print("[APClientWrapper] Successfully loaded lua-apclientpp DLL")
    return apclientpp
end

-- Load the module
local apclientpp = load_native_module()

---Create a new AP client
---@param uuid string UUID (can be empty string)
---@param game_name string Game name
---@param server string Server address (host:port)
---@return table|nil client AP client instance or nil on failure
function APClientWrapper.new(uuid, game_name, server)
    if not apclientpp then
        print("[APClientWrapper] Cannot create client: DLL not loaded")
        return nil
    end

    print(string.format("[APClientWrapper] Creating client:"))
    print(string.format("  UUID: '%s'", uuid))
    print(string.format("  Game: '%s'", game_name))
    print(string.format("  Server: '%s'", server))

    local success, client = pcall(function()
        return apclientpp.new(uuid, game_name, server)
    end)

    if not success then
        print("[APClientWrapper] ERROR: Failed to create client")
        print("[APClientWrapper] Error: " .. tostring(client))
        return nil
    end

    if not client then
        print("[APClientWrapper] ERROR: Client is nil")
        return nil
    end

    print("[APClientWrapper] Client created successfully")
    print(string.format("[APClientWrapper] Client type: %s", type(client)))

    -- Wrap the client to add logging
    local wrapped = {
        _native = client,
        _handlers = {}
    }

    -- Add poll method with logging
    wrapped.poll = function(self)
        local success, err = pcall(function()
            self._native:poll()
        end)

        if not success then
            print("[APClientWrapper] ERROR in poll(): " .. tostring(err))
        end
    end

    -- Add get_state method
    wrapped.get_state = function(self)
        return self._native:get_state()
    end

    -- Add handler registration methods with logging
    wrapped.set_socket_connected_handler = function(self, handler)
        print("[APClientWrapper] Registering socket_connected_handler")
        self._handlers.socket_connected = handler
        self._native:set_socket_connected_handler(function()
            print("[APClientWrapper] HANDLER FIRED: socket_connected")
            handler()
        end)
    end

    wrapped.set_socket_disconnected_handler = function(self, handler)
        print("[APClientWrapper] Registering socket_disconnected_handler")
        self._handlers.socket_disconnected = handler
        self._native:set_socket_disconnected_handler(function()
            print("[APClientWrapper] HANDLER FIRED: socket_disconnected")
            handler()
        end)
    end

    wrapped.set_socket_error_handler = function(self, handler)
        print("[APClientWrapper] Registering socket_error_handler")
        self._handlers.socket_error = handler
        self._native:set_socket_error_handler(function(error_msg)
            print("[APClientWrapper] HANDLER FIRED: socket_error")
            print("[APClientWrapper] Error message: " .. tostring(error_msg))
            handler(error_msg)
        end)
    end

    wrapped.set_room_info_handler = function(self, handler)
        print("[APClientWrapper] Registering room_info_handler")
        self._handlers.room_info = handler
        self._native:set_room_info_handler(function()
            print("[APClientWrapper] HANDLER FIRED: room_info")
            handler()
        end)
    end

    wrapped.set_slot_connected_handler = function(self, handler)
        print("[APClientWrapper] Registering slot_connected_handler")
        self._handlers.slot_connected = handler
        self._native:set_slot_connected_handler(function(slot_data)
            print("[APClientWrapper] HANDLER FIRED: slot_connected")
            print("[APClientWrapper] Slot data: " .. tostring(slot_data))
            handler(slot_data)
        end)
    end

    wrapped.set_slot_refused_handler = function(self, handler)
        print("[APClientWrapper] Registering slot_refused_handler")
        self._handlers.slot_refused = handler
        self._native:set_slot_refused_handler(function(reasons)
            print("[APClientWrapper] HANDLER FIRED: slot_refused")
            print("[APClientWrapper] Reasons: " .. tostring(reasons))
            handler(reasons)
        end)
    end

    wrapped.set_items_received_handler = function(self, handler)
        print("[APClientWrapper] Registering items_received_handler")
        self._handlers.items_received = handler
        self._native:set_items_received_handler(function(items)
            print("[APClientWrapper] HANDLER FIRED: items_received")
            print("[APClientWrapper] Item count: " .. #items)
            handler(items)
        end)
    end

    wrapped.set_location_checked_handler = function(self, handler)
        print("[APClientWrapper] Registering location_checked_handler")
        self._handlers.location_checked = handler
        self._native:set_location_checked_handler(function(locations)
            print("[APClientWrapper] HANDLER FIRED: location_checked")
            print("[APClientWrapper] Location count: " .. #locations)
            handler(locations)
        end)
    end

    wrapped.set_print_handler = function(self, handler)
        print("[APClientWrapper] Registering print_handler")
        self._handlers.print = handler
        self._native:set_print_handler(function(message)
            print("[APClientWrapper] HANDLER FIRED: print")
            print("[APClientWrapper] Message: " .. tostring(message))
            handler(message)
        end)
    end

    wrapped.ConnectSlot = function(self, slot_name, password, items_handling, tags, client_version)
        print("[APClientWrapper] ConnectSlot called:")
        print(string.format("  Slot: '%s'", slot_name))
        print(string.format("  Password: '%s'", password))
        print(string.format("  Items handling: %d", items_handling))
        print(string.format("  Tags: %s", table.concat(tags, ", ")))
        print(string.format("  Version: %d.%d.%d", client_version[1], client_version[2], client_version[3]))

        local success, err = pcall(function()
            self._native:ConnectSlot(slot_name, password, items_handling, tags, client_version)
        end)

        if not success then
            print("[APClientWrapper] ERROR in ConnectSlot(): " .. tostring(err))
        else
            print("[APClientWrapper] ConnectSlot executed successfully")
        end
    end

    return wrapped
end

---Check if the native module is available
---@return boolean available
function APClientWrapper.is_available()
    return apclientpp ~= nil
end

return APClientWrapper
