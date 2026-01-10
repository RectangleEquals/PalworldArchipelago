cmake_minimum_required(VERSION 3.20)
project(lua C)

# Lua core source files
set(LUA_CORE_SOURCES
    src/lapi.c
    src/lcode.c
    src/lctype.c
    src/ldebug.c
    src/ldo.c
    src/ldump.c
    src/lfunc.c
    src/lgc.c
    src/llex.c
    src/lmem.c
    src/lobject.c
    src/lopcodes.c
    src/lparser.c
    src/lstate.c
    src/lstring.c
    src/ltable.c
    src/ltm.c
    src/lundump.c
    src/lvm.c
    src/lzio.c
    src/lauxlib.c
    src/lbaselib.c
    src/lcorolib.c
    src/ldblib.c
    src/liolib.c
    src/lmathlib.c
    src/loadlib.c
    src/loslib.c
    src/lstrlib.c
    src/ltablib.c
    src/lutf8lib.c
    src/linit.c
)

# Build as static library
add_library(lua_static STATIC ${LUA_CORE_SOURCES})

target_include_directories(lua_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

set_target_properties(lua_static PROPERTIES
    OUTPUT_NAME "lua"
    POSITION_INDEPENDENT_CODE ON
)
