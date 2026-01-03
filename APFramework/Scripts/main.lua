-- APFramework Main Entry Point (IPC Branch)
-- Loads APFrameworkCore.dll and manages framework lifecycle

print("[APFramework] Loading APFramework v2.0.0 (IPC Architecture)...")

local FrameworkWrapper = require("framework_wrapper")
local Config = require("config")
local APClient = require("ap_client")

-- Configuration
local PIPE_NAME = "APFramework_default"
local MODS_DIRECTORY = "ue4ss\\Mods"
local CONFIG_PATH = "ue4ss\\Mods\\APFramework\\framework_config.json"
local LOG_PATH = "ue4ss\\Mods\\APFramework\\framework.log"
local CAPABILITIES_BASE_DIR = "ue4ss\\Mods\\APFramework"
local FRAMEWORK_MOD_ID = "archipelago.palworld.framework"

-- Dynamic capabilities path (set after config loads)
local CAPABILITIES_PATH = nil

-- Initialize logger FIRST (before creating framework)
FrameworkWrapper.init_logger(LOG_PATH)
print("[APFramework] Logger initialized at: " .. LOG_PATH)

-- Global state
local framework = nil
local config = nil
local framework_client = nil  -- Priority client instance
local state = {
    initialized = false,
    ipc_started = false,
    framework_registered = false,  -- NEW: Framework mod registration status
    mods_discovered = false,
    all_registered = false,
    connected = false,
    polling_started = false
}

local registration_start_time = 0

-- Function to build dynamic capabilities filename
local function build_capabilities_filename(slot_name)
    if not slot_name or slot_name == "" then
        print("[APFramework] WARNING: No slot_name configured, using default 'Player1'")
        slot_name = "Player1"
    end

    local filename = "APCapabilities_" .. slot_name .. ".json"
    local full_path = CAPABILITIES_BASE_DIR .. "\\" .. filename

    return full_path, filename
end

-- Initialize framework
local function initialize_framework()
    if state.initialized then return end

    print("[APFramework] Initializing framework...")

    -- Load config
    config = Config:new()
    local config_loaded = config:load(CONFIG_PATH)
    if config_loaded then
        print("[APFramework] Config loaded from: " .. CONFIG_PATH)
        print("[APFramework] Server: " .. config.server .. ":" .. config.port)
        print("[APFramework] Slot: " .. (config.slot_name or "(not set)"))
        print("[APFramework] Autoconnect: " .. tostring(config.autoconnect))
        print("[APFramework] Registration timeout: " .. config.registration_timeout .. "s")
    else
        print("[APFramework] Using default configuration")
    end

    -- Build dynamic capabilities filename
    local filename_only
    CAPABILITIES_PATH, filename_only = build_capabilities_filename(config.slot_name)
    print("[APFramework] Capabilities filename: " .. filename_only)

    -- Create framework
    print("[APFramework] Creating FrameworkCore with pipe: " .. PIPE_NAME)
    framework = FrameworkWrapper:new(PIPE_NAME)
    if not framework or not framework.handle then
        print("[APFramework] ERROR: Failed to create framework!")
        return
    end

    state.initialized = true
    print("[APFramework] Framework initialized successfully")
end

-- Start IPC server
local function start_ipc()
    if not state.initialized or state.ipc_started then return end

    print("[APFramework] Starting IPC server on pipe: " .. PIPE_NAME)
    framework:start_ipc()
    state.ipc_started = true
    print("[APFramework] IPC server started - waiting for mod connections")
end

-- Register framework mod as priority client
local function register_framework_client()
    if state.framework_registered then return end

    print("[APFramework] Registering framework mod as priority client...")
    print("[APFramework] Mod ID: " .. FRAMEWORK_MOD_ID)

    -- Create client instance
    framework_client = APClient:new(FRAMEWORK_MOD_ID, PIPE_NAME)

    if not framework_client or not framework_client.connected then
        print("[APFramework] WARNING: Failed to connect framework client to IPC server")
        print("[APFramework] Will retry later...")
        return
    end

    -- Setup callbacks for framework client
    framework_client.on_registration_complete = function()
        print("[APFramework] Framework mod registered as priority client!")
        state.framework_registered = true
    end

    framework_client.on_item_received = function(item_id, location_id, player_slot)
        print("[APFramework] [PRIORITY CLIENT] Item received: " .. item_id)
    end

    framework_client.on_location_checked = function(location_id)
        print("[APFramework] [PRIORITY CLIENT] Location checked: " .. location_id)
    end

    framework_client.on_connection_status = function(connected, slot_name)
        print("[APFramework] [PRIORITY CLIENT] Connection status: " .. tostring(connected))
    end

    framework_client.on_log = function(level, component, message)
        -- Format and display C++ framework logs in UE4SS console
        local prefix = "[APFrameworkCore][" .. component .. "][" .. level:upper() .. "]"
        print(prefix .. " " .. message)
    end

    -- Register with empty capabilities (framework mod provides no items/locations)
    local success = framework_client:register({
        items = {},
        locations = {},
        regions = {}
    })

    if success then
        print("[APFramework] Framework client registration message sent")
    else
        print("[APFramework] ERROR: Failed to send framework client registration")
    end
end

