#pragma once

/**
 * @file ffi_bindings.h
 * @brief C API for Lua FFI integration
 *
 * Provides a C-linkage API that can be called from LuaJIT FFI. All functions
 * use opaque handles to hide C++ implementation details from Lua.
 *
 * The framework instance is created with framework_core_create() and passed
 * to all subsequent functions. Memory management is handled by the framework -
 * Lua only needs to call framework_core_destroy() when done.
 */

extern "C" {
    // Opaque handle type for framework instance
    typedef void* FrameworkHandle;

    //
    // Lifecycle Management
    //

    /**
     * Create a new framework instance
     * @param pipe_name Named pipe path (e.g., "\\\\.\\pipe\\APFramework_default")
     * @return Framework handle, or nullptr on failure
     */
    __declspec(dllexport) FrameworkHandle framework_core_create(const char* pipe_name);

    /**
     * Destroy a framework instance and free all resources
     * @param handle Framework handle from framework_core_create()
     */
    __declspec(dllexport) void framework_core_destroy(FrameworkHandle handle);

    //
    // IPC Server Management
    //

    /**
     * Start the IPC server to listen for mod connections
     * @param handle Framework handle
     */
    __declspec(dllexport) void framework_core_start_ipc(FrameworkHandle handle);

    /**
     * Stop the IPC server
     * @param handle Framework handle
     */
    __declspec(dllexport) void framework_core_stop_ipc(FrameworkHandle handle);

    /**
     * Check if IPC server is running
     * @param handle Framework handle
     * @return true if running, false otherwise
     */
    __declspec(dllexport) bool framework_core_is_ipc_running(FrameworkHandle handle);

    //
    // AP Client Connection
    //

    /**
     * Connect to Archipelago server
     * @param handle Framework handle
     * @param server Server hostname or IP
     * @param port Server port
     * @param slot Slot name
     * @param password Slot password (can be empty string)
     * @return true if connection initiated successfully, false otherwise
     */
    __declspec(dllexport) bool framework_core_connect_ap(FrameworkHandle handle,
        const char* server, int port, const char* slot, const char* password);

    /**
     * Disconnect from AP server
     * @param handle Framework handle
     */
    __declspec(dllexport) void framework_core_disconnect_ap(FrameworkHandle handle);

    /**
     * Get AP client connection state
     * @param handle Framework handle
     * @return Connection state (0=disconnected, 1=connecting, 4=connected)
     */
    __declspec(dllexport) int framework_core_get_ap_state(FrameworkHandle handle);

    /**
     * Check if connected to AP server
     * @param handle Framework handle
     * @return true if connected, false otherwise
     */
    __declspec(dllexport) bool framework_core_is_ap_connected(FrameworkHandle handle);

    //
    // Mod Discovery and Registration
    //

    /**
     * Discover AP-enabled mods by scanning for ap_config.json files
     * @param handle Framework handle
     * @param mods_directory Path to UE4SS mods directory
     */
    __declspec(dllexport) void framework_core_discover_mods(FrameworkHandle handle,
        const char* mods_directory);

    /**
     * Check if all discovered mods have registered
     * @param handle Framework handle
     * @return true if all mods registered, false otherwise
     */
    __declspec(dllexport) bool framework_core_all_mods_registered(FrameworkHandle handle);

    /**
     * Generate APCapabilities.json from registered mods
     * @param handle Framework handle
     * @return JSON string (must be freed with framework_core_free_string)
     */
    __declspec(dllexport) const char* framework_core_generate_capabilities(FrameworkHandle handle);

    //
    // Polling Thread Management
    //

    /**
     * Start the background polling thread
     * @param handle Framework handle
     */
    __declspec(dllexport) void framework_core_start_polling(FrameworkHandle handle);

    /**
     * Stop the background polling thread
     * @param handle Framework handle
     */
    __declspec(dllexport) void framework_core_stop_polling(FrameworkHandle handle);

    /**
     * Check if polling thread is running
     * @param handle Framework handle
     * @return true if polling, false otherwise
     */
    __declspec(dllexport) bool framework_core_is_polling(FrameworkHandle handle);

    //
    // Configuration Management
    //

    /**
     * Load configuration from file
     * @param handle Framework handle
     * @param config_path Path to config.json
     * @return true if loaded successfully, false otherwise
     */
    __declspec(dllexport) bool framework_core_load_config(FrameworkHandle handle,
        const char* config_path);

    /**
     * Save configuration to file
     * @param handle Framework handle
     * @param config_path Path to config.json
     * @return true if saved successfully, false otherwise
     */
    __declspec(dllexport) bool framework_core_save_config(FrameworkHandle handle,
        const char* config_path);

    //
    // Utility
    //

    /**
     * Free a string returned by the framework
     * @param str String to free
     */
    __declspec(dllexport) void framework_core_free_string(const char* str);
}
