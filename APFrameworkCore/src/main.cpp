#include <windows.h>
#include <sol/sol.hpp>
#include "lua_bindings.h"

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    // Suppress unused parameter warnings
    (void)hModule;
    (void)lpReserved;

    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
        case DLL_PROCESS_DETACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}

// Lua module entry point (called by require("APFrameworkCore"))
// This receives the existing Lua state from UE4SS and registers our C++ bindings into it
extern "C" {
    __declspec(dllexport) int luaopen_APFrameworkCore(lua_State* L) {
        // Create sol::state_view from UE4SS's existing Lua state
        sol::state_view lua(L);

        // Register all APFramework bindings into this state
        APFramework::register_apframework_bindings(lua);

        // Return 1 to indicate module loaded successfully
        return 1;
    }
}