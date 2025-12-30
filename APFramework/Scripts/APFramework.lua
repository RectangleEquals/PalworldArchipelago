--[[
    Palworld Archipelago Framework
    Main Entry Point

    This is the central coordination module for the AP framework.
    It manages mod discovery, capability aggregation, and runtime coordination.
]]

local APFramework = {}

-- Version and metadata
APFramework.VERSION = "1.0.0"
APFramework.COMPATIBLE_VERSIONS = {"1.0.0"}

-- Framework state
APFramework.State = {
    initialized = false,
    mods_loaded = false,
    seed_loaded = false,
    ap_connected = false
}

-- Configuration
APFramework.Config = {
    debug = true,
    capability_file = "APCapabilities.json",
    seed_file = nil, -- Set by player when loading seed
    save_state_file = "APSaveState.json",
    framework_dir = "APFramework"
}

-- Module imports (lazy loaded)
local ModRegistry = nil
local APClient = nil
local EventBus = nil
local CapabilityManager = nil
local StateManager = nil
local ConfigManager = nil

---Initialize the framework
---@return boolean success
function APFramework:Initialize()
    if self.State.initialized then
        self:Log("Warning: Framework already initialized")
        return true
    end

    self:Log("Initializing Palworld Archipelago Framework v" .. self.VERSION)

    -- Load core modules
    local success = self:LoadCoreModules()
    if not success then
        self:LogError("Failed to load core modules")
        return false
    end

    -- Initialize configuration
    success = ConfigManager:Initialize(self.Config)
    if not success then
        self:LogError("Failed to initialize configuration")
        return false
    end

    -- Initialize event bus
    success = EventBus:Initialize()
    if not success then
        self:LogError("Failed to initialize event bus")
        return false
    end

    -- Initialize capability manager
    success = CapabilityManager:Initialize()
    if not success then
        self:LogError("Failed to initialize capability manager")
        return false
    end

    -- Initialize mod registry
    success = ModRegistry:Initialize(self.Config.framework_dir)
    if not success then
        self:LogError("Failed to initialize mod registry")
        return false
    end

    -- Initialize state manager
    success = StateManager:Initialize(self.Config.save_state_file)
    if not success then
        self:LogError("Failed to initialize state manager")
        return false
    end

    self.State.initialized = true
    self:Log("Framework initialized successfully")

    -- Fire initialization event
    EventBus:Fire("FrameworkInitialized")

    return true
end

---Load core framework modules
---@return boolean success
function APFramework:LoadCoreModules()
    local function safe_require(module_name)
        local success, result = pcall(require, module_name)
        if not success then
            self:LogError(string.format("Failed to load module '%s': %s", module_name, result))
            return nil
        end
        return result
    end

    EventBus = safe_require("EventBus")
    if not EventBus then return false end

    ConfigManager = safe_require("ConfigManager")
    if not ConfigManager then return false end

    StateManager = safe_require("StateManager")
    if not StateManager then return false end

    CapabilityManager = safe_require("CapabilityManager")
    if not CapabilityManager then return false end

    ModRegistry = safe_require("ModRegistry")
    if not ModRegistry then return false end

    -- APClient is optional for now (will be implemented in Phase 2)
    APClient = safe_require("APClient")
    -- Don't fail if APClient isn't implemented yet

    self:Log("Core modules loaded successfully")
    return true
end

