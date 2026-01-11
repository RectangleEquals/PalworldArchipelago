#include "ap_manager.h"
#include "ap_config.h"
#include "ap_logger.h"
#include "ap_ipc_server.h"
#include "ap_mod_registry.h"
#include "ap_capabilities_generator.h"
#include "ap_client.h"
#include "ap_message_router.h"
#include "ap_polling_thread.h"
#include <sstream>

namespace APFramework {

// Singleton instance
APManager& APManager::instance() {
    static APManager instance;
    return instance;
}

APManager::~APManager() {
    shutdown();
}

LifecyclePhase APManager::get_current_phase() const {
    return current_phase_.load();
}

bool APManager::is_running() const {
    return is_running_.load();
}

// ============================================================================
// Initialization
// ============================================================================

VoidResult APManager::init(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto& logger = APLogger::instance();
    auto& config = APConfig::instance();

    // Load configuration
    if (!config.load(config_path)) {
        std::string errors;
        for (const auto& err : config.get_errors()) {
            errors += err + "\n";
        }
        return VoidResult::failure(ErrorCode::CONFIG_ERROR,
            "Failed to load configuration: " + errors);
    }

    // Validate configuration
    if (!config.validate()) {
        std::string errors;
        for (const auto& err : config.get_errors()) {
            errors += err + "\n";
        }
        return VoidResult::failure(ErrorCode::CONFIG_ERROR,
            "Configuration validation failed: " + errors);
    }

    // Initialize logger
    if (!logger.init(config.get_log_level(),
                     config.get_log_file_path().string(),
                     config.should_log_to_console())) {
        return VoidResult::failure(ErrorCode::INIT_ERROR,
            "Failed to initialize logger");
    }

    logger.log(LogLevel::LOG_INFO, "APManager", "Initializing Archipelago Framework...");

    // Create components
    try {
        // IPC Server
        ipc_server_ = std::make_unique<APIPCServer>();
        auto ipc_result = ipc_server_->init(config.get_ipc_endpoint());
        if (!ipc_result.is_success()) {
            return VoidResult::failure(ErrorCode::IPC_ERROR,
                "Failed to initialize IPC server: " + ipc_result.error_message);
        }

        // Mod Registry
        mod_registry_ = std::make_unique<APModRegistry>(&logger);

        // Capabilities Generator
        capabilities_generator_ = std::make_unique<APCapabilitiesGenerator>(&logger);

        // AP Client
        ap_client_ = std::make_unique<APClient>(&logger);

        // Message Router
        message_router_ = std::make_unique<APMessageRouter>(&logger);

        // Polling Thread
        polling_thread_ = std::make_unique<APPollingThread>();

        logger.log(LogLevel::LOG_INFO, "APManager", "All components created successfully");

    } catch (const std::exception& e) {
        return VoidResult::failure(ErrorCode::INIT_ERROR,
            "Exception during component creation: " + std::string(e.what()));
    }

    // Setup console log routing if enabled
    if (config.should_log_to_console()) {
        setup_console_log_routing();
    }

    logger.log(LogLevel::LOG_INFO, "APManager", "Initialization complete");
    return VoidResult::success();
}

// ============================================================================
// Start & State Machine
// ============================================================================

VoidResult APManager::start() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto& logger = APLogger::instance();

    if (is_running_.load()) {
        return VoidResult::failure(ErrorCode::INTERNAL_ERROR,
            "APManager is already running");
    }

    // Start IPC server
    auto ipc_result = ipc_server_->start();
    if (!ipc_result.is_success()) {
        return VoidResult::failure(ErrorCode::IPC_ERROR,
            "Failed to start IPC server: " + ipc_result.error_message);
    }

    // Set IPC message callback
    ipc_server_->set_message_callback(
        [this](const IPCMessage& msg) {
            on_ipc_message(msg);
        }
    );

    // Start state machine thread
    should_stop_ = false;
    is_running_ = true;
    state_machine_thread_ = std::thread(&APManager::run_state_machine, this);

