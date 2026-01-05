#include "ap_client_lib.h"
#include "ipc_client.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <unordered_set>
#include <cstring>

using json = nlohmann::json;

namespace {

// Internal client instance structure
struct APClientInstance {
    std::string mod_id;
    std::unique_ptr<APClientLib::IPCClient> ipc;
    std::string last_error;

    // Callbacks
    ItemReceivedCallback item_received_cb = nullptr;
    void* item_received_user_data = nullptr;

    LocationCheckedCallback location_checked_cb = nullptr;
    void* location_checked_user_data = nullptr;

    ConnectionStatusCallback connection_status_cb = nullptr;
    void* connection_status_user_data = nullptr;

    RegistrationCompleteCallback registration_complete_cb = nullptr;
    void* registration_complete_user_data = nullptr;

    // Track processed items to avoid duplicates
    std::unordered_set<int64_t> processed_items;

    explicit APClientInstance(const std::string& mod_id_)
        : mod_id(mod_id_), ipc(std::make_unique<APClientLib::IPCClient>("APFramework_default")) {
    }

    void process_messages() {
        std::vector<APClientLib::IPCMessage> messages;
        if (!ipc->poll_messages(messages)) {
            return;
        }

        for (const auto& msg : messages) {
            try {
                json data = json::parse(msg.data);

                if (msg.type == "item_received") {
                    if (item_received_cb) {
                        int64_t item_id = data.value("item_id", 0LL);
                        int64_t location_id = data.value("location_id", 0LL);
                        int player_slot = data.value("player_slot", 0);
                        item_received_cb(item_id, location_id, player_slot, item_received_user_data);
                    }
                } else if (msg.type == "location_checked") {
                    if (location_checked_cb) {
                        int64_t location_id = data.value("location_id", 0LL);
                        location_checked_cb(location_id, location_checked_user_data);
                    }
                } else if (msg.type == "connection_status") {
                    if (connection_status_cb) {
                        bool connected = data.value("connected", false);
                        std::string slot_name = data.value("slot_name", "");
                        connection_status_cb(connected, slot_name.c_str(), connection_status_user_data);
                    }
                } else if (msg.type == "registration_complete") {
                    if (registration_complete_cb) {
                        registration_complete_cb(registration_complete_user_data);
                    }
                }
            } catch (const std::exception& e) {
                last_error = std::string("Failed to process message: ") + e.what();
            }
        }
    }
};

// Helper to allocate a C string copy
char* allocate_string(const std::string& str) {
    char* result = new char[str.size() + 1];
    std::strcpy(result, str.c_str());
    return result;
}

} // anonymous namespace

// C API implementation

APClientHandle ap_client_create(const char* mod_id) {
    if (!mod_id) {
        return nullptr;
    }

    try {
        auto* instance = new APClientInstance(mod_id);

        // Auto-connect to IPC pipe
        if (!instance->ipc->connect()) {
            instance->last_error = instance->ipc->get_last_error();
            // Don't fail creation - allow retries during poll
        }

        return static_cast<APClientHandle>(instance);
    } catch (...) {
        return nullptr;
    }
}

void ap_client_destroy(APClientHandle handle) {
    if (!handle) {
        return;
    }

    auto* instance = static_cast<APClientInstance*>(handle);
    delete instance;
}

bool ap_client_register(APClientHandle handle, const char* capabilities_json) {
    if (!handle || !capabilities_json) {
        return false;
    }

    auto* instance = static_cast<APClientInstance*>(handle);

    // Ensure connected
    if (!instance->ipc->is_connected()) {
        if (!instance->ipc->connect()) {
            instance->last_error = "Not connected to framework";
            return false;
        }
    }

    // Send registration message
    if (!instance->ipc->send_message("register", instance->mod_id, capabilities_json)) {
        instance->last_error = instance->ipc->get_last_error();
        return false;
    }

    return true;
}

void ap_client_poll(APClientHandle handle) {
    if (!handle) {
        return;
    }

    auto* instance = static_cast<APClientInstance*>(handle);

    // Try to reconnect if disconnected
    if (!instance->ipc->is_connected()) {
        instance->ipc->connect();
    }

    // Process incoming messages
    instance->process_messages();
}

void ap_client_check_location(APClientHandle handle, int64_t location_id) {
    if (!handle) {
        return;
    }

    auto* instance = static_cast<APClientInstance*>(handle);

    if (!instance->ipc->is_connected()) {
        return;
    }

    json data;
    data["location_id"] = location_id;

    instance->ipc->send_message("check_location", instance->mod_id, data.dump());
}

void ap_client_request_connection(APClientHandle handle,
                                   const char* server, int port,
                                   const char* slot_name, const char* password) {
    if (!handle || !server || !slot_name) {
        return;
    }

    auto* instance = static_cast<APClientInstance*>(handle);

    if (!instance->ipc->is_connected()) {
        return;
    }

    json data;
    data["server"] = server;
    data["port"] = port;
    data["slot_name"] = slot_name;
    data["password"] = password ? password : "";

    instance->ipc->send_message("request_connection", instance->mod_id, data.dump());
}

void ap_client_set_item_received_callback(APClientHandle handle,
                                           ItemReceivedCallback callback,
                                           void* user_data) {
    if (!handle) {
        return;
    }

    auto* instance = static_cast<APClientInstance*>(handle);
    instance->item_received_cb = callback;
    instance->item_received_user_data = user_data;
}

void ap_client_set_location_checked_callback(APClientHandle handle,
                                              LocationCheckedCallback callback,
                                              void* user_data) {
    if (!handle) {
        return;
    }

    auto* instance = static_cast<APClientInstance*>(handle);
    instance->location_checked_cb = callback;
    instance->location_checked_user_data = user_data;
}

void ap_client_set_connection_status_callback(APClientHandle handle,
                                               ConnectionStatusCallback callback,
                                               void* user_data) {
    if (!handle) {
        return;
    }

    auto* instance = static_cast<APClientInstance*>(handle);
    instance->connection_status_cb = callback;
    instance->connection_status_user_data = user_data;
}

void ap_client_set_registration_complete_callback(APClientHandle handle,
                                                   RegistrationCompleteCallback callback,
                                                   void* user_data) {
    if (!handle) {
        return;
    }

    auto* instance = static_cast<APClientInstance*>(handle);
    instance->registration_complete_cb = callback;
    instance->registration_complete_user_data = user_data;
}

const char* ap_client_get_last_error(APClientHandle handle) {
    if (!handle) {
        return "Invalid handle";
    }

    auto* instance = static_cast<APClientInstance*>(handle);
    return instance->last_error.c_str();
}

void ap_client_free_string(const char* str) {
    delete[] str;
}

bool ap_client_is_connected(APClientHandle handle) {
    if (!handle) {
        return false;
    }

    auto* instance = static_cast<APClientInstance*>(handle);
    return instance->ipc->is_connected();
}
