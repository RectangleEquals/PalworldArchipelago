#pragma once
#include "ap_types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>

namespace APFramework {

// Forward declarations
class APIPCServer;
class APModRegistry;
class APCapabilitiesGenerator;
class APClient;
class APMessageRouter;
class APPollingThread;
struct IPCMessage;

/**
 * APManager - Main orchestrator for the Archipelago Framework
 *
 * Responsibilities:
 * - Manages full lifecycle state machine
 * - Coordinates all framework components
 * - Handles mod registration (priority vs regular)
 * - Validates capabilities and detects conflicts
 * - Routes console logs to priority clients
 * - Handles commands from priority clients
 * - Manages resync with checksum validation
 */
class APManager {
public:
    // Singleton access
    static APManager& instance();

    // Delete copy/move
    APManager(const APManager&) = delete;
    APManager& operator=(const APManager&) = delete;

    // Core lifecycle methods
    VoidResult init(const std::string& config_path);
    VoidResult start();
    void shutdown();

    // State queries
    LifecyclePhase get_current_phase() const;
    bool is_running() const;

    // Command handling (from priority clients)
    VoidResult handle_command(const std::string& cmd, const nlohmann::json& data);

    // Resync mechanism
    VoidResult trigger_resync();

private:
    APManager() = default;
    ~APManager();

    // State machine
    void transition_to(LifecyclePhase new_phase);
    void enter_error_state(const std::string& error_msg);
    void run_state_machine();

    // Phase handlers
    void handle_discovering_mods();
    void handle_awaiting_priority_registration();
    void handle_awaiting_regular_registration();
    void handle_validating_capabilities();
    void handle_ready_for_connection();
    void handle_connecting();
    void handle_connected_and_syncing();
    void handle_running();

    // IPC message handler
    void on_ipc_message(const IPCMessage& msg);

    // Timeout checking
    void check_timeouts();

    // Console log routing setup
    void setup_console_log_routing();

    // Components (owned)
    std::unique_ptr<APIPCServer> ipc_server_;
    std::unique_ptr<APModRegistry> mod_registry_;
    std::unique_ptr<APCapabilitiesGenerator> capabilities_generator_;
    std::unique_ptr<APClient> ap_client_;
    std::unique_ptr<APMessageRouter> message_router_;
    std::unique_ptr<APPollingThread> polling_thread_;

    // Generated capabilities
    nlohmann::json aggregated_capabilities_;
    std::string capabilities_checksum_;

    // State tracking
    std::atomic<LifecyclePhase> current_phase_{LifecyclePhase::UNINITIALIZED};
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};

    // Timeout tracking
    std::chrono::steady_clock::time_point priority_registration_start_;
    std::chrono::steady_clock::time_point regular_registration_start_;
    std::chrono::steady_clock::time_point connection_start_;

    // Connection state
    std::atomic<bool> connection_initiated_{false};

    // Thread safety
    mutable std::recursive_mutex state_mutex_;
    std::thread state_machine_thread_;

    // Error state info
    std::string error_message_;
};

} // namespace APFramework