    logger.log(LogLevel::LOG_INFO, "APManager", "Framework started - state machine running");

    // Transition to first state
    transition_to(LifecyclePhase::DISCOVERING_MODS);

    return VoidResult::success();
}

void APManager::run_state_machine() {
    auto& logger = APLogger::instance();

    while (!should_stop_.load()) {
        LifecyclePhase phase = current_phase_.load();

        try {
            switch (phase) {
                case LifecyclePhase::DISCOVERING_MODS:
                    handle_discovering_mods();
                    break;

                case LifecyclePhase::AWAITING_PRIORITY_REGISTRATION:
                    handle_awaiting_priority_registration();
                    break;

                case LifecyclePhase::AWAITING_REGULAR_REGISTRATION:
                    handle_awaiting_regular_registration();
                    break;

                case LifecyclePhase::VALIDATING_CAPABILITIES:
                    handle_validating_capabilities();
                    break;

                case LifecyclePhase::READY_FOR_CONNECTION:
                    handle_ready_for_connection();
                    break;

                case LifecyclePhase::CONNECTING:
                    handle_connecting();
                    break;

                case LifecyclePhase::CONNECTED_AND_SYNCING:
                    handle_connected_and_syncing();
                    break;

                case LifecyclePhase::RUNNING:
                    handle_running();
                    break;

                case LifecyclePhase::ERROR_STATE:
                    // Wait in error state
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    break;

                case LifecyclePhase::UNINITIALIZED:
                case LifecyclePhase::GENERATING_CAPABILITIES:
                default:
                    // These states shouldn't be active during state machine loop
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    break;
            }

            // Check for timeouts
            check_timeouts();

        } catch (const std::exception& e) {
            logger.log(LogLevel::LOG_ERROR, "APManager",
                "Exception in state machine: " + std::string(e.what()));
            enter_error_state("State machine exception: " + std::string(e.what()));
        }

        // Small sleep to prevent busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    logger.log(LogLevel::LOG_INFO, "APManager", "State machine thread exiting");
}

// ============================================================================
// State Transition
// ============================================================================

void APManager::transition_to(LifecyclePhase new_phase) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto& logger = APLogger::instance();
    LifecyclePhase old_phase = current_phase_.load();

    if (old_phase == new_phase) {
        return;  // No change
    }

    // Log transition
    logger.log(LogLevel::LOG_INFO, "APManager",
        "State transition: " + std::to_string(static_cast<int>(old_phase)) +
        " -> " + std::to_string(static_cast<int>(new_phase)));

    current_phase_.store(new_phase);
}

void APManager::enter_error_state(const std::string& error_msg) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto& logger = APLogger::instance();

    // Log error
    logger.log(LogLevel::LOG_FATAL, "APManager", "ENTERING ERROR STATE: " + error_msg);

    // Store error message
    error_message_ = error_msg;

    // Transition to error state
    current_phase_.store(LifecyclePhase::ERROR_STATE);

    // Broadcast error to all connected mods
    if (ipc_server_) {
        IPCMessage error_msg_ipc;
        error_msg_ipc.type = "error";
        error_msg_ipc.from_mod_id = "framework";
        error_msg_ipc.data = {
            {"error_type", "FRAMEWORK_ERROR"},
            {"message", error_msg}
        };
        ipc_server_->broadcast(error_msg_ipc);
    }

    // Stop polling if active
    if (polling_thread_ && polling_thread_->is_running()) {
        polling_thread_->stop();
    }
}

// ============================================================================
// Phase Handlers
// ============================================================================

