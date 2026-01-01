-- APFramework FFI Wrapper
-- Wraps APFrameworkCore.dll C functions using LuaJIT FFI

local ffi = require("ffi")

-- FFI definitions from ffi_bindings.h
ffi.cdef[[
    // Opaque handle
    typedef void* FrameworkHandle;

    // Lifecycle
    FrameworkHandle framework_core_create(const char* pipe_name);
    void framework_core_destroy(FrameworkHandle handle);

    // IPC
    void framework_core_start_ipc(FrameworkHandle handle);
    void framework_core_stop_ipc(FrameworkHandle handle);

    // Mod discovery
    void framework_core_discover_mods(FrameworkHandle handle, const char* mods_directory);
    bool framework_core_all_mods_registered(FrameworkHandle handle);
    char* framework_core_get_pending_registrations(FrameworkHandle handle);

    // Capabilities
    char* framework_core_generate_capabilities(FrameworkHandle handle);

    // AP Connection
    bool framework_core_connect_ap(FrameworkHandle handle,
                                    const char* server, int port,
                                    const char* slot_name, const char* password);
    void framework_core_disconnect_ap(FrameworkHandle handle);
    bool framework_core_is_connected(FrameworkHandle handle);

    // Polling
    void framework_core_start_polling(FrameworkHandle handle);
    void framework_core_stop_polling(FrameworkHandle handle);

    // Configuration
    bool framework_core_load_config(FrameworkHandle handle, const char* config_path);
    bool framework_core_save_config(FrameworkHandle handle, const char* config_path);
    char* framework_core_get_active_profile_json(FrameworkHandle handle);
    bool framework_core_set_active_profile(FrameworkHandle handle, const char* profile_json);

    // Utility
    void framework_core_free_string(char* str);
]]

-- Load DLL (relative to game root or absolute path)
local dll_path = "Mods/APFramework/APFrameworkCore.dll"
local core = ffi.load(dll_path)

-- Wrapper class
local FrameworkWrapper = {}
FrameworkWrapper.__index = FrameworkWrapper

function FrameworkWrapper:new(pipe_name)
    local obj = {
        handle = core.framework_core_create(pipe_name),
        pipe_name = pipe_name
    }
    setmetatable(obj, self)
    return obj
end

function FrameworkWrapper:shutdown()
    if self.handle then
        core.framework_core_stop_polling(self.handle)
        core.framework_core_disconnect_ap(self.handle)
        core.framework_core_stop_ipc(self.handle)
        core.framework_core_destroy(self.handle)
        self.handle = nil
    end
end

-- Configuration
function FrameworkWrapper:load_config(config_path)
    return core.framework_core_load_config(self.handle, config_path)
end

function FrameworkWrapper:save_config(config_path)
    return core.framework_core_save_config(self.handle, config_path)
end

function FrameworkWrapper:get_active_profile()
    local c_str = core.framework_core_get_active_profile_json(self.handle)
    if c_str == nil then
        return nil
    end
    local str = ffi.string(c_str)
    core.framework_core_free_string(c_str)
    return str
end

function FrameworkWrapper:set_active_profile(profile_json)
    return core.framework_core_set_active_profile(self.handle, profile_json)
end

-- IPC
function FrameworkWrapper:start_ipc()
    core.framework_core_start_ipc(self.handle)
end

function FrameworkWrapper:stop_ipc()
    core.framework_core_stop_ipc(self.handle)
end

-- Mod Discovery
function FrameworkWrapper:discover_mods(mods_directory)
    core.framework_core_discover_mods(self.handle, mods_directory)
end

function FrameworkWrapper:all_mods_registered()
    return core.framework_core_all_mods_registered(self.handle)
end

function FrameworkWrapper:get_pending_registrations()
    local c_str = core.framework_core_get_pending_registrations(self.handle)
    if c_str == nil then
        return nil
    end
    local str = ffi.string(c_str)
    core.framework_core_free_string(c_str)
    return str
end

-- Capabilities
function FrameworkWrapper:generate_capabilities()
    local c_str = core.framework_core_generate_capabilities(self.handle)
    if c_str == nil then
        return nil
    end
    local str = ffi.string(c_str)
    core.framework_core_free_string(c_str)
    return str
end

-- AP Connection
function FrameworkWrapper:connect_ap(server, port, slot_name, password)
    return core.framework_core_connect_ap(self.handle, server, port, slot_name, password or "")
end

function FrameworkWrapper:disconnect_ap()
    core.framework_core_disconnect_ap(self.handle)
end

function FrameworkWrapper:is_connected()
    return core.framework_core_is_connected(self.handle)
end

-- Polling
function FrameworkWrapper:start_polling()
    core.framework_core_start_polling(self.handle)
end

function FrameworkWrapper:stop_polling()
    core.framework_core_stop_polling(self.handle)
end

return FrameworkWrapper
