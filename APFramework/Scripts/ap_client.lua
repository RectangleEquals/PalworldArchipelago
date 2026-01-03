-- APClient Lua Library
-- Pure Lua IPC wrapper for communicating with APFrameworkCore

local json = require("lib.lunajson")

local APClient = {}
APClient.__index = APClient

-- Named Pipe wrapper using io.open
local NamedPipe = {}
NamedPipe.__index = NamedPipe

function NamedPipe:new(pipe_name)
    local obj = {
        pipe_name = pipe_name,
        handle = nil,
        read_buffer = ""
    }
    setmetatable(obj, self)
    return obj
end

function NamedPipe:connect()
    if self.handle then
        return true
    end

    local full_path = "\\\\.\\pipe\\" .. self.pipe_name

    -- Try to open the pipe
    self.handle = io.open(full_path, "r+b")

    return self.handle ~= nil
end

function NamedPipe:send(message_str)
    if not self.handle then
        return false
    end

    -- Add newline delimiter
    local data = message_str .. "\n"

    self.handle:write(data)
    self.handle:flush()

    return true
end

function NamedPipe:receive()
    if not self.handle then
        return nil
    end

    -- Read until newline (non-blocking would require FFI)
    local line = self.handle:read("*l")

    return line
end

function NamedPipe:close()
    if self.handle then
        self.handle:close()
        self.handle = nil
    end
end

-- APClient implementation

function APClient:new(mod_id, pipe_name)
    local obj = {
        mod_id = mod_id,
        pipe = NamedPipe:new(pipe_name or "APFramework_default"),
        connected = false,

        -- Callbacks (set by user)
        on_item_received = nil,
        on_location_checked = nil,
        on_connection_status = nil,
        on_registration_complete = nil,

        -- Internal state
        message_queue = {}
    }

    setmetatable(obj, self)

    -- Auto-connect
    obj:_connect()

    return obj
end

function APClient:_connect()
    if self.connected then
        return true
    end

    self.connected = self.pipe:connect()
    return self.connected
end

function APClient:register(capabilities)
    -- Ensure connected
    if not self.connected then
        if not self:_connect() then
            print("[APClient] Failed to connect to framework pipe")
            return false
        end
    end

    -- Construct registration message
    local message = {
        type = "register",
        mod_id = self.mod_id,
        data = capabilities
    }

    local ok, message_str = pcall(json.encode, message)
    if not ok then
        print("[APClient] ERROR: Failed to encode registration message: " .. tostring(message_str))
        return false
    end

    return self.pipe:send(message_str)
end

function APClient:poll()
    -- Try to reconnect if disconnected
    if not self.connected then
        self:_connect()
    end

    if not self.connected then
        return
    end

    -- Read all available messages (non-blocking read would be better)
    local message_str = self.pipe:receive()

    if message_str then
        self:_process_message(message_str)
    end
end

function APClient:_process_message(message_str)
    local ok, message = pcall(json.decode, message_str)

    if not ok then
        print("[APClient] Failed to decode message: " .. tostring(message))
        return
    end

    local msg_type = message.type
    local data = message.data or {}

    if msg_type == "item_received" then
        if self.on_item_received then
            self.on_item_received(
                data.item_id or 0,
                data.location_id or 0,
                data.player_slot or 0
            )
        end
    elseif msg_type == "location_checked" then
        if self.on_location_checked then
            self.on_location_checked(data.location_id or 0)
        end
    elseif msg_type == "connection_status" then
        if self.on_connection_status then
            self.on_connection_status(
                data.connected or false,
                data.slot_name or ""
            )
        end
    elseif msg_type == "registration_complete" then
        if self.on_registration_complete then
            self.on_registration_complete()
        end
    end
end

function APClient:check_location(location_id)
    if not self.connected then
        return false
    end

    local message = {
        type = "check_location",
        mod_id = self.mod_id,
        data = {
            location_id = location_id
        }
    }

    local ok, message_str = pcall(json.encode, message)
    if not ok then
        print("[APClient] ERROR: Failed to encode check_location message: " .. tostring(message_str))
        return false
    end

    return self.pipe:send(message_str)
end

function APClient:request_connection(server, port, slot_name, password)
    if not self.connected then
        return false
    end

    local message = {
        type = "request_connection",
        mod_id = self.mod_id,
        data = {
            server = server,
            port = port,
            slot_name = slot_name,
            password = password or ""
        }
    }

    local ok, message_str = pcall(json.encode, message)
    if not ok then
        print("[APClient] ERROR: Failed to encode request_connection message: " .. tostring(message_str))
        return false
    end

    return self.pipe:send(message_str)
end

function APClient:shutdown()
    self.pipe:close()
    self.connected = false
end

return APClient