void APManager::handle_discovering_mods() {
    // Only run once
    static bool discovery_done = false;
    if (discovery_done) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return;
    }

    auto& logger = APLogger::instance();
    auto& config = APConfig::instance();

    logger.log(LogLevel::LOG_INFO, "APManager", "Starting mod discovery...");

    // Discover mods
    auto result = mod_registry_->discover_mods(config.get_mods_directory());
    if (!result.is_success()) {
        enter_error_state("Mod discovery failed: " + result.error_message);
        return;
    }

    size_t discovered_count = mod_registry_->get_discovered_count();
    size_t priority_count = mod_registry_->get_priority_count();

    logger.log(LogLevel::LOG_INFO, "APManager",
        "Discovered " + std::to_string(discovered_count) + " mods (" +
        std::to_string(priority_count) + " priority)");

    if (discovered_count == 0) {
        enter_error_state("No mods discovered in " + config.get_mods_directory().string());
        return;
    }

    discovery_done = true;

    // Transition based on priority mods
    if (priority_count > 0) {
        priority_registration_start_ = std::chrono::steady_clock::now();
        transition_to(LifecyclePhase::AWAITING_PRIORITY_REGISTRATION);
    } else {
        regular_registration_start_ = std::chrono::steady_clock::now();
        transition_to(LifecyclePhase::AWAITING_REGULAR_REGISTRATION);
    }
}

void APManager::handle_awaiting_priority_registration() {
    // Check if all priority mods registered
    if (mod_registry_->all_priority_mods_registered()) {
        auto& logger = APLogger::instance();
        logger.log(LogLevel::LOG_INFO, "APManager", "All priority mods registered");

        // Reset for regular registration phase
        regular_registration_start_ = std::chrono::steady_clock::now();
        transition_to(LifecyclePhase::AWAITING_REGULAR_REGISTRATION);
    }
    // Timeout handling is in check_timeouts()
}

void APManager::handle_awaiting_regular_registration() {
    // Check if all mods registered
    if (mod_registry_->all_mods_registered()) {
        auto& logger = APLogger::instance();
        logger.log(LogLevel::LOG_INFO, "APManager", "All mods registered");
        transition_to(LifecyclePhase::VALIDATING_CAPABILITIES);
    }
    // Timeout handling is in check_timeouts()
}

void APManager::handle_validating_capabilities() {
    // Only run once
    static bool validation_done = false;
    if (validation_done) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return;
    }

    auto& logger = APLogger::instance();
    auto& config = APConfig::instance();

    logger.log(LogLevel::LOG_INFO, "APManager", "Validating capabilities...");

    // Get all registered mods
    auto mods = mod_registry_->get_registered_mods();

    // Generate aggregated capabilities
    auto cap_result = capabilities_generator_->generate_capabilities(
        mods, config.get_game_name());

    if (!cap_result.is_success()) {
        enter_error_state("Capability generation failed: " + cap_result.error_message);
        return;
    }

    aggregated_capabilities_ = cap_result.value;

    // Detect conflicts
    auto conflicts = capabilities_generator_->detect_conflicts(aggregated_capabilities_);
    if (!conflicts.empty()) {
        std::ostringstream oss;
        oss << "Capability conflicts detected:\n";
        for (const auto& conflict : conflicts) {
            oss << "  - " << conflict.description << ": " << conflict.details << "\n";
            oss << "    Involved mods: ";
            for (size_t i = 0; i < conflict.involved_mods.size(); ++i) {
                oss << conflict.involved_mods[i];
                if (i < conflict.involved_mods.size() - 1) oss << ", ";
            }
            oss << "\n";
        }
        enter_error_state(oss.str());
        return;
    }

    // Compute checksum
    capabilities_checksum_ = capabilities_generator_->compute_checksum(aggregated_capabilities_);
    logger.log(LogLevel::LOG_INFO, "APManager", "Capabilities checksum: " + capabilities_checksum_);

    // Build message router subscriptions
    message_router_->build_subscriptions(aggregated_capabilities_);
    message_router_->set_ipc_server(ipc_server_.get());

    logger.log(LogLevel::LOG_INFO, "APManager", "Capabilities validated successfully");

    validation_done = true;
    transition_to(LifecyclePhase::READY_FOR_CONNECTION);
}

void APManager::handle_ready_for_connection() {
    // Just wait for CMD_CONNECT command
    // Do nothing - idle state
}

void APManager::handle_connecting() {
    // Connection is initiated by handle_command
    // This handler just waits for the async callback
    // Connection callback will transition to CONNECTED_AND_SYNCING or ERROR_STATE
}

