-- APClient Lua Library
-- Pure Lua IPC wrapper for communicating with APFrameworkCore

local APClient = {}
APClient.__index = APClient

-- JSON serialization helpers
local function json_encode(obj)
    if type(obj) == "table" then
        local is_array = #obj > 0
        local result = {}

        if is_array then
            table.insert(result, "[")
            for i, v in ipairs(obj) do
                if i > 1 then table.insert(result, ",") end
                table.insert(result, json_encode(v))
            end
            table.insert(result, "]")
        else
            table.insert(result, "{")
            local first = true
            for k, v in pairs(obj) do
                if not first then table.insert(result, ",") end
                first = false
                table.insert(result, '"' .. tostring(k) .. '":')
                table.insert(result, json_encode(v))
            end
            table.insert(result, "}")
        end

        return table.concat(result)
    elseif type(obj) == "string" then
        -- Escape special characters
        local escaped = obj:gsub('\\', '\\\\'):gsub('"', '\\"'):gsub('\n', '\\n'):gsub('\r', '\\r'):gsub('\t', '\\t')
        return '"' .. escaped .. '"'
    elseif type(obj) == "number" or type(obj) == "boolean" then
        return tostring(obj)
    elseif obj == nil then
        return "null"
    else
        return '""'
    end
end

local function json_decode(str)
    -- Simple JSON decoder (handles basic cases)
    -- For production, use a proper JSON library if available

    -- Remove whitespace
    str = str:gsub("^%s+", ""):gsub("%s+$", "")

    if str:sub(1, 1) == "{" then
        local result = {}
        local content = str:sub(2, -2) -- Remove { }

        -- Very basic key-value parsing
        for key_val in content:gmatch('"([^"]+)"%s*:%s*([^,}]+)') do
            local key, val = key_val:match('"([^"]+)"%s*:%s*(.+)')
            if key then
                if val:sub(1, 1) == '"' then
                    result[key] = val:sub(2, -2) -- Remove quotes
                elseif val == "true" then
                    result[key] = true
                elseif val == "false" then
                    result[key] = false
                elseif val == "null" then
                    result[key] = nil
                else
                    result[key] = tonumber(val) or val
                end
            end
        end

        return result
    elseif str:sub(1, 1) == "[" then
        local result = {}
        local content = str:sub(2, -2) -- Remove [ ]

        for item in content:gmatch("[^,]+") do
            table.insert(result, json_decode(item))
        end

        return result
    elseif str:sub(1, 1) == '"' then
        return str:sub(2, -2) -- Remove quotes
    elseif str == "true" then
        return true
    elseif str == "false" then
        return false
    elseif str == "null" then
        return nil
    else
        return tonumber(str) or str
    end
end

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

    local message_str = json_encode(message)

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
    local ok, message = pcall(json_decode, message_str)

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

    local message_str = json_encode(message)
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

    local message_str = json_encode(message)
    return self.pipe:send(message_str)
end

function APClient:shutdown()
    self.pipe:close()
    self.connected = false
end

return APClient
