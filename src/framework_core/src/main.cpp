#include <windows.h>

/**
 * @file main.cpp
 * @brief DLL entry point for APFrameworkCore.dll
 *
 * This file provides the DllMain entry point for the Windows DLL.
 * All actual functionality is exposed through the Lua C bindings in lua_bindings.h.
 */

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            // DLL is being loaded into a process
            // No initialization needed - framework instances are created via Lua C bindings
            break;

        case DLL_THREAD_ATTACH:
            // New thread is being created in the process
            break;

        case DLL_THREAD_DETACH:
            // Thread is exiting cleanly
            break;

        case DLL_PROCESS_DETACH:
            // DLL is being unloaded from a process
            // Cleanup is handled by Lua C bindings or C library garbage collection
            break;
    }

    return TRUE;
}