void APManager::handle_connected_and_syncing() {
    // Only run once
    static bool sync_started = false;
    if (sync_started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return;
    }

    auto& logger = APLogger::instance();
    auto& config = APConfig::instance();

    logger.log(LogLevel::LOG_INFO, "APManager", "Starting polling thread...");

    // Initialize and start polling thread
    polling_thread_->init(
        ap_client_.get(),
        message_router_.get(),
        this,
        config.get_polling_interval()
    );

    auto poll_result = polling_thread_->start();
    if (!poll_result.is_success()) {
        enter_error_state("Failed to start polling thread: " + poll_result.error_message);
        return;
    }

    sync_started = true;

    // Transition to running state
    // In a real implementation, we might wait for initial sync messages
    // For now, transition immediately
    logger.log(LogLevel::LOG_INFO, "APManager", "Connected and syncing - transitioning to RUNNING");
    transition_to(LifecyclePhase::RUNNING);
}

void APManager::handle_running() {
    // Normal operation - polling thread is active
    // Just monitor and respond to commands
}

// ============================================================================
// IPC Message Handler
// ============================================================================

void APManager::on_ipc_message(const IPCMessage& msg) {
    auto& logger = APLogger::instance();

    try {
        if (msg.type == "register") {
            // Mod registration
            std::string mod_id = msg.data["mod_id"].get<std::string>();
            nlohmann::json capabilities = msg.data["capabilities"];
            bool is_priority = msg.data.value("is_priority", false);

            logger.log(LogLevel::LOG_INFO, "APManager",
                "Received registration from mod: " + mod_id);

            auto result = mod_registry_->register_mod(mod_id, capabilities, is_priority);
            if (!result.is_success()) {
                logger.log(LogLevel::LOG_WARN, "APManager",
                    "Failed to register mod " + mod_id + ": " + result.error_message);
            }

            // Check if we can advance phase
            LifecyclePhase phase = current_phase_.load();
            if (phase == LifecyclePhase::AWAITING_PRIORITY_REGISTRATION) {
                if (is_priority && mod_registry_->all_priority_mods_registered()) {
                    logger.log(LogLevel::LOG_INFO, "APManager",
                        "All priority mods registered - advancing phase");
                }
            } else if (phase == LifecyclePhase::AWAITING_REGULAR_REGISTRATION) {
                if (mod_registry_->all_mods_registered()) {
                    logger.log(LogLevel::LOG_INFO, "APManager",
                        "All mods registered - advancing phase");
                }
            }

        } else if (msg.type == "command") {
            // Command from priority client
            std::string cmd = msg.data["cmd"].get<std::string>();
            nlohmann::json cmd_data = msg.data.value("data", nlohmann::json::object());

            logger.log(LogLevel::LOG_INFO, "APManager",
                "Received command: " + cmd + " from " + msg.from_mod_id);

            auto result = handle_command(cmd, cmd_data);
            if (!result.is_success()) {
                logger.log(LogLevel::LOG_WARN, "APManager",
                    "Command failed: " + result.error_message);
            }

        } else {
            // Other message types - could be routed through message router
            // For now, just log
            logger.log(LogLevel::LOG_DEBUG, "APManager",
                "Received IPC message type: " + msg.type);
        }

    } catch (const std::exception& e) {
        logger.log(LogLevel::LOG_ERROR, "APManager",
            "Exception handling IPC message: " + std::string(e.what()));
    }
}

// ============================================================================
// Command Handling
// ============================================================================