---Discover and register all AP-compatible mods
---@return boolean success
function APFramework:DiscoverMods()
    if not self.State.initialized then
        self:LogError("Framework not initialized")
        return false
    end

    self:Log("Discovering AP-compatible mods...")

    local mods = ModRegistry:DiscoverMods()
    self:Log(string.format("Discovered %d AP-compatible mods", #mods))

    -- Validate mod dependencies
    local valid_mods = ModRegistry:ValidateDependencies()
    self:Log(string.format("Validated %d mods (passed dependency checks)", #valid_mods))

    self.State.mods_loaded = true
    EventBus:Fire("ModsDiscovered", {mods = valid_mods})

    return true
end

---Generate capability manifest from registered mods
---@return table|nil capabilities Capability manifest or nil on error
function APFramework:GenerateCapabilityManifest()
    if not self.State.mods_loaded then
        self:LogError("Mods not loaded yet")
        return nil
    end

    self:Log("Generating capability manifest...")

    -- Query each mod for capabilities
    EventBus:Fire("CapabilityQuery")

    -- Aggregate capabilities
    local mods = ModRegistry:GetMods()
    local manifest = CapabilityManager:AggregateCapabilities(mods)
    if not manifest then
        self:LogError("Failed to aggregate capabilities")
        return nil
    end

    -- Validate manifest
    local valid, error_msg = CapabilityManager:ValidateManifest(manifest)
    if not valid then
        self:LogError("Capability manifest validation failed: " .. tostring(error_msg))
        return nil
    end

    -- Save to file
    local success = CapabilityManager:SaveManifest(manifest, self.Config.capability_file)
    if not success then
        self:LogError("Failed to save capability manifest")
        return nil
    end

    self:Log(string.format("Capability manifest saved to %s", self.Config.capability_file))
    CapabilityManager:PrintManifestSummary(manifest)

    return manifest
end

---Load seed data and distribute to mods
---@param seed_file string Path to .appalworld seed file
---@return boolean success
function APFramework:LoadSeed(seed_file)
    if not self.State.mods_loaded then
        self:LogError("Mods not loaded yet")
        return false
    end

    self:Log(string.format("Loading seed from %s...", seed_file))

    -- Load and parse seed file
    local seed_data = self:LoadSeedFile(seed_file)
    if not seed_data then
        self:LogError("Failed to load seed file")
        return false
    end

    -- Validate seed data
    local valid, error_msg = CapabilityManager:ValidateSeedData(seed_data)
    if not valid then
        self:LogError("Seed data validation failed: " .. tostring(error_msg))
        return false
    end

    -- Distribute seed data to mods
    EventBus:Fire("SeedReceived", {seed_data = seed_data})

    -- Store seed metadata
    if seed_data.metadata then
        StateManager:SetSeedMetadata(seed_data.metadata)
    end

    self.State.seed_loaded = true
    self.Config.seed_file = seed_file

    self:Log("Seed loaded and distributed successfully")
    return true
end

---Connect to Archipelago server
---@param host string Server hostname
---@param port integer Server port
---@param slot string Player slot name
---@param password string|nil Server password
---@return boolean success
function APFramework:ConnectToServer(host, port, slot, password)
    if not APClient then
        self:LogError("APClient not available yet (Phase 2)")
        return false
    end

    if not self.State.seed_loaded then
        self:LogError("Seed not loaded yet")
        return false
    end

    self:Log(string.format("Connecting to AP server at %s:%d...", host, port))

    -- Initialize AP client
    local success = APClient:Initialize(host, port)
    if not success then
        self:LogError("Failed to initialize AP client")
        return false
    end

    -- Authenticate
    local seed_name = StateManager:GetSeedMetadata().seed_name
    success = APClient:Authenticate(slot, password, seed_name)
    if not success then
        self:LogError("Failed to authenticate with AP server")
        return false
    end

    -- Set up event handlers
    self:SetupAPEventHandlers()

    self.State.ap_connected = true
    EventBus:Fire("APConnected")

    self:Log("Connected to AP server successfully")
    return true
end

---Set up handlers for AP server events
function APFramework:SetupAPEventHandlers()
    if not APClient then
        return
    end

    -- Location checked by server
    APClient:OnLocationChecked(function(location_id)
        EventBus:Fire("LocationCheckedRemote", {location_id = location_id})
    end)

    -- Item received from server
    APClient:OnItemReceived(function(item_data)
        EventBus:Fire("ItemReceived", {item = item_data})
        StateManager:RecordItemReceived(item_data)
    end)

    -- Goal completed
    APClient:OnGoalComplete(function()
        EventBus:Fire("GoalComplete")
    end)

    -- Connection lost
    APClient:OnDisconnected(function()
        self.State.ap_connected = false
        EventBus:Fire("APDisconnected")
        self:LogError("Lost connection to AP server")
    end)
end

---Report location check to AP server
---@param location_id integer AP location ID
function APFramework:CheckLocation(location_id)
    if not self.State.ap_connected and APClient then
        self:LogError("Not connected to AP server")
        return
    end

    -- Record in state
    StateManager:RecordLocationChecked(location_id)

    -- Send to server (if connected)
    if APClient and self.State.ap_connected then
        APClient:CheckLocation(location_id)
    end

    -- Fire local event
    EventBus:Fire("LocationChecked", {location_id = location_id})

    self:Log(string.format("Location %d checked", location_id))
end

---Save current AP state
---@return boolean success
function APFramework:SaveState()
    if not self.State.seed_loaded then
        return true -- Nothing to save
    end

    self:Log("Saving AP state...")

    -- Fire save event to let mods save their data
    EventBus:Fire("Save")

    -- Collect mod states
    -- (Mods should return their state via a callback, but for now we'll handle this in Phase 3)

    -- Save framework state
    local success = StateManager:Save()
    if not success then
        self:LogError("Failed to save state")
        return false
    end

    self:Log("State saved successfully")
    return true
end

---Load saved AP state
---@return boolean success
function APFramework:LoadState()
    if not self.State.seed_loaded then
        self:LogError("Seed must be loaded before loading state")
        return false
    end

    self:Log("Loading AP state...")

    local success = StateManager:Load()
    if not success then
        self:Log("No existing state found or failed to load")
        return false
    end

    -- Fire load event to let mods restore their data
    EventBus:Fire("Load", {state = StateManager:GetAllModStates()})

    self:Log("State loaded successfully")
    return true
end

---Load seed file from disk
---@param file_path string Path to seed file
---@return table|nil seed_data Parsed seed data or nil on error
function APFramework:LoadSeedFile(file_path)
    local file = io.open(file_path, "r")
    if not file then
        self:LogError(string.format("Failed to open file: %s", file_path))
        return nil
    end

    local content = file:read("*all")
    file:close()

    -- Parse JSON
    local json = require("lib.lunajson")
    local success, result = pcall(json.decode, content)
    if not success then
        self:LogError(string.format("Failed to parse JSON: %s", result))
        return nil
    end

    return result
end

---Logging utilities
function APFramework:Log(message)
    local prefix = "[APFramework]"
    print(string.format("%s %s", prefix, message))
end

function APFramework:LogError(message)
    local prefix = "[APFramework ERROR]"
    print(string.format("%s %s", prefix, message))
end

function APFramework:LogDebug(message)
    if not self.Config.debug then return end
    local prefix = "[APFramework DEBUG]"
    print(string.format("%s %s", prefix, message))
end

---Public API for mod registration
---@param mod_config table Mod capability configuration
---@return boolean success
function APFramework.RegisterMod(mod_config)
    if not ModRegistry then
        print("[APFramework ERROR] Mod registry not initialized")
        return false
    end

    return ModRegistry:RegisterMod(mod_config)
end

---Public API for subscribing to events
---@param event_name string Event to subscribe to
---@param callback function Callback function
---@param priority integer|nil Priority (higher = called first)
---@return integer subscription_id
function APFramework.Subscribe(event_name, callback, priority)
    if not EventBus then
        print("[APFramework ERROR] Event bus not initialized")
        return -1
    end

    return EventBus:Subscribe(event_name, callback, priority)
end

---Public API for unsubscribing from events
---@param subscription_id integer Subscription ID to remove
function APFramework.Unsubscribe(subscription_id)
    if not EventBus then
        print("[APFramework ERROR] Event bus not initialized")
        return
    end

    EventBus:Unsubscribe(subscription_id)
end

---Public API for firing events (for advanced usage)
---@param event_name string Event to fire
---@param event_data table|nil Event data
function APFramework.FireEvent(event_name, event_data)
    if not EventBus then
        print("[APFramework ERROR] Event bus not initialized")
        return
    end

    EventBus:Fire(event_name, event_data)
end

-- Make framework globally accessible
_G.APFramework = APFramework

return APFramework
