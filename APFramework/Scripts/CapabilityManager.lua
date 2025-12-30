--[[
    Palworld Archipelago Framework
    Module: CapabilityManager

    Aggregates mod capabilities, generates JSON manifests, validates against schema,
    and detects conflicts between mods.
]]

local CapabilityManager = {}

-- Module state
CapabilityManager.initialized = false
CapabilityManager.schema = nil
CapabilityManager.schema_file = "APFramework/schemas/capability_schema.json"

---Initialize the capability manager
---@return boolean success
function CapabilityManager:Initialize()
    if self.initialized then
        return true
    end

    -- Load capability schema
    self:LoadSchema()

    self.initialized = true
    print("[CapabilityManager] Initialized")

    return true
end

---Load capability schema from file
---@return boolean success
function CapabilityManager:LoadSchema()
    local file = io.open(self.schema_file, "r")
    if not file then
        print("[CapabilityManager] Warning: Schema file not found at " .. self.schema_file)
        self.schema = nil
        return false
    end

    local content = file:read("*all")
    file:close()

    local json = require("lib.lunajson")
    local success, schema = pcall(json.decode, content)

    if not success then
        print("[CapabilityManager] Error parsing schema: " .. tostring(schema))
        self.schema = nil
        return false
    end

    self.schema = schema
    print("[CapabilityManager] Schema loaded successfully")

    return true
end

