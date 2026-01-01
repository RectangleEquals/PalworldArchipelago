-- APFramework Main Entry Point (IPC Branch)
-- Loads APFrameworkCore.dll and manages framework lifecycle

print("[APFramework] Loading APFramework v2.0.0 (IPC Architecture)...")

local FrameworkWrapper = require("framework_wrapper")
local Config = require("config")

-- Global state
local framework = nil
local config = nil
local state = {
    initialized = false,
    ipc_started = false,
    mods_discovered = false,
    all_registered = false,
    connected = false,
    polling_started = false
}

local registration_start_time = 0
local registration_timeout = 180 -- 3 minutes

-- Configuration
local PIPE_NAME = "APFramework_default"
local MODS_DIRECTORY = "Mods"
local CONFIG_PATH = "Mods/APFramework/framework_config.json"

-- Initialize framework
local function initialize_framework()
    if state.initialized then return end

    print("[APFramework] Initializing...")

    -- Load config
    config = Config:new()
    config:load(CONFIG_PATH)

    -- Create framework
    framework = FrameworkWrapper:new(PIPE_NAME)
    if not framework or not framework.handle then
        print("[APFramework] ERROR: Failed to create framework!")
        return
    end

    state.initialized = true
    print("[APFramework] Framework initialized")
end

-- Start IPC server
local function start_ipc()
    if not state.initialized or state.ipc_started then return end

    print("[APFramework] Starting IPC server on pipe: " .. PIPE_NAME)
    framework:start_ipc()
    state.ipc_started = true
end

-- Discover mods
local function discover_mods()
    if not state.ipc_started or state.mods_discovered then return end

    print("[APFramework] Discovering AP-enabled mods...")
    framework:discover_mods(MODS_DIRECTORY)
    state.mods_discovered = true
    registration_start_time = os.time()
end

-- Check registration
local function check_registration()
    if state.all_registered then return true end

    if not framework:all_mods_registered() then
        local elapsed = os.time() - registration_start_time
        if elapsed > registration_timeout then
            print("[APFramework] ERROR: Registration timeout!")
            return false
        end
        return false
    end

    state.all_registered = true
    print("[APFramework] All mods registered!")

    -- Generate capabilities
    local capabilities = framework:generate_capabilities()
    if capabilities then
        local file = io.open("Mods/APFramework/APCapabilities.json", "w")
        if file then
            file:write(capabilities)
            file:close()
            print("[APFramework] APCapabilities.json generated")
        end
    end

    return true
end

-- Connect to AP
local function connect_to_ap()
    if state.connected then return end

    if not config.autoconnect then
        print("[APFramework] Autoconnect disabled")
        return
    end

    if not config.slot_name or config.slot_name == "" then
        print("[APFramework] No slot name configured")
        return
    end

    print("[APFramework] Connecting to " .. config.server .. ":" .. config.port)
    print("[APFramework] Slot: " .. config.slot_name)

    local success = framework:connect_ap(
        config.server,
        config.port,
        config.slot_name,
        config.password
    )

    if success then
        print("[APFramework] Connected!")
        state.connected = true

        if not state.polling_started then
            framework:start_polling()
            state.polling_started = true
            print("[APFramework] Polling started")
        end
    else
        print("[APFramework] Connection failed")
    end
end

-- Shutdown
local function shutdown()
    if not framework then return end
    print("[APFramework] Shutting down...")
    framework:shutdown()
    framework = nil
end

-- Lifecycle states
local lifecycle = "INIT"

local function update_lifecycle()
    if lifecycle == "INIT" then
        initialize_framework()
        if state.initialized then lifecycle = "START_IPC" end

    elseif lifecycle == "START_IPC" then
        start_ipc()
        if state.ipc_started then lifecycle = "DISCOVER" end

    elseif lifecycle == "DISCOVER" then
        discover_mods()
        if state.mods_discovered then lifecycle = "WAIT_REG" end

    elseif lifecycle == "WAIT_REG" then
        if check_registration() then lifecycle = "CONNECT" end

    elseif lifecycle == "CONNECT" then
        connect_to_ap()
        lifecycle = "RUNNING"

    elseif lifecycle == "RUNNING" then
        -- Normal operation
    end
end

-- UE4SS Hooks
RegisterHook("/Script/Engine.PlayerController:ServerAcknowledgePossession", function()
    if lifecycle ~= "RUNNING" then
        update_lifecycle()
    end
end)

RegisterHook("/Script/Engine.PlayerController:PlayerTick", function()
    if lifecycle ~= "RUNNING" then
        update_lifecycle()
    end
end)

RegisterHook("/Script/Engine.GameInstance:Shutdown", function()
    shutdown()
end)

print("[APFramework] Main script loaded")
