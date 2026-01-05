-- Configuration Management for APFramework
-- Uses lunajson for robust JSON parsing

local json = require("lunajson")

local Config = {}
Config.__index = Config

function Config:new()
    local obj = {
        server = "archipelago.gg",
        port = 38281,
        slot_name = "P1",
        password = "",
        autoconnect = false,
        registration_timeout = 180,  -- 3 minutes by default
        log_mode = "framework_only",  -- "minimal", "framework_only", "all"
        log_verbosity = "info"  -- "debug", "info", "warning", "error"
    }
    setmetatable(obj, self)
    return obj
end

-- Load configuration from JSON file
function Config:load(config_path)
    local file = io.open(config_path, "r")
    if not file then
        print("[Config] No config file found, using defaults")
        return false
    end

    local content = file:read("*a")
    file:close()

    -- Parse JSON using lunajson
    local success, config_data = pcall(json.decode, content)
    if not success then
        print("[Config] ERROR: Failed to parse config JSON: " .. tostring(config_data))
        print("[Config] Using default configuration")
        return false
    end

    -- Apply config values (with defaults for missing fields)
    self.server = config_data.server or self.server
    self.port = config_data.port or self.port
    self.slot_name = config_data.slot_name or self.slot_name
    self.password = config_data.password or self.password

    -- Handle boolean autoconnect (explicit check for nil vs false)
    if config_data.autoconnect ~= nil then
        self.autoconnect = config_data.autoconnect
    end

    self.registration_timeout = config_data.registration_timeout or self.registration_timeout
    self.log_mode = config_data.log_mode or self.log_mode
    self.log_verbosity = config_data.log_verbosity or self.log_verbosity

    print("[Config] Configuration loaded successfully")
    return true
end

-- Save configuration to JSON file
function Config:save(config_path)
    local config_data = {
        server = self.server,
        port = self.port,
        slot_name = self.slot_name,
        password = self.password,
        autoconnect = self.autoconnect,
        registration_timeout = self.registration_timeout,
        log_mode = self.log_mode,
        log_verbosity = self.log_verbosity
    }

    local success, json_str = pcall(json.encode, config_data)
    if not success then
        print("[Config] ERROR: Failed to encode config JSON: " .. tostring(json_str))
        return false
    end

    local file = io.open(config_path, "w")
    if not file then
        print("[Config] ERROR: Could not save config file: " .. config_path)
        return false
    end

    file:write(json_str)
    file:close()

    print("[Config] Configuration saved successfully")
    return true
end

return Config