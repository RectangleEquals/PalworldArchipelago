--[[
    Palworld Archipelago Framework
    Module: StateManager

    Manages persistent state for AP progression, including:
    - Checked locations
    - Received items
    - Seed metadata
    - Per-mod state isolation
]]

local StateManager = {}

-- Module state
StateManager.initialized = false
StateManager.state_file = nil
StateManager.state = {
    version = "1.0.0",
    seed_metadata = {},
    locations_checked = {},
    items_received = {},
    mod_states = {},
    connection_info = {},
    timestamps = {}
}

---Initialize the state manager
---@param state_file_path string Path to save state file
---@return boolean success
function StateManager:Initialize(state_file_path)
    if self.initialized then
        return true
    end

    self.state_file = state_file_path

    -- Initialize empty state
    self:ResetState()

    self.initialized = true
    print("[StateManager] Initialized with file: " .. state_file_path)

    return true
end

---Reset state to defaults
function StateManager:ResetState()
    self.state = {
        version = "1.0.0",
        seed_metadata = {},
        locations_checked = {},
        items_received = {},
        mod_states = {},
        connection_info = {},
        timestamps = {
            created = os.time(),
            last_modified = os.time()
        }
    }
end

---Save state to file
---@return boolean success
function StateManager:Save()
    if not self.initialized then
        print("[StateManager] Error: Not initialized")
        return false
    end

    -- Update timestamp
    self.state.timestamps.last_modified = os.time()

    -- Serialize to JSON
    local json = require("lib.lunajson")
    local success, json_string = pcall(json.encode, self.state)

    if not success then
        print("[StateManager] Error encoding state to JSON: " .. tostring(json_string))
        return false
    end

    -- Write to file
    local file = io.open(self.state_file, "w")
    if not file then
        print("[StateManager] Error opening file for writing: " .. self.state_file)
        return false
    end

    file:write(json_string)
    file:close()

    print("[StateManager] State saved successfully")
    return true
end

---Load state from file
---@return boolean success
function StateManager:Load()
    if not self.initialized then
        print("[StateManager] Error: Not initialized")
        return false
    end

    local file = io.open(self.state_file, "r")
    if not file then
        print("[StateManager] No existing state file found")
        return false
    end

    local content = file:read("*all")
    file:close()

    -- Parse JSON
    local json = require("lib.lunajson")
    local success, loaded_state = pcall(json.decode, content)

    if not success then
        print("[StateManager] Error parsing state file: " .. tostring(loaded_state))
        return false
    end

    -- Validate version compatibility
    if loaded_state.version ~= self.state.version then
        print(string.format("[StateManager] Warning: State version mismatch (file: %s, current: %s)",
              loaded_state.version, self.state.version))
    end

    -- Load state
    self.state = loaded_state

    print("[StateManager] State loaded successfully")
    print(string.format("[StateManager] Locations checked: %d, Items received: %d",
          self:GetLocationCheckCount(), self:GetItemReceivedCount()))

    return true
end

---Set seed metadata
---@param metadata table Seed metadata
function StateManager:SetSeedMetadata(metadata)
    self.state.seed_metadata = metadata
end

---Get seed metadata
---@return table metadata Seed metadata
function StateManager:GetSeedMetadata()
    return self.state.seed_metadata
end

---Record a location check
---@param location_id integer AP location ID
function StateManager:RecordLocationChecked(location_id)
    if not self.state.locations_checked then
        self.state.locations_checked = {}
    end

    -- Store as key for fast lookup
    self.state.locations_checked[tostring(location_id)] = true
end

---Check if location has been checked
---@param location_id integer AP location ID
---@return boolean checked
function StateManager:IsLocationChecked(location_id)
    if not self.state.locations_checked then
        return false
    end

    return self.state.locations_checked[tostring(location_id)] == true
end

---Get all checked location IDs
---@return table location_ids Array of location IDs
function StateManager:GetCheckedLocations()
    local locations = {}
    if self.state.locations_checked then
        for location_id_str, _ in pairs(self.state.locations_checked) do
            table.insert(locations, tonumber(location_id_str))
        end
    end
    return locations
