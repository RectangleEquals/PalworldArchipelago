#include "ap_client_lib.h"
#include <sol/sol.hpp>

namespace APClientLib {

/**
 * Register APClientLib bindings with a Lua state
 *
 * This function exposes the APClient class and related types to Lua,
 * allowing mods to communicate with the APFramework.
 *
 * @param lua The sol::state to register bindings with
 */
void register_apclient_bindings(sol::state& lua) {
    // Register VoidResult type
    lua.new_usertype<VoidResult>("APClientResult",
        sol::no_constructor,
        "success", sol::readonly(&VoidResult::success),
        "error_message", sol::readonly(&VoidResult::error_message),
        "is_success", &VoidResult::is_success,
        "is_failure", &VoidResult::is_failure
    );

    // Register IPCMessage type
    lua.new_usertype<IPCMessage>("IPCMessage",
        sol::no_constructor,
        "type", &IPCMessage::type,
        "from_mod_id", &IPCMessage::from_mod_id,
        "to_mod_id", &IPCMessage::to_mod_id,
        "msg_id", &IPCMessage::msg_id,
        "data", &IPCMessage::data
    );

    // Register APClient class
    lua.new_usertype<APClient>("APClientNative",
        sol::constructors<APClient()>(),

        // Initialization
        "init", sol::overload(
            [](APClient& self, const std::string& mod_id) {
                return self.init(mod_id);
            },
            [](APClient& self, const std::string& mod_id, const std::string& pipe_name) {
                return self.init(mod_id, pipe_name);
            }
        ),

        // Registration
        "register_with_framework", [](APClient& self, sol::table capabilities, bool is_priority) {
            // Convert Lua table to JSON
            nlohmann::json cap_json = nlohmann::json::object();

            // Simple conversion - would need recursive handling for complex structures
            for (const auto& pair : capabilities) {
                std::string key = pair.first.as<std::string>();
                auto value = pair.second;

                if (value.is<std::string>()) {
                    cap_json[key] = value.as<std::string>();
                } else if (value.is<int>()) {
                    cap_json[key] = value.as<int>();
                } else if (value.is<double>()) {
                    cap_json[key] = value.as<double>();
                } else if (value.is<bool>()) {
                    cap_json[key] = value.as<bool>();
                } else if (value.is<sol::table>()) {
                    // Nested table - would need recursive handling
                    cap_json[key] = nlohmann::json::object();
                }
            }

            return self.register_with_framework(cap_json, is_priority);
        },

        // Sending messages
        "send_location_check", &APClient::send_location_check,
        "send_location_checks", &APClient::send_location_checks,
        "send_message", &APClient::send_message,
        "send_command", [](APClient& self, const std::string& cmd, sol::optional<sol::table> data_table) {
            nlohmann::json data = nlohmann::json::object();

            if (data_table) {
                // Convert Lua table to JSON
                for (const auto& pair : *data_table) {
                    std::string key = pair.first.as<std::string>();
                    auto value = pair.second;

                    if (value.is<std::string>()) {
                        data[key] = value.as<std::string>();
                    } else if (value.is<int>()) {
                        data[key] = value.as<int>();
                    } else if (value.is<double>()) {
                        data[key] = value.as<double>();
                    } else if (value.is<bool>()) {
                        data[key] = value.as<bool>();
                    }
                }
            }

            return self.send_command(cmd, data);
        },

        // Receiving messages
        "poll", &APClient::poll,
        "get_messages", &APClient::get_messages,

        // Callbacks
        "on_received_items", &APClient::on_received_items,
        "on_location_info", &APClient::on_location_info,
        "on_lifecycle_change", &APClient::on_lifecycle_change,
        "on_message", &APClient::on_message,

        // Status
        "is_connected", &APClient::is_connected,
        "get_mod_id", &APClient::get_mod_id
    );
}

} // namespace APClientLib

// Lua module entry point (called by require("APClientLib"))
// This receives the existing Lua state from UE4SS and registers our C++ bindings into it
extern "C" {
    __declspec(dllexport) int luaopen_APClientLib(lua_State* L) {
        // Create sol::state_view from UE4SS's existing Lua state
        sol::state_view lua(L);

        // Register all APClientLib bindings into this state
        APClientLib::register_apclient_bindings(lua);

        // Return 1 to indicate module loaded successfully
        return 1;
    }
}