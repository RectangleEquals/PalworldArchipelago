// Lua bindings for APClient C library
// Provides Lua interface to ap_client_lib.h C API

extern "C" {
    #include <lua.h>
    #include <lualib.h>
    #include <lauxlib.h>
}

#include "ap_client_lib.h"
#include <string>
#include <map>

// Metatable name for APClient userdata
static const char* APCLIENT_METATABLE = "APClient";

// Helper to check and retrieve APClientHandle from Lua
static APClientHandle check_client(lua_State* L, int index) {
    APClientHandle* handle_ptr = static_cast<APClientHandle*>(
        luaL_checkudata(L, index, APCLIENT_METATABLE)
    );
    if (!handle_ptr || !*handle_ptr) {
        luaL_error(L, "Invalid APClient handle");
    }
    return *handle_ptr;
}

// ap_client.new(mod_id) -> client
static int lua_client_new(lua_State* L) {
    const char* mod_id = luaL_checkstring(L, 1);

    // Allocate userdata for handle
    APClientHandle* handle_ptr = static_cast<APClientHandle*>(
        lua_newuserdata(L, sizeof(APClientHandle))
    );

    *handle_ptr = ap_client_create(mod_id);

    if (!*handle_ptr) {
        return luaL_error(L, "Failed to create APClient");
    }

    // Set metatable
    luaL_getmetatable(L, APCLIENT_METATABLE);
    lua_setmetatable(L, -2);

    return 1; // Return userdata
}

// client:register(capabilities_json) -> bool
static int lua_client_register(lua_State* L) {
    APClientHandle handle = check_client(L, 1);
    const char* capabilities_json = luaL_checkstring(L, 2);

    bool result = ap_client_register(handle, capabilities_json);
    lua_pushboolean(L, result);
    return 1;
}

// client:poll()
static int lua_client_poll(lua_State* L) {
    APClientHandle handle = check_client(L, 1);
    ap_client_poll(handle);
    return 0;
}

// client:check_location(location_id)
static int lua_client_check_location(lua_State* L) {
    APClientHandle handle = check_client(L, 1);
    int64_t location_id = static_cast<int64_t>(luaL_checkinteger(L, 2));

    ap_client_check_location(handle, location_id);
    return 0;
}

// client:request_connection(server, port, slot_name, password)
static int lua_client_request_connection(lua_State* L) {
    APClientHandle handle = check_client(L, 1);
    const char* server = luaL_checkstring(L, 2);
    int port = static_cast<int>(luaL_checkinteger(L, 3));
    const char* slot_name = luaL_checkstring(L, 4);
    const char* password = luaL_optstring(L, 5, "");

    ap_client_request_connection(handle, server, port, slot_name, password);
    return 0;
}

// Callback wrappers - these are called from C and need to invoke Lua callbacks

struct LuaCallbacks {
    lua_State* L;
    int item_received_ref = LUA_NOREF;
    int location_checked_ref = LUA_NOREF;
    int connection_status_ref = LUA_NOREF;
    int registration_complete_ref = LUA_NOREF;
};

// Global map of handles to Lua callbacks
static std::map<APClientHandle, LuaCallbacks*> g_callbacks;

static void item_received_callback(int64_t item_id, int64_t location_id, int player_slot, void* user_data) {
    LuaCallbacks* callbacks = static_cast<LuaCallbacks*>(user_data);
    if (!callbacks || callbacks->item_received_ref == LUA_NOREF) return;

    lua_State* L = callbacks->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks->item_received_ref);
    lua_pushinteger(L, item_id);
    lua_pushinteger(L, location_id);
    lua_pushinteger(L, player_slot);

    if (lua_pcall(L, 3, 0, 0) != 0) {
        const char* error = lua_tostring(L, -1);
        fprintf(stderr, "[APClient] Error in item_received callback: %s\n", error);
        lua_pop(L, 1);
    }
}

static void location_checked_callback(int64_t location_id, void* user_data) {
    LuaCallbacks* callbacks = static_cast<LuaCallbacks*>(user_data);
    if (!callbacks || callbacks->location_checked_ref == LUA_NOREF) return;

    lua_State* L = callbacks->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks->location_checked_ref);
    lua_pushinteger(L, location_id);

    if (lua_pcall(L, 1, 0, 0) != 0) {
        const char* error = lua_tostring(L, -1);
        fprintf(stderr, "[APClient] Error in location_checked callback: %s\n", error);
        lua_pop(L, 1);
    }
}

static void connection_status_callback(bool connected, const char* slot_name, void* user_data) {
    LuaCallbacks* callbacks = static_cast<LuaCallbacks*>(user_data);
    if (!callbacks || callbacks->connection_status_ref == LUA_NOREF) return;

    lua_State* L = callbacks->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks->connection_status_ref);
    lua_pushboolean(L, connected);
    lua_pushstring(L, slot_name);

    if (lua_pcall(L, 2, 0, 0) != 0) {
        const char* error = lua_tostring(L, -1);
        fprintf(stderr, "[APClient] Error in connection_status callback: %s\n", error);
        lua_pop(L, 1);
    }
}

static void registration_complete_callback(void* user_data) {
    LuaCallbacks* callbacks = static_cast<LuaCallbacks*>(user_data);
    if (!callbacks || callbacks->registration_complete_ref == LUA_NOREF) return;

    lua_State* L = callbacks->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks->registration_complete_ref);

    if (lua_pcall(L, 0, 0, 0) != 0) {
        const char* error = lua_tostring(L, -1);
        fprintf(stderr, "[APClient] Error in registration_complete callback: %s\n", error);
        lua_pop(L, 1);
    }
}

