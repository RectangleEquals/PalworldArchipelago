#include "lua_bindings.h"
#include "framework_core.h"
#include <string>

using namespace APFramework;

// Metatable name for FrameworkCore userdata
static const char* FRAMEWORK_METATABLE = "APFramework.FrameworkCore";

// Helper: Get FrameworkCore* from Lua userdata
static FrameworkCore* check_framework(lua_State* L, int index) {
    void* ud = luaL_checkudata(L, index, FRAMEWORK_METATABLE);
    luaL_argcheck(L, ud != nullptr, index, "`FrameworkCore` expected");
    return *static_cast<FrameworkCore**>(ud);
}

// framework.init_logger(log_file_path) - DEPRECATED: Logger now integrated into FrameworkCore
static int lua_init_logger(lua_State* L) {
    // No-op: Logger is now initialized automatically by FrameworkCore
    return 0;
}

// framework.create(pipe_name) -> handle
static int lua_framework_create(lua_State* L) {
    const char* pipe_name = luaL_checkstring(L, 1);

    // Allocate userdata for FrameworkCore*
    FrameworkCore** ud = static_cast<FrameworkCore**>(
        lua_newuserdata(L, sizeof(FrameworkCore*))
    );

    *ud = new FrameworkCore(pipe_name);

    // Set metatable
    luaL_getmetatable(L, FRAMEWORK_METATABLE);
    lua_setmetatable(L, -2);

    return 1; // Return userdata
}

// handle:start_ipc()
static int lua_start_ipc(lua_State* L) {
    FrameworkCore* fw = check_framework(L, 1);
    fw->start_ipc();
    return 0;
}

// handle:stop_ipc()
static int lua_stop_ipc(lua_State* L) {
    FrameworkCore* fw = check_framework(L, 1);
    fw->stop_ipc();
    return 0;
}

// handle:discover_mods(mods_directory)
static int lua_discover_mods(lua_State* L) {
    FrameworkCore* fw = check_framework(L, 1);
    const char* mods_dir = luaL_checkstring(L, 2);
    fw->discover_mods(mods_dir);
    return 0;
}

// handle:all_mods_registered() -> bool
static int lua_all_mods_registered(lua_State* L) {
    FrameworkCore* fw = check_framework(L, 1);
    bool result = fw->all_mods_registered();
    lua_pushboolean(L, result);
    return 1;
}

// handle:get_pending_registrations() -> nil (not implemented yet)
static int lua_get_pending_registrations(lua_State* L) {
    // TODO: Implement get_pending_registrations in FrameworkCore
    lua_pushstring(L, "[]");  // Return empty array for now
    return 1;
}

// handle:generate_capabilities() -> string (JSON)
static int lua_generate_capabilities(lua_State* L) {
    FrameworkCore* fw = check_framework(L, 1);
    std::string result = fw->generate_capabilities();
    lua_pushstring(L, result.c_str());
    return 1;
}

// handle:connect_ap(server, port, slot_name, password) -> bool
static int lua_connect_ap(lua_State* L) {
    FrameworkCore* fw = check_framework(L, 1);
    const char* server = luaL_checkstring(L, 2);
    int port = static_cast<int>(luaL_checkinteger(L, 3));
    const char* slot_name = luaL_checkstring(L, 4);
    const char* password = luaL_optstring(L, 5, "");

    bool result = fw->connect_ap(server, port, slot_name, password);
    lua_pushboolean(L, result);
    return 1;
}

// handle:disconnect_ap()
static int lua_disconnect_ap(lua_State* L) {
    FrameworkCore* fw = check_framework(L, 1);
    fw->disconnect_ap();
    return 0;
}

// handle:is_connected() -> bool
static int lua_is_connected(lua_State* L) {
    FrameworkCore* fw = check_framework(L, 1);
    bool result = fw->is_ap_connected();
    lua_pushboolean(L, result);
    return 1;
}

// handle:start_polling()
static int lua_start_polling(lua_State* L) {
    FrameworkCore* fw = check_framework(L, 1);
    fw->start_polling();
    return 0;
}

// handle:stop_polling()
static int lua_stop_polling(lua_State* L) {
    FrameworkCore* fw = check_framework(L, 1);
    fw->stop_polling();
    return 0;
}

// handle:load_config(config_path) -> bool
static int lua_load_config(lua_State* L) {
    FrameworkCore* fw = check_framework(L, 1);
    const char* config_path = luaL_checkstring(L, 2);
    bool result = fw->load_config(config_path);
    lua_pushboolean(L, result);
    return 1;
}

// handle:save_config(config_path) -> bool
static int lua_save_config(lua_State* L) {
    FrameworkCore* fw = check_framework(L, 1);
    const char* config_path = luaL_checkstring(L, 2);
    bool result = fw->save_config(config_path);
    lua_pushboolean(L, result);
    return 1;
}

// handle:get_active_profile() -> string (JSON)
// TODO: These methods don't exist in FrameworkCore yet
// For now, return stub values
static int lua_get_active_profile(lua_State* L) {
    // Return empty JSON object
    lua_pushstring(L, "{}");
    return 1;
}

// handle:set_active_profile(profile_json) -> bool
static int lua_set_active_profile(lua_State* L) {
    // Stub - always return true
    lua_pushboolean(L, true);
    return 1;
}

// Garbage collection
static int lua_framework_gc(lua_State* L) {
    FrameworkCore* fw = check_framework(L, 1);
    delete fw;
    return 0;
}

// Metatable methods
static const luaL_Reg framework_methods[] = {
    {"start_ipc", lua_start_ipc},
    {"stop_ipc", lua_stop_ipc},
    {"discover_mods", lua_discover_mods},
    {"all_mods_registered", lua_all_mods_registered},
    {"get_pending_registrations", lua_get_pending_registrations},
    {"generate_capabilities", lua_generate_capabilities},
    {"connect_ap", lua_connect_ap},
    {"disconnect_ap", lua_disconnect_ap},
    {"is_connected", lua_is_connected},
    {"start_polling", lua_start_polling},
    {"stop_polling", lua_stop_polling},
    {"load_config", lua_load_config},
    {"save_config", lua_save_config},
    {"get_active_profile", lua_get_active_profile},
    {"set_active_profile", lua_set_active_profile},
    {"__gc", lua_framework_gc},
    {nullptr, nullptr}
};

// Module entry point
extern "C" __declspec(dllexport) int luaopen_APFrameworkCore(lua_State* L) {
    // Create metatable for FrameworkCore userdata
    luaL_newmetatable(L, FRAMEWORK_METATABLE);

    // Set __index to self (metatable contains methods)
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    // Register methods
    luaL_setfuncs(L, framework_methods, 0);

    // Create module table
    lua_newtable(L);

    // Add init_logger function
    lua_pushcfunction(L, lua_init_logger);
    lua_setfield(L, -2, "init_logger");

    // Add create function
    lua_pushcfunction(L, lua_framework_create);
    lua_setfield(L, -2, "create");

    return 1; // Return module table
}
