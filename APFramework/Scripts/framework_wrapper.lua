-- APFramework Lua C API Wrapper
-- Wraps APFrameworkCore.dll using native Lua C bindings

-- Load the native module
local core = require("APFrameworkCore")

-- Wrapper class
local FrameworkWrapper = {}
FrameworkWrapper.__index = FrameworkWrapper

function FrameworkWrapper:new(pipe_name)
    local obj = {
        handle = core.create(pipe_name),
        pipe_name = pipe_name
    }
    setmetatable(obj, self)
    return obj
end

function FrameworkWrapper:shutdown()
    if self.handle then
        self.handle:stop_polling()
        self.handle:disconnect_ap()
        self.handle:stop_ipc()
        -- handle will be automatically garbage collected
        self.handle = nil
    end
end

-- Configuration
function FrameworkWrapper:load_config(config_path)
    return self.handle:load_config(config_path)
end

function FrameworkWrapper:save_config(config_path)
    return self.handle:save_config(config_path)
end

function FrameworkWrapper:get_active_profile()
    return self.handle:get_active_profile()
end

function FrameworkWrapper:set_active_profile(profile_json)
    return self.handle:set_active_profile(profile_json)
end

-- IPC
function FrameworkWrapper:start_ipc()
    self.handle:start_ipc()
end

function FrameworkWrapper:stop_ipc()
    self.handle:stop_ipc()
end

-- Mod Discovery
function FrameworkWrapper:discover_mods(mods_directory)
    self.handle:discover_mods(mods_directory)
end

function FrameworkWrapper:all_mods_registered()
    return self.handle:all_mods_registered()
end

function FrameworkWrapper:get_pending_registrations()
    return self.handle:get_pending_registrations()
end

-- Capabilities
function FrameworkWrapper:generate_capabilities()
    return self.handle:generate_capabilities()
end

-- AP Connection
function FrameworkWrapper:connect_ap(server, port, slot_name, password)
    return self.handle:connect_ap(server, port, slot_name, password or "")
end

function FrameworkWrapper:disconnect_ap()
    self.handle:disconnect_ap()
end

function FrameworkWrapper:is_connected()
    return self.handle:is_ap_connected()
end

-- Polling
function FrameworkWrapper:start_polling()
    self.handle:start_polling()
end

function FrameworkWrapper:stop_polling()
    self.handle:stop_polling()
end

return FrameworkWrapper