end

---Get count of checked locations
---@return integer count
function StateManager:GetLocationCheckCount()
    local count = 0
    if self.state.locations_checked then
        for _ in pairs(self.state.locations_checked) do
            count = count + 1
        end
    end
    return count
end

---Record an item received
---@param item_data table Item data (item_id, item_name, player, etc.)
function StateManager:RecordItemReceived(item_data)
    if not self.state.items_received then
        self.state.items_received = {}
    end

    -- Add timestamp
    item_data.timestamp = os.time()

    table.insert(self.state.items_received, item_data)
end

---Get all received items
---@return table items Array of item data
function StateManager:GetReceivedItems()
    return self.state.items_received or {}
end

---Get count of received items
---@return integer count
function StateManager:GetItemReceivedCount()
    if not self.state.items_received then
        return 0
    end
    return #self.state.items_received
end

---Set mod-specific state
---@param mod_id string Mod identifier
---@param mod_state table State data for the mod
function StateManager:SetModState(mod_id, mod_state)
    if not self.state.mod_states then
        self.state.mod_states = {}
    end

    self.state.mod_states[mod_id] = mod_state
end

---Get mod-specific state
---@param mod_id string Mod identifier
---@return table|nil state Mod state or nil if not found
function StateManager:GetModState(mod_id)
    if not self.state.mod_states then
        return nil
    end

    return self.state.mod_states[mod_id]
end

---Get all mod states
---@return table states Map of mod_id -> state
function StateManager:GetAllModStates()
    return self.state.mod_states or {}
end

---Set connection info
---@param connection_info table Connection information
function StateManager:SetConnectionInfo(connection_info)
    self.state.connection_info = connection_info
end

---Get connection info
---@return table connection_info Connection information
function StateManager:GetConnectionInfo()
    return self.state.connection_info or {}
end

---Clear all state (but keep version)
function StateManager:Clear()
    local version = self.state.version
    self:ResetState()
    self.state.version = version
    print("[StateManager] State cleared")
end

---Export state to table (for backup/debugging)
---@return table state Full state table
function StateManager:Export()
    return self.state
end

---Import state from table
---@param imported_state table State to import
---@return boolean success
function StateManager:Import(imported_state)
    if type(imported_state) ~= "table" then
        print("[StateManager] Error: Imported state must be a table")
        return false
    end

    -- Validate version
    if imported_state.version and imported_state.version ~= self.state.version then
        print(string.format("[StateManager] Warning: Importing state with different version (%s)",
              imported_state.version))
    end

    self.state = imported_state
    print("[StateManager] State imported successfully")

    return true
end

---Get state statistics
---@return table stats State statistics
function StateManager:GetStatistics()
    return {
        version = self.state.version,
        locations_checked = self:GetLocationCheckCount(),
        items_received = self:GetItemReceivedCount(),
        mods_with_state = self:CountModsWithState(),
        created = self.state.timestamps.created,
        last_modified = self.state.timestamps.last_modified
    }
end

---Count mods with saved state
---@return integer count
function StateManager:CountModsWithState()
    local count = 0
    if self.state.mod_states then
        for _ in pairs(self.state.mod_states) do
            count = count + 1
        end
    end
    return count
end

---Print state statistics
function StateManager:PrintStatistics()
    local stats = self:GetStatistics()

    print("[StateManager] === State Statistics ===")
    print(string.format("  Version: %s", stats.version))
    print(string.format("  Locations Checked: %d", stats.locations_checked))
    print(string.format("  Items Received: %d", stats.items_received))
    print(string.format("  Mods with State: %d", stats.mods_with_state))

    if stats.created then
        print(string.format("  Created: %s", os.date("%Y-%m-%d %H:%M:%S", stats.created)))
    end

    if stats.last_modified then
        print(string.format("  Last Modified: %s", os.date("%Y-%m-%d %H:%M:%S", stats.last_modified)))
    end

    print("[StateManager] ========================")
end

return StateManager