// client:set_item_received_callback(function)
static int lua_client_set_item_received_callback(lua_State* L) {
    APClientHandle handle = check_client(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    // Get or create callbacks struct
    LuaCallbacks* callbacks = g_callbacks[handle];
    if (!callbacks) {
        callbacks = new LuaCallbacks{L};
        g_callbacks[handle] = callbacks;
    }

    // Store function reference
    if (callbacks->item_received_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, callbacks->item_received_ref);
    }
    lua_pushvalue(L, 2);
    callbacks->item_received_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    // Register C callback
    ap_client_set_item_received_callback(handle, item_received_callback, callbacks);

    return 0;
}

// client:set_location_checked_callback(function)
static int lua_client_set_location_checked_callback(lua_State* L) {
    APClientHandle handle = check_client(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    LuaCallbacks* callbacks = g_callbacks[handle];
    if (!callbacks) {
        callbacks = new LuaCallbacks{L};
        g_callbacks[handle] = callbacks;
    }

    if (callbacks->location_checked_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, callbacks->location_checked_ref);
    }
    lua_pushvalue(L, 2);
    callbacks->location_checked_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    ap_client_set_location_checked_callback(handle, location_checked_callback, callbacks);

    return 0;
}

// client:set_connection_status_callback(function)
static int lua_client_set_connection_status_callback(lua_State* L) {
    APClientHandle handle = check_client(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    LuaCallbacks* callbacks = g_callbacks[handle];
    if (!callbacks) {
        callbacks = new LuaCallbacks{L};
        g_callbacks[handle] = callbacks;
    }

    if (callbacks->connection_status_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, callbacks->connection_status_ref);
    }
    lua_pushvalue(L, 2);
    callbacks->connection_status_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    ap_client_set_connection_status_callback(handle, connection_status_callback, callbacks);

    return 0;
}

// client:set_registration_complete_callback(function)
static int lua_client_set_registration_complete_callback(lua_State* L) {
    APClientHandle handle = check_client(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    LuaCallbacks* callbacks = g_callbacks[handle];
    if (!callbacks) {
        callbacks = new LuaCallbacks{L};
        g_callbacks[handle] = callbacks;
    }

    if (callbacks->registration_complete_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, callbacks->registration_complete_ref);
    }
    lua_pushvalue(L, 2);
    callbacks->registration_complete_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    ap_client_set_registration_complete_callback(handle, registration_complete_callback, callbacks);

    return 0;
}

// client:is_connected() -> boolean
static int lua_client_is_connected(lua_State* L) {
    APClientHandle handle = check_client(L, 1);
    bool connected = ap_client_is_connected(handle);
    lua_pushboolean(L, connected);
    return 1;
}

// Garbage collection
static int lua_client_gc(lua_State* L) {
    APClientHandle* handle_ptr = static_cast<APClientHandle*>(
        luaL_checkudata(L, 1, APCLIENT_METATABLE)
    );

    if (handle_ptr && *handle_ptr) {
        // Clean up callbacks
        auto it = g_callbacks.find(*handle_ptr);
        if (it != g_callbacks.end()) {
            LuaCallbacks* callbacks = it->second;
            if (callbacks) {
                if (callbacks->item_received_ref != LUA_NOREF) {
                    luaL_unref(L, LUA_REGISTRYINDEX, callbacks->item_received_ref);
                }
                if (callbacks->location_checked_ref != LUA_NOREF) {
                    luaL_unref(L, LUA_REGISTRYINDEX, callbacks->location_checked_ref);
                }
                if (callbacks->connection_status_ref != LUA_NOREF) {
                    luaL_unref(L, LUA_REGISTRYINDEX, callbacks->connection_status_ref);
                }
                if (callbacks->registration_complete_ref != LUA_NOREF) {
                    luaL_unref(L, LUA_REGISTRYINDEX, callbacks->registration_complete_ref);
                }
                delete callbacks;
            }
            g_callbacks.erase(it);
        }

        ap_client_destroy(*handle_ptr);
        *handle_ptr = nullptr;
    }

    return 0;
}

// Module registration
extern "C" __declspec(dllexport) int luaopen_APClientLib(lua_State* L) {
    // Create metatable for APClient
    luaL_newmetatable(L, APCLIENT_METATABLE);

    // Set __gc metamethod
    lua_pushcfunction(L, lua_client_gc);
    lua_setfield(L, -2, "__gc");

    // Set __index to itself (for methods)
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    // Register methods
    static const luaL_Reg methods[] = {
        {"register", lua_client_register},
        {"poll", lua_client_poll},
        {"check_location", lua_client_check_location},
        {"request_connection", lua_client_request_connection},
        {"set_item_received_callback", lua_client_set_item_received_callback},
        {"set_location_checked_callback", lua_client_set_location_checked_callback},
        {"set_connection_status_callback", lua_client_set_connection_status_callback},
        {"set_registration_complete_callback", lua_client_set_registration_complete_callback},
        {"is_connected", lua_client_is_connected},
        {nullptr, nullptr}
    };
    luaL_setfuncs(L, methods, 0);

    // Create module table
    lua_newtable(L);

    // Add new function
    lua_pushcfunction(L, lua_client_new);
    lua_setfield(L, -2, "new");

    return 1; // Return module table
}