-- Poll framework client for messages
local function poll_framework_client()
    if framework_client and framework_client.connected then
        framework_client:poll()
    end
end

-- Discover mods
local function discover_mods()
    if not state.ipc_started or state.mods_discovered then return end

    print("[APFramework] Discovering AP-enabled mods in: " .. MODS_DIRECTORY)
    framework:discover_mods(MODS_DIRECTORY)
    state.mods_discovered = true
    registration_start_time = os.time()
    print("[APFramework] Mod discovery initiated - waiting for registrations...")
end

-- Check registration
local function check_registration()
    if state.all_registered then return true end

    if not framework:all_mods_registered() then
        local elapsed = os.time() - registration_start_time
        if elapsed > config.registration_timeout then
            print("[APFramework] ERROR: Mod registration timeout after " .. config.registration_timeout .. "s!")
            local pending = framework:get_pending_registrations()
            print("[APFramework] Pending registrations: " .. (pending or "unknown"))
            return false
        end
        return false
    end

    state.all_registered = true
    print("[APFramework] All mods registered successfully!")

    -- Generate capabilities
    print("[APFramework] Generating capabilities JSON...")
    local capabilities = framework:generate_capabilities()
    if capabilities and #capabilities > 0 then
        local file = io.open(CAPABILITIES_PATH, "w")
        if file then
            file:write(capabilities)
            file:close()
            print("[APFramework] Capabilities saved to: " .. CAPABILITIES_PATH)
            print("[APFramework] Capabilities size: " .. #capabilities .. " bytes")
        else
            print("[APFramework] ERROR: Failed to write capabilities file")
        end
    else
        print("[APFramework] WARNING: No capabilities generated")
    end

    return true
end

-- Connect to AP
local function connect_to_ap()
    if state.connected then return end

    if not config.autoconnect then
        print("[APFramework] Autoconnect disabled - skipping AP connection")
        print("[APFramework] Use /connect command or set autoconnect=true in config")
        return
    end

    if not config.slot_name or config.slot_name == "" then
        print("[APFramework] ERROR: No slot name configured - cannot connect")
        return
    end

    print("[APFramework] ========== Connecting to Archipelago ==========")
    print("[APFramework] Server: " .. config.server .. ":" .. config.port)
    print("[APFramework] Slot: " .. config.slot_name)
    print("[APFramework] ===============================================")

    local success = framework:connect_ap(
        config.server,
        config.port,
        config.slot_name,
        config.password
    )

    if success then
        print("[APFramework] Connection initiated successfully!")
        state.connected = true

        if not state.polling_started then
            print("[APFramework] Starting polling thread...")
            framework:start_polling()
            state.polling_started = true
            print("[APFramework] Polling thread started")
        end
    else
        print("[APFramework] ERROR: Connection failed!")
        print("[APFramework] Check server address and slot name")
    end
end

-- Shutdown
local function shutdown()
    if not framework then return end
    print("[APFramework] ========== Shutting down APFramework ==========")
    framework:shutdown()
    framework = nil
    print("[APFramework] Shutdown complete")
end

-- Lifecycle states
local lifecycle = "INIT"
local lifecycle_count = 0

local function update_lifecycle()
    if lifecycle == "INIT" then
        initialize_framework()
        if state.initialized then
            lifecycle = "START_IPC"
            print("[APFramework] Lifecycle: INIT -> START_IPC")
        end

    elseif lifecycle == "START_IPC" then
        start_ipc()
        if state.ipc_started then
            -- Register framework mod as priority client (after slight delay for server to be ready)
            register_framework_client()
            lifecycle = "DISCOVER"
            print("[APFramework] Lifecycle: START_IPC -> DISCOVER")
        end

    elseif lifecycle == "DISCOVER" then
        -- Retry framework client registration if it failed
        if not state.framework_registered then
            register_framework_client()
        end

        discover_mods()
        if state.mods_discovered then
            lifecycle = "WAIT_REG"
            print("[APFramework] Lifecycle: DISCOVER -> WAIT_REG")
        end

    elseif lifecycle == "WAIT_REG" then
        if check_registration() then
            lifecycle = "CONNECT"
            print("[APFramework] Lifecycle: WAIT_REG -> CONNECT")
        end

    elseif lifecycle == "CONNECT" then
        connect_to_ap()
        lifecycle = "RUNNING"
        print("[APFramework] Lifecycle: CONNECT -> RUNNING")
        print("[APFramework] ========== Framework fully operational ==========")

    elseif lifecycle == "RUNNING" then
        -- Normal operation - nothing to do here
    end
end

-- UE4SS Hooks

-- Tick event - runs every frame
-- This will drive the lifecycle state machine until we reach RUNNING
RegisterCustomEvent("Tick", function(deltaTime)
    if lifecycle ~= "RUNNING" then
        lifecycle_count = lifecycle_count + 1
        -- Only update every 60 ticks to avoid spam
        if lifecycle_count % 60 == 0 then
            update_lifecycle()
        end
    end

    -- Poll framework client for messages (runs always, even in RUNNING state)
    poll_framework_client()
end)

-- Shutdown hook
RegisterHook("/Script/Engine.GameInstance:ReceiveShutdown", function()
    shutdown()
end)

print("[APFramework] Main script loaded - lifecycle will start on first Tick event")
