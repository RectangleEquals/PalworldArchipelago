-- APClient.lua
-- High-level Lua wrapper for APClientLib
--
-- Provides a user-friendly interface for AP-enabled mods to communicate
-- with the APFramework.

-- Load the APClientLib DLL (registers C++ bindings into this Lua state)
-- This DLL is a Lua-loadable library, NOT a UE4SS C++ mod
local success, err = pcall(require, "APClientLib")
if not success then
    error("[APClient] Failed to load APClientLib.dll: " .. tostring(err))
end

local APClient = {}
APClient.__index = APClient

-- Create a new APClient instance
-- @param mod_id Unique mod identifier (e.g., "mymod.palworld.example")
-- @return APClient instance
function APClient:new(mod_id)
    local obj = setmetatable({}, APClient)
    obj.client = APClientNative.new()  -- C++ APClient from lua_bindings
    obj.mod_id = mod_id
    obj.callbacks = {
        received_items = {},
        location_info = {},
        lifecycle = {},
        custom = {}
    }
    obj.is_initialized = false
    return obj
end

-- Initialize and connect to framework
-- @param pipe_name Optional pipe name (defaults to framework default)
-- @return true on success, or nil + error message on failure
function APClient:init(pipe_name)
    local result
    if pipe_name then
        result = self.client:init(self.mod_id, pipe_name)
    else
        result = self.client:init(self.mod_id)
    end

    if not result:is_success() then
        return nil, result.error_message
    end

    -- Set up C++ callbacks to route to Lua callbacks
    self.client:on_received_items(function(data)
        self:_trigger_callbacks("received_items", data)
    end)

    self.client:on_location_info(function(data)
        self:_trigger_callbacks("location_info", data)
    end)

    self.client:on_lifecycle_change(function(msg)
        self:_trigger_callbacks("lifecycle", msg)
    end)

    self.is_initialized = true
    return true
end

-- Register mod with framework
-- @param capabilities JSON object with mod capabilities
-- @param is_priority Whether this is a priority client
-- @return true on success, or nil + error message on failure
function APClient:register(capabilities, is_priority)
    is_priority = is_priority or false

    local result = self.client:register_with_framework(capabilities, is_priority)
    if not result:is_success() then
        return nil, result.error_message
    end

    return true
end

-- Send a location check to the framework
-- @param location_id AP location ID to check
-- @return true on success, or nil + error message on failure
function APClient:send_location_check(location_id)
    local result = self.client:send_location_check(location_id)
    if not result:is_success() then
        return nil, result.error_message
    end
    return true
end

-- Send multiple location checks (batched)
-- @param location_ids Table of location IDs
-- @return true on success, or nil + error message on failure
function APClient:send_location_checks(location_ids)
    local result = self.client:send_location_checks(location_ids)
    if not result:is_success() then
        return nil, result.error_message
    end
    return true
end

-- Send a command to the framework (priority clients only)
-- @param cmd Command name
-- @param data Optional command data (table)
-- @return true on success, or nil + error message on failure
function APClient:send_command(cmd, data)
    local result = self.client:send_command(cmd, data or {})
    if not result:is_success() then
        return nil, result.error_message
    end
    return true
end

-- Poll for messages from framework
-- Call this regularly (e.g., every tick) to process incoming messages
function APClient:poll()
    if self.is_initialized then
        self.client:poll()
    end
end

-- Register callback for received items from AP server
-- @param callback Function to call: function(item_data) ... end
function APClient:on_received_items(callback)
    table.insert(self.callbacks.received_items, callback)
end

-- Register callback for location info updates
-- @param callback Function to call: function(location_data) ... end
function APClient:on_location_info(callback)
    table.insert(self.callbacks.location_info, callback)
end

-- Register callback for lifecycle phase changes
-- @param callback Function to call: function(msg) ... end
function APClient:on_lifecycle_change(callback)
    table.insert(self.callbacks.lifecycle, callback)
end

-- Register callback for custom message types
-- @param message_type Type of message to listen for
-- @param callback Function to call: function(msg) ... end
function APClient:on_message(message_type, callback)
    if not self.callbacks.custom[message_type] then
        self.callbacks.custom[message_type] = {}

        -- Register with C++ layer
        self.client:on_message(message_type, function(msg)
            self:_trigger_callbacks("custom_" .. message_type, msg)
        end)
    end

    table.insert(self.callbacks.custom[message_type], callback)
end

-- Check if connected to framework
-- @return boolean
function APClient:is_connected()
    return self.client:is_connected()
end

-- Get mod ID
-- @return string
function APClient:get_mod_id()
    return self.mod_id
end

-- Internal: Trigger all callbacks for a specific type
function APClient:_trigger_callbacks(callback_type, data)
    local callbacks
    if callback_type:sub(1, 7) == "custom_" then
        local msg_type = callback_type:sub(8)
        callbacks = self.callbacks.custom[msg_type] or {}
    else
        callbacks = self.callbacks[callback_type] or {}
    end

    for _, callback in ipairs(callbacks) do
        local success, err = pcall(callback, data)
        if not success then
            print("[APClient] Error in callback: " .. tostring(err))
        end
    end
end

return APClient