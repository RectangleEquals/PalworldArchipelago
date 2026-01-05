-- APClient Lua Library
-- Thin wrapper around APClientLib C library

local client_lib = require("APClientLib")
local json = require("lunajson")

local APClient = {}
APClient.__index = APClient

function APClient:new(mod_id)
    local obj = {
        mod_id = mod_id,
        client = client_lib.new(mod_id),
        connected = false, -- Will be set during first poll

        -- User callbacks
        on_item_received = nil,
        on_location_checked = nil,
        on_connection_status = nil,
        on_registration_complete = nil
    }

    setmetatable(obj, self)

    -- Setup C callbacks to call Lua callbacks
    obj.client:set_item_received_callback(function(item_id, location_id, player_slot)
        if obj.on_item_received then
            obj.on_item_received(item_id, location_id, player_slot)
        end
    end)

    obj.client:set_location_checked_callback(function(location_id)
        if obj.on_location_checked then
            obj.on_location_checked(location_id)
        end
    end)

    obj.client:set_connection_status_callback(function(connected, slot_name)
        if obj.on_connection_status then
            obj.on_connection_status(connected, slot_name)
        end
    end)

    obj.client:set_registration_complete_callback(function()
        if obj.on_registration_complete then
            obj.on_registration_complete()
        end
    end)

    return obj
end

function APClient:register(capabilities)
    -- Convert capabilities table to JSON
    local ok, caps_json = pcall(json.encode, capabilities)
    if not ok then
        print("[APClient] ERROR: Failed to encode capabilities: " .. tostring(caps_json))
        return false
    end

    return self.client:register(caps_json)
end

function APClient:poll()
    self.client:poll()

    -- Update connected status
    self.connected = self.client:is_connected()
end

function APClient:check_location(location_id)
    self.client:check_location(location_id)
end

function APClient:request_connection(server, port, slot_name, password)
    self.client:request_connection(server, port, slot_name, password or "")
end

function APClient:shutdown()
    -- Cleanup handled by C library garbage collection
    self.client = nil
end

return APClient
