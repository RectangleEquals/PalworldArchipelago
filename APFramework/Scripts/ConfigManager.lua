--[[
    Palworld Archipelago Framework
    Module: ConfigManager

    Handles loading, validation, and management of framework configuration.
    Supports default values, user overrides, and JSON config files.
]]

local ConfigManager = {}

-- Module state
ConfigManager.initialized = false
ConfigManager.config = {}
ConfigManager.defaults = {}
ConfigManager.config_file = "APFramework/config.json"

---Default configuration values
ConfigManager.defaults = {
    debug = false,
    capability_file = "APCapabilities.json",
    seed_file = nil,
    save_state_file = "APSaveState.json",
    framework_dir = "APFramework",

    -- Discovery settings
    discovery = {
        auto_discover = true,
        registry_file = "ap_registry.json",
        adapter_dir = "adapters",
        ue4ss_mods_dir = "../"
    },

    -- AP Client settings
    ap_client = {
        auto_reconnect = true,
        reconnect_delay = 5,
        connection_timeout = 30
    },

    -- Event settings
    events = {
        max_event_history = 100,
        enable_event_logging = false
    },

    -- Performance settings
    performance = {
        location_check_batch_size = 10,
        location_check_interval = 0.5,
        save_state_interval = 60
    }
}

---Initialize the configuration manager
---@param initial_config table|nil Initial configuration to merge with defaults
---@return boolean success
function ConfigManager:Initialize(initial_config)
    if self.initialized then
        return true
    end

    -- Start with defaults
    self.config = self:DeepCopy(self.defaults)

    -- Merge initial config if provided
    if initial_config then
        self:MergeConfig(self.config, initial_config)
    end

    -- Try to load config file
    self:LoadConfigFile()

    self.initialized = true
    print("[ConfigManager] Initialized")

    if self.config.debug then
        print("[ConfigManager] Debug mode enabled")
    end

    return true
end

---Load configuration from JSON file
---@param file_path string|nil Path to config file (default: config.json)
---@return boolean success
function ConfigManager:LoadConfigFile(file_path)
    file_path = file_path or self.config_file

    local file = io.open(file_path, "r")
    if not file then
        print("[ConfigManager] No config file found at " .. file_path .. ", using defaults")
        return false
    end

    local content = file:read("*all")
    file:close()

    -- Parse JSON
    local json = require("lib.lunajson")
    local success, loaded_config = pcall(json.decode, content)

    if not success then
        print("[ConfigManager] Error parsing config file: " .. tostring(loaded_config))
        return false
    end

    -- Merge loaded config with current config
    self:MergeConfig(self.config, loaded_config)

    print("[ConfigManager] Loaded config from " .. file_path)
    return true
end

---Save current configuration to JSON file
---@param file_path string|nil Path to save config (default: config.json)
---@return boolean success
function ConfigManager:SaveConfigFile(file_path)
    file_path = file_path or self.config_file

    local json = require("lib.lunajson")
    local success, json_string = pcall(json.encode, self.config)

    if not success then
        print("[ConfigManager] Error encoding config to JSON: " .. tostring(json_string))
        return false
    end

    local file = io.open(file_path, "w")
    if not file then
        print("[ConfigManager] Error opening file for writing: " .. file_path)
        return false
    end

    file:write(json_string)
    file:close()

    print("[ConfigManager] Saved config to " .. file_path)
    return true
end

---Get a configuration value
---@param key_path string Dot-separated path (e.g., "discovery.auto_discover")
---@param default any|nil Default value if key not found
---@return any value Configuration value
function ConfigManager:Get(key_path, default)
    local keys = {}
    for key in string.gmatch(key_path, "[^.]+") do
        table.insert(keys, key)
    end

    local value = self.config
    for _, key in ipairs(keys) do
        if type(value) ~= "table" or value[key] == nil then
            return default
        end
        value = value[key]
    end

    return value
end

---Set a configuration value
---@param key_path string Dot-separated path (e.g., "discovery.auto_discover")
---@param value any Value to set
---@return boolean success
function ConfigManager:Set(key_path, value)
    local keys = {}
    for key in string.gmatch(key_path, "[^.]+") do
        table.insert(keys, key)
    end

    if #keys == 0 then
        print("[ConfigManager] Error: Invalid key path")
        return false
    end

    local current = self.config
    for i = 1, #keys - 1 do
        local key = keys[i]
        if type(current[key]) ~= "table" then
            current[key] = {}
        end
        current = current[key]
    end

    current[keys[#keys]] = value
    return true
end

---Merge source config into target config (deep merge)
---@param target table Target configuration table
---@param source table Source configuration table
function ConfigManager:MergeConfig(target, source)
    for key, value in pairs(source) do
        if type(value) == "table" and type(target[key]) == "table" then
            -- Recursively merge tables
            self:MergeConfig(target[key], value)
        else
            -- Overwrite value
            target[key] = value
        end
    end
end

---Deep copy a table
---@param original table Table to copy
---@return table copy Deep copy of table
function ConfigManager:DeepCopy(original)
    local copy
    if type(original) == "table" then
        copy = {}
        for key, value in pairs(original) do
            copy[key] = self:DeepCopy(value)
        end
    else
        copy = original
    end
    return copy
end

---Validate configuration
---@return boolean valid
---@return string|nil error_message
function ConfigManager:Validate()
    -- Check required fields
    if not self.config.framework_dir then
        return false, "Missing framework_dir"
    end

    if not self.config.capability_file then
        return false, "Missing capability_file"
    end

    -- Validate types
    if type(self.config.debug) ~= "boolean" then
        return false, "debug must be boolean"
    end

    if self.config.discovery then
        if type(self.config.discovery.auto_discover) ~= "boolean" then
            return false, "discovery.auto_discover must be boolean"
        end
    end

    return true
end

---Reset configuration to defaults
function ConfigManager:Reset()
    self.config = self:DeepCopy(self.defaults)
    print("[ConfigManager] Reset to defaults")
end

---Get all configuration
---@return table config Full configuration table
function ConfigManager:GetAll()
    return self.config
end

---Get default configuration
---@return table defaults Default configuration table
function ConfigManager:GetDefaults()
    return self:DeepCopy(self.defaults)
end

---Print current configuration
function ConfigManager:PrintConfig()
    print("[ConfigManager] === Current Configuration ===")
    self:PrintTable(self.config, "  ")
    print("[ConfigManager] ================================")
end

---Recursively print table
---@param tbl table Table to print
---@param indent string Indentation string
function ConfigManager:PrintTable(tbl, indent)
    indent = indent or ""
    for key, value in pairs(tbl) do
        if type(value) == "table" then
            print(string.format("%s%s:", indent, key))
            self:PrintTable(value, indent .. "  ")
        else
            print(string.format("%s%s: %s", indent, key, tostring(value)))
        end
    end
end

return ConfigManager
