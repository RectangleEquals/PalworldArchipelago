#include "ffi_bindings.h"
#include "framework_core.h"
#include <cstring>

using namespace APFramework;

// Helper: Convert opaque handle to FrameworkCore pointer
static FrameworkCore* handle_to_core(FrameworkHandle handle) {
    return static_cast<FrameworkCore*>(handle);
}

// Lifecycle Management
FrameworkHandle framework_core_create(const char* pipe_name) {
    if (!pipe_name) {
        return nullptr;
    }

    try {
        FrameworkCore* core = new FrameworkCore(pipe_name);
        return static_cast<FrameworkHandle>(core);
    } catch (...) {
        return nullptr;
    }
}

void framework_core_destroy(FrameworkHandle handle) {
    if (handle) {
        delete handle_to_core(handle);
    }
}

// IPC Server Management
void framework_core_start_ipc(FrameworkHandle handle) {
    if (handle) {
        handle_to_core(handle)->start_ipc();
    }
}

void framework_core_stop_ipc(FrameworkHandle handle) {
    if (handle) {
        handle_to_core(handle)->stop_ipc();
    }
}

bool framework_core_is_ipc_running(FrameworkHandle handle) {
    if (handle) {
        return handle_to_core(handle)->is_ipc_running();
    }
    return false;
}

// AP Client Connection
bool framework_core_connect_ap(FrameworkHandle handle,
                               const char* server, int port,
                               const char* slot, const char* password) {
    if (!handle || !server || !slot || !password) {
        return false;
    }

    return handle_to_core(handle)->connect_ap(server, port, slot, password);
}

void framework_core_disconnect_ap(FrameworkHandle handle) {
    if (handle) {
        handle_to_core(handle)->disconnect_ap();
    }
}

int framework_core_get_ap_state(FrameworkHandle handle) {
    if (handle) {
        return handle_to_core(handle)->get_ap_state();
    }
    return 0; // Disconnected
}

bool framework_core_is_ap_connected(FrameworkHandle handle) {
    if (handle) {
        return handle_to_core(handle)->is_ap_connected();
    }
    return false;
}

// Mod Discovery and Registration
void framework_core_discover_mods(FrameworkHandle handle, const char* mods_directory) {
    if (handle && mods_directory) {
        handle_to_core(handle)->discover_mods(mods_directory);
    }
}

bool framework_core_all_mods_registered(FrameworkHandle handle) {
    if (handle) {
        return handle_to_core(handle)->all_mods_registered();
    }
    return false;
}

const char* framework_core_generate_capabilities(FrameworkHandle handle) {
    if (!handle) {
        return nullptr;
    }

    try {
        std::string capabilities = handle_to_core(handle)->generate_capabilities();

        // Allocate C string (caller must free with framework_core_free_string)
        char* result = new char[capabilities.size() + 1];
        std::strcpy(result, capabilities.c_str());

        return result;
    } catch (...) {
        return nullptr;
    }
}

// Polling Thread Management
void framework_core_start_polling(FrameworkHandle handle) {
    if (handle) {
        handle_to_core(handle)->start_polling();
    }
}

void framework_core_stop_polling(FrameworkHandle handle) {
    if (handle) {
        handle_to_core(handle)->stop_polling();
    }
}

bool framework_core_is_polling(FrameworkHandle handle) {
    if (handle) {
        return handle_to_core(handle)->is_polling();
    }
    return false;
}

// Configuration Management
bool framework_core_load_config(FrameworkHandle handle, const char* config_path) {
    if (handle && config_path) {
        return handle_to_core(handle)->load_config(config_path);
    }
    return false;
}

bool framework_core_save_config(FrameworkHandle handle, const char* config_path) {
    if (handle && config_path) {
        return handle_to_core(handle)->save_config(config_path);
    }
    return false;
}

// Utility
void framework_core_free_string(const char* str) {
    if (str) {
        delete[] str;
    }
}
