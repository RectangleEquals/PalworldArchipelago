-- Configuration Management for APFramework
-- Simplified configuration for the IPC branch

local Config = {}
Config.__index = Config

function Config:new()
    local obj = {
        server = "archipelago.gg",
        port = 38281,
        slot_name = "P1",
        password = "",
        autoconnect = false
    }
    setmetatable(obj, self)
    return obj
end

-- Load configuration from JSON file
function Config:load(config_path)
    local file = io.open(config_path, "r")
    if not file then
        print("[APFramework] No config file found, using defaults")
        return false
    end

    local content = file:read("*a")
    file:close()

    -- Very basic JSON parsing for simple flat structure
    self.server = content:match('"server"%s*:%s*"([^"]+)"') or self.server
    self.port = tonumber(content:match('"port"%s*:%s*(%d+)')) or self.port
    self.slot_name = content:match('"slot_name"%s*:%s*"([^"]*)"') or self.slot_name
    self.password = content:match('"password"%s*:%s*"([^"]*)"') or self.password
    self.autoconnect = content:match('"autoconnect"%s*:%s*(true)') ~= nil

    return true
end

return Config
