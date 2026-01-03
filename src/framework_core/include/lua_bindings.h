#ifndef LUA_BINDINGS_H
#define LUA_BINDINGS_H

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

// Module entry point for Lua
// UE4SS will call this when require("APFrameworkCore") is used
extern "C" __declspec(dllexport) int luaopen_APFrameworkCore(lua_State* L);

#endif // LUA_BINDINGS_H
