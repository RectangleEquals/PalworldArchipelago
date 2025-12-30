--[[
    Palworld Archipelago Framework
    Module: ModRegistry

    Handles discovery, registration, and validation of AP-compatible mods.
    Supports multiple discovery methods: config files, adapters, registry, and direct registration.
]]

local ModRegistry = {}

-- Registry state
ModRegistry.mods = {}
ModRegistry.mod_lookup = {}
ModRegistry.initialization_order = {}
ModRegistry.initialized = false
ModRegistry.mods_dir = nil

-- Discovery configuration
ModRegistry.discovery_config = {
    ue4ss_mods_dir = "../",  -- Relative to APFramework
    ap_registry_file = "ap_registry.json",
    adapter_dir = "adapters",
    auto_discover = true
}

---Initialize the mod registry
---@param mods_directory string|nil Directory containing AP mods
---@return boolean success
function ModRegistry:Initialize(mods_directory)
    if self.initialized then
        return true
    end

    self.mods_dir = mods_directory or "Mods/APFramework/Mods"
    self.initialized = true

    print("[ModRegistry] Initialized with directory: " .. self.mods_dir)
    return true
end

---Discover all AP-compatible mods across multiple locations
---@return table mods Array of discovered mod configs
function ModRegistry:DiscoverMods()
    print("[ModRegistry] Starting mod discovery...")

    local discovered = {}

    -- Method 1: Check central registry
    local registry_mods = self:DiscoverFromRegistry()
    for _, mod in ipairs(registry_mods) do
        table.insert(discovered, mod)
    end

    -- Method 2: Scan for ap_config.json files
    if self.discovery_config.auto_discover then
        local config_mods = self:DiscoverByConfigFile()
        for _, mod in ipairs(config_mods) do
            table.insert(discovered, mod)
        end
    end

    -- Method 3: Load adapters for known mods
    local adapter_mods = self:DiscoverViaAdapters()
    for _, mod in ipairs(adapter_mods) do
        table.insert(discovered, mod)
    end

    -- Method 4: Mods that called RegisterMod directly are already in self.mods

    print(string.format("[ModRegistry] Discovered %d mods", #discovered))

    -- Register all discovered mods
    for _, mod in ipairs(discovered) do
        self:RegisterMod(mod)
    end

    return self.mods
end

---Discover mods listed in ap_registry.json
---@return table mods Array of mod configs
function ModRegistry:DiscoverFromRegistry()
    local registry_path = self.discovery_config.ap_registry_file
    local file = io.open(registry_path, "r")

    if not file then
        print("[ModRegistry] No registry file found at " .. registry_path)
        return {}
    end

    local content = file:read("*all")
    file:close()

    local json = require("lib.lunajson")
    local success, registry = pcall(json.decode, content)

    if not success then
        print("[ModRegistry] Error parsing registry: " .. tostring(registry))
        return {}
    end

    local mods = {}
    for _, entry in ipairs(registry.registered_mods or {}) do
        local mod = self:LoadModFromEntry(entry)
        if mod then
            table.insert(mods, mod)
        end
    end

    print(string.format("[ModRegistry] Found %d mods in registry", #mods))
    return mods
end

---Load mod from a registry entry
---@param entry table Registry entry
---@return table|nil mod Mod config or nil on error
function ModRegistry:LoadModFromEntry(entry)
    if entry.adapter_script then
        -- Load via adapter
        local adapter_path = entry.adapter_script
        local success, adapter = pcall(require, adapter_path:gsub("%.lua$", ""):gsub("/", "."))

        if success and adapter.IsModPresent and adapter.IsModPresent() then
            return adapter.CreateCapabilities()
        end
    elseif entry.ap_config_path then
        -- Load via config file
        local config_path = string.format("%s/%s/%s",
            self.discovery_config.ue4ss_mods_dir,
            entry.mod_folder,
            entry.ap_config_path
        )

        return self:LoadModFromConfig(config_path, entry.mod_folder)
    end

    return nil
end

---Scan UE4SS mods directory for ap_config.json files
---@return table mods Array of mod configs
function ModRegistry:DiscoverByConfigFile()
    local mods_dir = self.discovery_config.ue4ss_mods_dir
    local mods = {}

    -- Get list of all mod folders
    local mod_folders = self:GetDirectories(mods_dir)

    for _, folder in ipairs(mod_folders) do
        local config_path = string.format("%s/%s/ap_config.json", mods_dir, folder)

        if self:FileExists(config_path) then
            print(string.format("[ModRegistry] Found AP config in: %s", folder))

            local mod = self:LoadModFromConfig(config_path, folder)
            if mod then
                table.insert(mods, mod)
            end
        end
    end

    print(string.format("[ModRegistry] Found %d mods via config files", #mods))
    return mods
end

---Load mod from ap_config.json
---@param config_path string Path to ap_config.json
---@param folder_name string Mod folder name
---@return table|nil mod Mod config or nil on error
function ModRegistry:LoadModFromConfig(config_path, folder_name)
    local file = io.open(config_path, "r")
    if not file then
        print("[ModRegistry] Cannot open config: " .. config_path)
        return nil
    end

    local content = file:read("*all")
    file:close()

    local json = require("lib.lunajson")
    local success, config = pcall(json.decode, content)

    if not success then
        print("[ModRegistry] Error parsing config: " .. tostring(config))
        return nil
    end

    if not config.ap_enabled then
        print("[ModRegistry] AP not enabled for " .. folder_name)
        return nil
    end

    -- If mod has integration script, load it
    if config.capabilities and config.capabilities.integration_script then
        local script_path = string.format("%s/%s/%s",
            self.discovery_config.ue4ss_mods_dir,
            folder_name,
            config.capabilities.integration_script
        )

        if self:FileExists(script_path) then
            local success, integration = pcall(dofile, script_path)
            if success then
                if integration.GetCapabilities then
                    return integration.GetCapabilities()
                end
            else
                print("[ModRegistry] Error loading integration script: " .. tostring(integration))
            end
        end
    end

    -- Otherwise, use automatic discovery based on config
    return self:BuildCapabilitiesFromConfig(config, folder_name)
end

---Build capabilities automatically from config
---@param config table AP config
---@param folder_name string Mod folder name
---@return table capabilities Capability configuration
function ModRegistry:BuildCapabilitiesFromConfig(config, folder_name)
    local capabilities = {
        mod_info = {
            id = config.mod_id or folder_name:lower():gsub("[^a-z0-9_]", "_"),
            name = config.mod_name or folder_name,
            version = config.version or "1.0.0",
            author = config.author or "Unknown"
        },
        capabilities = {}
    }

    -- Auto-discover based on config instructions
    if config.capabilities and config.capabilities.locations then
        for _, loc_config in ipairs(config.capabilities.locations) do
            if loc_config.discovery_method == "scan_actors" then
                -- In a real implementation, this would scan game world
                -- For now, placeholder
                capabilities.capabilities.locations = {}
            end
        end
    end

    return capabilities
end

---Load mods via adapter scripts
---@return table mods Array of mod configs
function ModRegistry:DiscoverViaAdapters()
    local adapter_dir = self.discovery_config.adapter_dir
    local mods = {}

    -- Get all adapter scripts
    local adapters = self:GetLuaFiles(adapter_dir)

    for _, adapter_file in ipairs(adapters) do
        local adapter_name = adapter_file:gsub("%.lua$", "")
        local adapter_path = "" .. adapter_dir .. "." .. adapter_name

        print(string.format("[ModRegistry] Loading adapter: %s", adapter_file))

        local success, adapter = pcall(require, adapter_path)
        if success and adapter.IsModPresent and adapter.IsModPresent() then
            local capabilities = adapter.CreateCapabilities()
            if capabilities then
                table.insert(mods, capabilities)
                print(string.format("[ModRegistry] Adapter loaded: %s", adapter_file))
            end
        elseif success then
            print(string.format("[ModRegistry] Adapter %s loaded but mod not present", adapter_file))
        else
            print(string.format("[ModRegistry] Error loading adapter %s: %s", adapter_file, tostring(adapter)))
        end
    end

    print(string.format("[ModRegistry] Found %d mods via adapters", #mods))
    return mods
end

---Register a mod with the framework
---@param mod_config table Mod capability configuration
---@return boolean success
function ModRegistry:RegisterMod(mod_config)
    -- Validate mod config structure
    local valid, error_msg = self:ValidateModConfig(mod_config)
    if not valid then
        print(string.format("[ModRegistry] Invalid mod config: %s", error_msg))
        return false
    end

    local mod_id = mod_config.mod_info.id

    -- Check for duplicate registration
    if self.mod_lookup[mod_id] then
        print(string.format("[ModRegistry] Warning: Mod '%s' already registered, skipping", mod_id))
        return false
    end

    -- Initialize mod callbacks if not present
    if not mod_config.callbacks then
        mod_config.callbacks = {}
    end

    -- Store mod
    table.insert(self.mods, mod_config)
    self.mod_lookup[mod_id] = mod_config

    -- Track initialization order
    local order = mod_config.runtime_requirements and
                  mod_config.runtime_requirements.initialization_order or 0
    table.insert(self.initialization_order, {
        id = mod_id,
        order = order
    })

    print(string.format("[ModRegistry] Registered mod: %s v%s",
          mod_config.mod_info.name, mod_config.mod_info.version))

    return true
end

---Validate mod configuration structure
---@param mod_config table Mod config to validate
---@return boolean valid
---@return string|nil error_message
function ModRegistry:ValidateModConfig(mod_config)
    -- Check required top-level fields
    if not mod_config.mod_info then
        return false, "Missing mod_info"
    end

    if not mod_config.capabilities then
        return false, "Missing capabilities"
    end

    -- Validate mod_info
    local info = mod_config.mod_info
    if not info.id or type(info.id) ~= "string" then
        return false, "Invalid or missing mod_info.id"
    end

    if not info.name or type(info.name) ~= "string" then
        return false, "Invalid or missing mod_info.name"
    end

    if not info.version or type(info.version) ~= "string" then
        return false, "Invalid or missing mod_info.version"
    end

    -- Validate ID format (snake_case alphanumeric)
    if not info.id:match("^[a-z0-9_]+$") then
        return false, "mod_info.id must be snake_case alphanumeric"
    end

    -- Validate version format (semver)
    if not info.version:match("^%d+%.%d+%.%d+$") then
        return false, "mod_info.version must be semantic version (X.Y.Z)"
    end

    return true
end

---Validate dependencies between registered mods
---@return table valid_mods Array of mods that passed validation
function ModRegistry:ValidateDependencies()
    print("[ModRegistry] Validating mod dependencies...")

    local valid_mods = {}
    local invalid_mods = {}

    for _, mod in ipairs(self.mods) do
        local valid, error_msg = self:ValidateModDependencies(mod)
        if valid then
            table.insert(valid_mods, mod)
        else
            table.insert(invalid_mods, {
                mod = mod,
                error = error_msg
            })
        end
    end

    -- Report invalid mods
    if #invalid_mods > 0 then
        print(string.format("[ModRegistry] %d mods failed dependency validation:", #invalid_mods))
        for _, invalid in ipairs(invalid_mods) do
            print(string.format("  - %s: %s", invalid.mod.mod_info.name, invalid.error))
        end
    end

    -- Sort by initialization order
    self:SortByInitializationOrder(valid_mods)

    return valid_mods
end

---Validate a single mod's dependencies
---@param mod table Mod config to validate
---@return boolean valid
---@return string|nil error_message
function ModRegistry:ValidateModDependencies(mod)
    if not mod.dependencies then
        return true -- No dependencies to check
    end

    local deps = mod.dependencies

    -- Check required dependencies
    if deps.requires then
        for _, required in ipairs(deps.requires) do
            local dep_mod = self.mod_lookup[required.mod_id]
            if not dep_mod then
                return false, string.format("Missing required dependency: %s", required.mod_id)
            end

            -- Check version if specified
            if required.min_version then
                if not self:VersionCompatible(dep_mod.mod_info.version, required.min_version) then
                    return false, string.format(
                        "Dependency version mismatch: %s requires >= %s, found %s",
                        required.mod_id, required.min_version, dep_mod.mod_info.version
                    )
                end
            end
        end
    end

    -- Check conflicts
    if deps.conflicts then
        for _, conflict_id in ipairs(deps.conflicts) do
            if self.mod_lookup[conflict_id] then
                return false, string.format("Conflict with mod: %s", conflict_id)
            end
        end
    end

    return true
end

---Check if version meets minimum requirement
---@param current string Current version (X.Y.Z)
---@param required string Required version (X.Y.Z)
---@return boolean compatible
function ModRegistry:VersionCompatible(current, required)
    local function parse_version(v)
        local major, minor, patch = v:match("^(%d+)%.(%d+)%.(%d+)$")
        return {
            tonumber(major) or 0,
            tonumber(minor) or 0,
            tonumber(patch) or 0
        }
    end

    local curr = parse_version(current)
    local req = parse_version(required)

    -- Compare major.minor.patch
    for i = 1, 3 do
        if curr[i] > req[i] then
            return true
        elseif curr[i] < req[i] then
            return false
        end
    end

    return true -- Versions are equal
end

---Sort mods by initialization order
---@param mods table Array of mods to sort
function ModRegistry:SortByInitializationOrder(mods)
    table.sort(mods, function(a, b)
        local order_a = a.runtime_requirements and
                       a.runtime_requirements.initialization_order or 0
        local order_b = b.runtime_requirements and
                       b.runtime_requirements.initialization_order or 0

        -- Higher order = initialized first
        return order_a > order_b
    end)
end

---Helper: Check if file exists
---@param path string File path
---@return boolean exists
function ModRegistry:FileExists(path)
    local file = io.open(path, "r")
    if file then
        file:close()
        return true
    end
    return false
end

---Helper: Get all directories in a path (Windows)
---@param path string Directory path
---@return table directories Array of directory names
function ModRegistry:GetDirectories(path)
    local directories = {}

    -- Windows command
    local handle = io.popen('dir "' .. path .. '" /b /ad 2>nul')
    if handle then
        for dir in handle:lines() do
            if dir ~= "." and dir ~= ".." then
                table.insert(directories, dir)
            end
        end
        handle:close()
    end

    return directories
end

---Helper: Get all .lua files in a directory (Windows)
---@param path string Directory path
---@return table files Array of .lua filenames
function ModRegistry:GetLuaFiles(path)
    local files = {}

    -- Windows command
    local handle = io.popen('dir "' .. path .. '\\*.lua" /b 2>nul')
    if handle then
        for file in handle:lines() do
            table.insert(files, file)
        end
        handle:close()
    end

    return files
end

---Get all registered mods
---@return table mods Array of mod configs
function ModRegistry:GetMods()
    return self.mods
end

---Get mod by ID
---@param mod_id string Mod identifier
---@return table|nil mod Mod config or nil if not found
function ModRegistry:GetMod(mod_id)
    return self.mod_lookup[mod_id]
end

---Get all mod IDs
---@return table ids Array of mod IDs
function ModRegistry:GetModIds()
    local ids = {}
    for id, _ in pairs(self.mod_lookup) do
        table.insert(ids, id)
    end
    return ids
end

---Check if a mod is registered
---@param mod_id string Mod identifier
---@return boolean registered
function ModRegistry:IsModRegistered(mod_id)
    return self.mod_lookup[mod_id] ~= nil
end

---Get mods by capability type
---@param capability_type string Capability type (e.g., "locations", "items")
---@return table mods Array of mods with specified capability
function ModRegistry:GetModsByCapability(capability_type)
    local result = {}

    for _, mod in ipairs(self.mods) do
        if mod.capabilities[capability_type] and
           #mod.capabilities[capability_type] > 0 then
            table.insert(result, mod)
        end
    end

    return result
end

---Get total count of a capability type across all mods
---@param capability_type string Capability type
---@return integer count
function ModRegistry:GetCapabilityCount(capability_type)
    local count = 0

    for _, mod in ipairs(self.mods) do
        if mod.capabilities[capability_type] then
            count = count + #mod.capabilities[capability_type]
        end
    end

    return count
end

---Call initialization callbacks for all mods
function ModRegistry:InitializeMods()
    print("[ModRegistry] Initializing mods...")

    for _, mod in ipairs(self.mods) do
        if mod.callbacks.onInit then
            local success, error_msg = pcall(mod.callbacks.onInit)
            if not success then
                print(string.format("[ModRegistry] Error initializing %s: %s",
                      mod.mod_info.name, error_msg))
            else
                print(string.format("[ModRegistry] Initialized: %s", mod.mod_info.name))
            end
        end
    end

    print("[ModRegistry] Mod initialization complete")
end

---Generate registry statistics
---@return table stats Registry statistics
function ModRegistry:GetStatistics()
    return {
        total_mods = #self.mods,
        location_count = self:GetCapabilityCount("locations"),
        item_count = self:GetCapabilityCount("items"),
        region_count = self:GetCapabilityCount("regions"),
        mods_by_capability = {
            locations = #self:GetModsByCapability("locations"),
            items = #self:GetModsByCapability("items"),
            regions = #self:GetModsByCapability("regions")
        }
    }
end

---Print registry statistics
function ModRegistry:PrintStatistics()
    local stats = self:GetStatistics()

    print("[ModRegistry] === Registry Statistics ===")
    print(string.format("  Total Mods: %d", stats.total_mods))
    print(string.format("  Total Locations: %d", stats.location_count))
    print(string.format("  Total Items: %d", stats.item_count))
    print(string.format("  Total Regions: %d", stats.region_count))
    print(string.format("  Mods with Locations: %d", stats.mods_by_capability.locations))
    print(string.format("  Mods with Items: %d", stats.mods_by_capability.items))
    print(string.format("  Mods with Regions: %d", stats.mods_by_capability.regions))
    print("[ModRegistry] ==============================")
end

return ModRegistry
