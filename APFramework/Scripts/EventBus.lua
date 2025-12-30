--[[
    Palworld Archipelago Framework
    Module: EventBus

    Simple publish/subscribe event system for framework and mod coordination.
    Supports priority-based callbacks and error isolation.
]]

local EventBus = {}

-- Module state
EventBus.initialized = false
EventBus.events = {}
EventBus.next_subscription_id = 1
EventBus.subscriptions = {}

---Initialize the event bus
---@return boolean success
function EventBus:Initialize()
    if self.initialized then
        return true
    end

    self.events = {}
    self.next_subscription_id = 1
    self.subscriptions = {}
    self.initialized = true

    print("[EventBus] Initialized")
    return true
end

---Subscribe to an event
---@param event_name string Event to subscribe to
---@param callback function Callback function(event_data)
---@param priority integer|nil Priority (higher = called first, default: 0)
---@return integer subscription_id Unique subscription ID for unsubscribing
function EventBus:Subscribe(event_name, callback, priority)
    if not self.initialized then
        print("[EventBus] Error: Not initialized")
        return -1
    end

    if type(event_name) ~= "string" then
        print("[EventBus] Error: event_name must be a string")
        return -1
    end

    if type(callback) ~= "function" then
        print("[EventBus] Error: callback must be a function")
        return -1
    end

    priority = priority or 0

    -- Create event table if it doesn't exist
    if not self.events[event_name] then
        self.events[event_name] = {}
    end

    -- Create subscription entry
    local subscription_id = self.next_subscription_id
    self.next_subscription_id = self.next_subscription_id + 1

    local subscription = {
        id = subscription_id,
        callback = callback,
        priority = priority,
        event_name = event_name
    }

    -- Add to event subscribers
    table.insert(self.events[event_name], subscription)

    -- Sort by priority (higher first)
    table.sort(self.events[event_name], function(a, b)
        return a.priority > b.priority
    end)

    -- Store subscription for lookup
    self.subscriptions[subscription_id] = subscription

    return subscription_id
end

---Unsubscribe from an event
---@param subscription_id integer Subscription ID returned from Subscribe
---@return boolean success
function EventBus:Unsubscribe(subscription_id)
    if not self.initialized then
        print("[EventBus] Error: Not initialized")
        return false
    end

    local subscription = self.subscriptions[subscription_id]
    if not subscription then
        print("[EventBus] Warning: Subscription not found: " .. tostring(subscription_id))
        return false
    end

    local event_name = subscription.event_name
    local event_subscribers = self.events[event_name]

    if event_subscribers then
        for i, sub in ipairs(event_subscribers) do
            if sub.id == subscription_id then
                table.remove(event_subscribers, i)
                break
            end
        end
    end

    self.subscriptions[subscription_id] = nil

    return true
end

---Fire an event and call all subscribers
---@param event_name string Event to fire
---@param event_data table|nil Data to pass to callbacks
---@return integer callbacks_called Number of callbacks successfully called
function EventBus:Fire(event_name, event_data)
    if not self.initialized then
        print("[EventBus] Error: Not initialized")
        return 0
    end

    if type(event_name) ~= "string" then
        print("[EventBus] Error: event_name must be a string")
        return 0
    end

    local event_subscribers = self.events[event_name]
    if not event_subscribers or #event_subscribers == 0 then
        -- No subscribers, this is fine
        return 0
    end

    event_data = event_data or {}
    local callbacks_called = 0
    local errors = {}

    for _, subscription in ipairs(event_subscribers) do
        local success, error_msg = pcall(subscription.callback, event_data)

        if success then
            callbacks_called = callbacks_called + 1
        else
            -- Isolate errors - don't let one callback break others
            table.insert(errors, {
                subscription_id = subscription.id,
                error = error_msg
            })
            print(string.format("[EventBus] Error in callback for event '%s' (sub_id: %d): %s",
                  event_name, subscription.id, tostring(error_msg)))
        end
    end

    if #errors > 0 then
        print(string.format("[EventBus] Event '%s' had %d error(s) out of %d callbacks",
              event_name, #errors, #event_subscribers))
    end

    return callbacks_called
end

---Get number of subscribers for an event
---@param event_name string Event name
---@return integer count Number of subscribers
function EventBus:GetSubscriberCount(event_name)
    if not self.events[event_name] then
        return 0
    end
    return #self.events[event_name]
end

---Get all event names that have subscribers
---@return table event_names Array of event names
function EventBus:GetEventNames()
    local names = {}
    for event_name, _ in pairs(self.events) do
        table.insert(names, event_name)
    end
    return names
end

---Clear all subscribers for an event
---@param event_name string Event to clear
---@return boolean success
function EventBus:ClearEvent(event_name)
    if not self.events[event_name] then
        return false
    end

    -- Remove all subscriptions from lookup
    for _, subscription in ipairs(self.events[event_name]) do
        self.subscriptions[subscription.id] = nil
    end

    self.events[event_name] = nil
    return true
end

---Clear all events and subscriptions
function EventBus:ClearAll()
    self.events = {}
    self.subscriptions = {}
    self.next_subscription_id = 1
end

---Get statistics about the event bus
---@return table stats Event bus statistics
function EventBus:GetStatistics()
    local total_subscribers = 0
    local events_with_subscribers = 0

    for event_name, subscribers in pairs(self.events) do
        events_with_subscribers = events_with_subscribers + 1
        total_subscribers = total_subscribers + #subscribers
    end

    return {
        total_events = events_with_subscribers,
        total_subscribers = total_subscribers,
        next_subscription_id = self.next_subscription_id
    }
end

---Print event bus statistics
function EventBus:PrintStatistics()
    local stats = self:GetStatistics()

    print("[EventBus] === Statistics ===")
    print(string.format("  Total Events: %d", stats.total_events))
    print(string.format("  Total Subscribers: %d", stats.total_subscribers))
    print(string.format("  Next Sub ID: %d", stats.next_subscription_id))

    if stats.total_events > 0 then
        print("  Events:")
        for event_name, subscribers in pairs(self.events) do
            print(string.format("    - %s: %d subscribers", event_name, #subscribers))
        end
    end

    print("[EventBus] ===================")
end

return EventBus