VoidResult APManager::handle_command(const std::string& cmd, const nlohmann::json& data) {
    auto& logger = APLogger::instance();
    auto& config = APConfig::instance();

    if (cmd == "CONNECT") {
        // Validate phase
        if (current_phase_.load() != LifecyclePhase::READY_FOR_CONNECTION) {
            return VoidResult::failure(ErrorCode::INTERNAL_ERROR,
                "Cannot connect - not in READY_FOR_CONNECTION state");
        }

        // Extract connection parameters (or use config defaults)
        std::string server = data.value("server", config.get_server());
        int port = data.value("port", config.get_port());
        std::string slot_name = data.value("slot_name", config.get_slot_name());
        std::string password = data.value("password", config.get_password());

        logger.log(LogLevel::LOG_INFO, "APManager",
            "Connecting to " + server + ":" + std::to_string(port) + " as " + slot_name);

        // Transition to connecting state
        transition_to(LifecyclePhase::CONNECTING);
        connection_initiated_ = true;
        connection_start_ = std::chrono::steady_clock::now();

        // Initiate async connection
        ap_client_->connect_async(
            server, port, slot_name, password,
            config.get_connection_timeout(),
            [this](bool success, const std::string& error) {
                if (success) {
                    auto& logger = APLogger::instance();
                    logger.log(LogLevel::LOG_INFO, "APManager", "Connected to AP server");
                    transition_to(LifecyclePhase::CONNECTED_AND_SYNCING);
                } else {
                    enter_error_state("AP connection failed: " + error);
                }
                connection_initiated_ = false;
            }
        );

        return VoidResult::success();

    } else if (cmd == "GENERATE") {
        // Save capabilities to file
        std::string output_path = data.value("output_path", "capabilities.json");

        logger.log(LogLevel::LOG_INFO, "APManager",
            "Generating capabilities file: " + output_path);

        auto result = capabilities_generator_->save_to_file(
            aggregated_capabilities_,
            output_path
        );

        if (!result.is_success()) {
            return VoidResult::failure(ErrorCode::CONFIG_ERROR,
                "Failed to save capabilities: " + result.error_message);
        }

        return VoidResult::success();

    } else if (cmd == "DISCONNECT") {
        // Gracefully disconnect from AP server
        if (current_phase_.load() != LifecyclePhase::RUNNING) {
            return VoidResult::failure(ErrorCode::INTERNAL_ERROR,
                "Cannot disconnect - not in RUNNING state");
        }

        logger.log(LogLevel::LOG_INFO, "APManager", "Disconnecting from AP server");

        // Stop polling
        if (polling_thread_ && polling_thread_->is_running()) {
            polling_thread_->stop();
        }

        // Disconnect AP client
        ap_client_->disconnect();

        // Transition back to ready state
        transition_to(LifecyclePhase::READY_FOR_CONNECTION);

        return VoidResult::success();

    } else if (cmd == "RESYNC") {
        return trigger_resync();

    } else {
        return VoidResult::failure(ErrorCode::INTERNAL_ERROR,
            "Unknown command: " + cmd);
    }
}

// ============================================================================
// Resync Mechanism
// ============================================================================

VoidResult APManager::trigger_resync() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto& logger = APLogger::instance();

    if (current_phase_.load() != LifecyclePhase::RUNNING) {
        return VoidResult::failure(ErrorCode::INTERNAL_ERROR,
            "Cannot resync - not in RUNNING state");
    }

    logger.log(LogLevel::LOG_INFO, "APManager", "Triggering resync...");

    // Stop polling thread
    if (polling_thread_ && polling_thread_->is_running()) {
        polling_thread_->stop();
    }

    // Disconnect from AP server
    ap_client_->disconnect();

    // Clear message router subscriptions
    message_router_->clear();

    // Clear mod registry
    mod_registry_->clear();

    // Clear capabilities
    std::string old_checksum = capabilities_checksum_;
    aggregated_capabilities_ = nlohmann::json();
    capabilities_checksum_.clear();

    // Transition back to discovering mods
    logger.log(LogLevel::LOG_INFO, "APManager", "Resync: restarting mod discovery");

    // Reset discovery flag
    // Note: This is a simplified approach - in production might need better state management
    transition_to(LifecyclePhase::DISCOVERING_MODS);

    return VoidResult::success();
}

// ============================================================================
// Timeout Checking
// ============================================================================