---Aggregate capabilities from all registered mods
---@param mods table Array of mod configurations
---@return table|nil manifest Capability manifest or nil on error
function CapabilityManager:AggregateCapabilities(mods)
    if not mods or #mods == 0 then
        print("[CapabilityManager] No mods to aggregate")
        return nil
    end

    local manifest = {
        version = "1.0.0",
        generated = os.date("%Y-%m-%dT%H:%M:%S"),
        framework_version = "1.0.0",
        mods = {}
    }

    -- Aggregate each mod's capabilities
    for _, mod in ipairs(mods) do
        local mod_entry = {
            mod_info = mod.mod_info,
            dependencies = mod.dependencies or {},
            capabilities = mod.capabilities or {},
            runtime_requirements = mod.runtime_requirements or {}
        }

        table.insert(manifest.mods, mod_entry)
    end

    -- Calculate totals
    manifest.totals = self:CalculateTotals(manifest)

    print(string.format("[CapabilityManager] Aggregated capabilities from %d mods", #mods))

    return manifest
end

---Calculate total counts across all mods
---@param manifest table Capability manifest
---@return table totals Total counts
function CapabilityManager:CalculateTotals(manifest)
    local totals = {
        locations = 0,
        items = 0,
        regions = 0,
        options = 0
    }

    for _, mod in ipairs(manifest.mods) do
        if mod.capabilities.locations then
            totals.locations = totals.locations + #mod.capabilities.locations
        end

        if mod.capabilities.items then
            for _, item in ipairs(mod.capabilities.items) do
                totals.items = totals.items + (item.count or 1)
            end
        end

        if mod.capabilities.regions then
            totals.regions = totals.regions + #mod.capabilities.regions
        end

        if mod.capabilities.options then
            totals.options = totals.options + #mod.capabilities.options
        end
    end

    return totals
end

---Validate manifest against schema
---@param manifest table Capability manifest
---@return boolean valid
---@return string|nil error_message
function CapabilityManager:ValidateManifest(manifest)
    if not manifest then
        return false, "Manifest is nil"
    end

    -- Basic structural validation
    if not manifest.version then
        return false, "Missing version"
    end

    if not manifest.mods then
        return false, "Missing mods array"
    end

    if type(manifest.mods) ~= "table" then
        return false, "mods must be an array"
    end

    -- Validate each mod
    for i, mod in ipairs(manifest.mods) do
        local valid, error_msg = self:ValidateModCapability(mod)
        if not valid then
            return false, string.format("Mod %d validation failed: %s", i, error_msg)
        end
    end

    -- Check for conflicts
    local conflicts = self:DetectConflicts(manifest)
    if #conflicts > 0 then
        local conflict_msg = "Conflicts detected:\n"
        for _, conflict in ipairs(conflicts) do
            conflict_msg = conflict_msg .. string.format("  - %s\n", conflict)
        end
        return false, conflict_msg
    end

    return true
end

---Validate a single mod's capability configuration
---@param mod table Mod capability configuration
---@return boolean valid
---@return string|nil error_message
function CapabilityManager:ValidateModCapability(mod)
    -- Check required fields
    if not mod.mod_info then
        return false, "Missing mod_info"
    end

    if not mod.mod_info.id then
        return false, "Missing mod_info.id"
    end

    if not mod.mod_info.name then
        return false, "Missing mod_info.name"
    end

    if not mod.mod_info.version then
        return false, "Missing mod_info.version"
    end

    -- Validate ID format
    if not string.match(mod.mod_info.id, "^[a-z0-9_]+$") then
        return false, "mod_info.id must be snake_case alphanumeric"
    end

    -- Validate version format
    if not string.match(mod.mod_info.version, "^%d+%.%d+%.%d+$") then
        return false, "mod_info.version must be semantic version (X.Y.Z)"
    end

    -- Validate capabilities structure
    if mod.capabilities then
        if mod.capabilities.locations then
            if type(mod.capabilities.locations) ~= "table" then
                return false, "capabilities.locations must be an array"
            end
        end

        if mod.capabilities.items then
            if type(mod.capabilities.items) ~= "table" then
                return false, "capabilities.items must be an array"
            end
        end

        if mod.capabilities.regions then
            if type(mod.capabilities.regions) ~= "table" then
                return false, "capabilities.regions must be an array"
            end
        end
    end

    return true
end

---Detect conflicts between mods
---@param manifest table Capability manifest
---@return table conflicts Array of conflict descriptions
function CapabilityManager:DetectConflicts(manifest)
    local conflicts = {}

    -- Build mod lookup
    local mod_lookup = {}
    for _, mod in ipairs(manifest.mods) do
        mod_lookup[mod.mod_info.id] = mod
    end

    -- Check each mod's declared conflicts
    for _, mod in ipairs(manifest.mods) do
        if mod.dependencies and mod.dependencies.conflicts then
            for _, conflict_id in ipairs(mod.dependencies.conflicts) do
                if mod_lookup[conflict_id] then
                    table.insert(conflicts, string.format(
                        "Mod '%s' conflicts with '%s'",
                        mod.mod_info.id,
                        conflict_id
                    ))
                end
            end
        end
    end

    -- Check for duplicate IDs in locations
    local location_ids = {}
    for _, mod in ipairs(manifest.mods) do
        if mod.capabilities.locations then
            for _, location in ipairs(mod.capabilities.locations) do
                local full_id = mod.mod_info.id .. ":" .. location.id
                if location_ids[full_id] then
                    table.insert(conflicts, string.format(
                        "Duplicate location ID: %s",
                        full_id
                    ))
                end
                location_ids[full_id] = true
            end
        end
    end

    -- Check for duplicate IDs in items
    local item_ids = {}
    for _, mod in ipairs(manifest.mods) do
        if mod.capabilities.items then
            for _, item in ipairs(mod.capabilities.items) do
                local full_id = mod.mod_info.id .. ":" .. item.id
                if item_ids[full_id] then
                    table.insert(conflicts, string.format(
                        "Duplicate item ID: %s",
                        full_id
                    ))
                end
                item_ids[full_id] = true
            end
        end
    end

    return conflicts
end

---Save manifest to file
---@param manifest table Capability manifest
---@param file_path string Path to save manifest
---@return boolean success
function CapabilityManager:SaveManifest(manifest, file_path)
    local json = require("lib.lunajson")
    local success, json_string = pcall(json.encode, manifest)

    if not success then
        print("[CapabilityManager] Error encoding manifest to JSON: " .. tostring(json_string))
        return false
    end

    local file = io.open(file_path, "w")
    if not file then
        print("[CapabilityManager] Error opening file for writing: " .. file_path)
        return false
    end

    file:write(json_string)
    file:close()

    print("[CapabilityManager] Manifest saved to " .. file_path)
    return true
end

---Load manifest from file
---@param file_path string Path to manifest file
---@return table|nil manifest Capability manifest or nil on error
function CapabilityManager:LoadManifest(file_path)
    local file = io.open(file_path, "r")
    if not file then
        print("[CapabilityManager] Error opening file: " .. file_path)
        return nil
    end

    local content = file:read("*all")
    file:close()

    local json = require("lib.lunajson")
    local success, manifest = pcall(json.decode, content)

    if not success then
        print("[CapabilityManager] Error parsing manifest: " .. tostring(manifest))
        return nil
    end

    return manifest
end

---Validate seed data against capabilities
---@param seed_data table Seed data from .appalworld file
---@return boolean valid
---@return string|nil error_message
function CapabilityManager:ValidateSeedData(seed_data)
    if not seed_data then
        return false, "Seed data is nil"
    end

    -- Check required fields
    if not seed_data.metadata then
        return false, "Missing metadata"
    end

    if not seed_data.slot_data then
        return false, "Missing slot_data"
    end

    -- Validate structure
    if type(seed_data.slot_data) ~= "table" then
        return false, "slot_data must be a table"
    end

    return true
end

---Print manifest summary
---@param manifest table Capability manifest
function CapabilityManager:PrintManifestSummary(manifest)
    if not manifest then
        print("[CapabilityManager] No manifest to print")
        return
    end

    print("[CapabilityManager] === Manifest Summary ===")
    print(string.format("  Version: %s", manifest.version or "unknown"))
    print(string.format("  Framework: %s", manifest.framework_version or "unknown"))
    print(string.format("  Generated: %s", manifest.generated or "unknown"))
    print(string.format("  Mods: %d", #manifest.mods))

    if manifest.totals then
        print("  Totals:")
        print(string.format("    Locations: %d", manifest.totals.locations))
        print(string.format("    Items: %d", manifest.totals.items))
        print(string.format("    Regions: %d", manifest.totals.regions))
        print(string.format("    Options: %d", manifest.totals.options))
    end

    print("  Mods:")
    for _, mod in ipairs(manifest.mods) do
        local loc_count = mod.capabilities.locations and #mod.capabilities.locations or 0
        local item_count = mod.capabilities.items and #mod.capabilities.items or 0
        print(string.format("    - %s v%s (%d locations, %d items)",
              mod.mod_info.name,
              mod.mod_info.version,
              loc_count,
              item_count))
    end

    print("[CapabilityManager] ==========================")
end

return CapabilityManager
