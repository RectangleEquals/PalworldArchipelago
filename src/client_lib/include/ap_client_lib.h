#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Opaque handle to client instance
typedef void* APClientHandle;

// Callback function types
typedef void (*ItemReceivedCallback)(int64_t item_id, int64_t location_id, int player_slot, void* user_data);
typedef void (*LocationCheckedCallback)(int64_t location_id, void* user_data);
typedef void (*ConnectionStatusCallback)(bool connected, const char* slot_name, void* user_data);
typedef void (*RegistrationCompleteCallback)(void* user_data);

// Lifecycle functions
APClientHandle ap_client_create(const char* mod_id);
void ap_client_destroy(APClientHandle handle);

// Registration
bool ap_client_register(APClientHandle handle, const char* capabilities_json);

// Polling - call each frame to process messages
void ap_client_poll(APClientHandle handle);

// Commands to send to framework
void ap_client_check_location(APClientHandle handle, int64_t location_id);
void ap_client_request_connection(APClientHandle handle,
                                   const char* server, int port,
                                   const char* slot_name, const char* password);

// Callback registration
void ap_client_set_item_received_callback(APClientHandle handle,
                                           ItemReceivedCallback callback,
                                           void* user_data);
void ap_client_set_location_checked_callback(APClientHandle handle,
                                              LocationCheckedCallback callback,
                                              void* user_data);
void ap_client_set_connection_status_callback(APClientHandle handle,
                                               ConnectionStatusCallback callback,
                                               void* user_data);
void ap_client_set_registration_complete_callback(APClientHandle handle,
                                                   RegistrationCompleteCallback callback,
                                                   void* user_data);

// Utility functions
const char* ap_client_get_last_error(APClientHandle handle);
void ap_client_free_string(const char* str);
bool ap_client_is_connected(APClientHandle handle);

#ifdef __cplusplus
}
#endif
