#include "lua_bindings.h"
#include "ap_manager.h"
#include "ap_logger.h"
#include "ap_types.h"
#include <sol/sol.hpp>
#include <string>

namespace APFramework {

void register_apframework_bindings(sol::state& lua) {
    // Register LifecyclePhase enum
    lua.new_enum<LifecyclePhase>("LifecyclePhase",
        {
            {"UNINITIALIZED", LifecyclePhase::UNINITIALIZED},
            {"DISCOVERING_MODS", LifecyclePhase::DISCOVERING_MODS},
            {"AWAITING_PRIORITY_REGISTRATION", LifecyclePhase::AWAITING_PRIORITY_REGISTRATION},
            {"AWAITING_REGULAR_REGISTRATION", LifecyclePhase::AWAITING_REGULAR_REGISTRATION},
            {"VALIDATING_CAPABILITIES", LifecyclePhase::VALIDATING_CAPABILITIES},
            {"GENERATING_CAPABILITIES", LifecyclePhase::GENERATING_CAPABILITIES},
            {"READY_FOR_CONNECTION", LifecyclePhase::READY_FOR_CONNECTION},
            {"CONNECTING", LifecyclePhase::CONNECTING},
            {"CONNECTED_AND_SYNCING", LifecyclePhase::CONNECTED_AND_SYNCING},
            {"RUNNING", LifecyclePhase::RUNNING},
            {"ERROR_STATE", LifecyclePhase::ERROR_STATE}
        }
    );

    // Register LogLevel enum
    lua.new_enum<LogLevel>("LogLevel",
        {
            {"LOG_TRACE", LogLevel::LOG_TRACE},
            {"LOG_DEBUG", LogLevel::LOG_DEBUG},
            {"LOG_INFO", LogLevel::LOG_INFO},
            {"LOG_WARN", LogLevel::LOG_WARN},
            {"LOG_ERROR", LogLevel::LOG_ERROR},
            {"LOG_FATAL", LogLevel::LOG_FATAL}
        }
    );

    // Register VoidResult type
    // Since VoidResult doesn't have a value, we just need success/error
    lua.new_usertype<VoidResult>("VoidResult",
        sol::no_constructor,
        "is_success", &VoidResult::is_success,
        "error_message", sol::readonly(&VoidResult::error_message)
    );

    // Register APManager singleton
    lua.new_usertype<APManager>("APManager",
        sol::no_constructor,

        // Singleton access
        "instance", &APManager::instance,

        // Core lifecycle methods
        "init", &APManager::init,
        "start", &APManager::start,
        "shutdown", &APManager::shutdown,

        // State queries
        "get_current_phase", &APManager::get_current_phase,
        "is_running", &APManager::is_running,

        // Command handling
        "handle_command", [](APManager& self, const std::string& cmd, sol::optional<sol::table> data_table) {
            nlohmann::json data = nlohmann::json::object();

            if (data_table) {
                // Convert Lua table to JSON
                // Note: This is a simplified conversion - real implementation would need recursive handling
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

            return self.handle_command(cmd, data);
        },

        // Resync mechanism
        "trigger_resync", &APManager::trigger_resync
    );

    // Register APLogger singleton
    lua.new_usertype<APLogger>("APLogger",
        sol::no_constructor,

        // Singleton access
        "instance", &APLogger::instance,

        // Logging methods
        "log", [](APLogger& self, LogLevel level, const std::string& component, const std::string& message) {
            self.log(level, component, message);
        },
        "trace", [](APLogger& self, const std::string& message) {
            self.log(LogLevel::LOG_TRACE, "Lua", message);
        },
        "debug", [](APLogger& self, const std::string& message) {
            self.log(LogLevel::LOG_DEBUG, "Lua", message);
        },
        "info", [](APLogger& self, const std::string& message) {
            self.log(LogLevel::LOG_INFO, "Lua", message);
        },
        "warn", [](APLogger& self, const std::string& message) {
            self.log(LogLevel::LOG_WARN, "Lua", message);
        },
        "error", [](APLogger& self, const std::string& message) {
            self.log(LogLevel::LOG_ERROR, "Lua", message);
        },
        "fatal", [](APLogger& self, const std::string& message) {
            self.log(LogLevel::LOG_FATAL, "Lua", message);
        }
    );

    // Helper function to convert LifecyclePhase to string (for debugging)
    lua.set_function("lifecycle_phase_to_string", [](LifecyclePhase phase) -> std::string {
        switch (phase) {
            case LifecyclePhase::UNINITIALIZED: return "UNINITIALIZED";
            case LifecyclePhase::DISCOVERING_MODS: return "DISCOVERING_MODS";
            case LifecyclePhase::AWAITING_PRIORITY_REGISTRATION: return "AWAITING_PRIORITY_REGISTRATION";
            case LifecyclePhase::AWAITING_REGULAR_REGISTRATION: return "AWAITING_REGULAR_REGISTRATION";
            case LifecyclePhase::VALIDATING_CAPABILITIES: return "VALIDATING_CAPABILITIES";
            case LifecyclePhase::GENERATING_CAPABILITIES: return "GENERATING_CAPABILITIES";
            case LifecyclePhase::READY_FOR_CONNECTION: return "READY_FOR_CONNECTION";
            case LifecyclePhase::CONNECTING: return "CONNECTING";
            case LifecyclePhase::CONNECTED_AND_SYNCING: return "CONNECTED_AND_SYNCING";
            case LifecyclePhase::RUNNING: return "RUNNING";
            case LifecyclePhase::ERROR_STATE: return "ERROR_STATE";
            default: return "UNKNOWN";
        }
    });
}

} // namespace APFramework