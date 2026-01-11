-- APFramework.lua
-- High-level Lua wrapper for APFrameworkCore
--
-- Provides a convenient interface for interacting with the framework from Lua.
-- All heavy lifting is done in C++ (APFrameworkCore), this just provides
-- a user-friendly Lua API.

local APFramework = {}

-- Initialize the framework
-- @param config_path Path to framework_config.json (optional, defaults to "framework_config.json")
-- @return true on success, or nil + error message on failure
function APFramework.init(config_path)
    config_path = config_path or "framework_config.json"

    local manager = APManager.instance()
    local result = manager:init(config_path)

    if not result:is_success() then
        return nil, result.error_message
    end

    return true
end

-- Start the framework (begins state machine and IPC server)
-- @return true on success, or nil + error message on failure
function APFramework.start()
    local manager = APManager.instance()
    local result = manager:start()

    if not result:is_success() then
        return nil, result.error_message
    end

    return true
end

-- Shutdown the framework (for convenience only - may never be called!)
-- The framework MUST be memory-safe even if this is never called
function APFramework.shutdown()
    local manager = APManager.instance()
    manager:shutdown()
end

-- Get the current lifecycle phase
-- @return LifecyclePhase enum value
function APFramework.get_phase()
    local manager = APManager.instance()
    return manager:get_current_phase()
end

-- Get the current lifecycle phase as a string
-- @return string representation of the current phase
function APFramework.get_phase_string()
    local phase = APFramework.get_phase()
    return lifecycle_phase_to_string(phase)
end

-- Check if the framework is running
-- @return boolean
function APFramework.is_running()
    local manager = APManager.instance()
    return manager:is_running()
end

-- Send a command to the framework (priority clients only)
-- @param cmd Command name (e.g., "CONNECT", "GENERATE", "DISCONNECT", "RESYNC")
-- @param data Optional table of command data
-- @return true on success, or nil + error message on failure
function APFramework.send_command(cmd, data)
    local manager = APManager.instance()
    local result = manager:handle_command(cmd, data or {})

    if not result:is_success() then
        return nil, result.error_message
    end

    return true
end

-- Trigger a resync
-- @return true on success, or nil + error message on failure
function APFramework.resync()
    local manager = APManager.instance()
    local result = manager:trigger_resync()

    if not result:is_success() then
        return nil, result.error_message
    end

    return true
end

-- Logging helpers
APFramework.Log = {}

function APFramework.Log.trace(message)
    local logger = APLogger.instance()
    logger:trace(message)
end

function APFramework.Log.debug(message)
    local logger = APLogger.instance()
    logger:debug(message)
end

function APFramework.Log.info(message)
    local logger = APLogger.instance()
    logger:info(message)
end

function APFramework.Log.warn(message)
    local logger = APLogger.instance()
    logger:warn(message)
end

function APFramework.Log.error(message)
    local logger = APLogger.instance()
    logger:error(message)
end

function APFramework.Log.fatal(message)
    local logger = APLogger.instance()
    logger:fatal(message)
end

return APFramework