void APManager::check_timeouts() {
    auto& logger = APLogger::instance();
    auto& config = APConfig::instance();
    auto now = std::chrono::steady_clock::now();

    LifecyclePhase phase = current_phase_.load();

    // Priority registration timeout
    if (phase == LifecyclePhase::AWAITING_PRIORITY_REGISTRATION) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - priority_registration_start_);

        if (elapsed >= config.get_priority_registration_timeout()) {
            logger.log(LogLevel::LOG_WARN, "APManager",
                "Priority registration timeout - continuing with registered mods");

            // Move to regular registration phase anyway
            regular_registration_start_ = std::chrono::steady_clock::now();
            transition_to(LifecyclePhase::AWAITING_REGULAR_REGISTRATION);
        }
    }

    // Regular registration timeout
    if (phase == LifecyclePhase::AWAITING_REGULAR_REGISTRATION) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - regular_registration_start_);

        if (elapsed >= config.get_registration_timeout()) {
            logger.log(LogLevel::LOG_WARN, "APManager",
                "Regular registration timeout - continuing with registered mods");

            // Move to validation phase anyway
            transition_to(LifecyclePhase::VALIDATING_CAPABILITIES);
        }
    }

    // Connection timeout
    if (phase == LifecyclePhase::CONNECTING && connection_initiated_.load()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - connection_start_);

        if (elapsed >= config.get_connection_timeout()) {
            enter_error_state("Connection timeout after " +
                std::to_string(config.get_connection_timeout().count()) + "ms");
            connection_initiated_ = false;
        }
    }
}

// ============================================================================
// Console Log Routing
// ============================================================================

void APManager::setup_console_log_routing() {
    auto& logger = APLogger::instance();

    logger.set_log_callback([this](LogLevel level, const std::string& msg) {
        // Convert LogLevel to string
        std::string level_str;
        switch (level) {
            case LogLevel::LOG_TRACE: level_str = "TRACE"; break;
            case LogLevel::LOG_DEBUG: level_str = "DEBUG"; break;
            case LogLevel::LOG_INFO:  level_str = "INFO";  break;
            case LogLevel::LOG_WARN:  level_str = "WARN";  break;
            case LogLevel::LOG_ERROR: level_str = "ERROR"; break;
            case LogLevel::LOG_FATAL: level_str = "FATAL"; break;
            default: level_str = "UNKNOWN"; break;
        }

        // Send to priority clients only
        if (ipc_server_) {
            IPCMessage log_msg;
            log_msg.type = "log";
            log_msg.from_mod_id = "framework";
            log_msg.data = {
                {"level", level_str},
                {"message", msg}
            };

            ipc_server_->broadcast_to_priority_clients(log_msg);
        }
    });

    logger.log(LogLevel::LOG_INFO, "APManager", "Console log routing to priority clients enabled");
}

// ============================================================================
// Shutdown
// ============================================================================

void APManager::shutdown() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto& logger = APLogger::instance();

    if (!is_running_.load()) {
        return;  // Already shutdown
    }

    logger.log(LogLevel::LOG_INFO, "APManager", "Shutting down...");

    // Signal state machine to stop
    should_stop_ = true;

    // Wait for state machine thread
    if (state_machine_thread_.joinable()) {
        state_machine_thread_.join();
    }

    // Stop polling thread
    if (polling_thread_ && polling_thread_->is_running()) {
        polling_thread_->stop();
    }

    // Disconnect AP client
    if (ap_client_) {
        ap_client_->disconnect();
    }

    // Stop IPC server
    if (ipc_server_) {
        ipc_server_->stop();
    }

    // Clear components (in reverse order)
    polling_thread_.reset();
    message_router_.reset();
    ap_client_.reset();
    capabilities_generator_.reset();
    mod_registry_.reset();
    ipc_server_.reset();

    // Shutdown logger
    logger.log(LogLevel::LOG_INFO, "APManager", "Shutdown complete");
    logger.shutdown();

    // Transition to uninitialized
    current_phase_.store(LifecyclePhase::UNINITIALIZED);
    is_running_ = false;
}

} // namespace APFramework
