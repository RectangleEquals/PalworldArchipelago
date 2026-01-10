#pragma once
#include "ap_types.h"
#include <thread>
#include <atomic>
#include <chrono>

namespace APFramework {

// Forward declarations
class APClient;
class APMessageRouter;
class APManager;

/**
 * APPollingThread - Dedicated thread for polling APClient at 60fps
 *
 * Responsibilities:
 * - Poll APClient at configured interval (default 16ms / 60fps)
 * - Check lifecycle state before polling (only poll in appropriate states)
 * - Retrieve messages from APClient and route them via APMessageRouter
 * - Graceful start/stop with thread safety
 *
 * CRITICAL: Uses time accumulation to ensure true polling rate,
 * not sleep-based intervals (which would be poll_interval + execution_time)
 */
class APPollingThread {
public:
    APPollingThread();
    ~APPollingThread();

    // Initialize with dependencies
    void init(
        APClient* ap_client,
        APMessageRouter* message_router,
        APManager* ap_manager,
        std::chrono::milliseconds poll_interval = std::chrono::milliseconds(16)  // 60fps
    );

    // Start polling thread
    void start();

    // Stop polling thread (blocks until thread completes)
    void stop();

    // Check if thread is running
    bool is_running() const;

    // Set polling interval (can be called while running)
    void set_poll_interval(std::chrono::milliseconds interval);

private:
    // Main polling loop (runs in dedicated thread)
    void polling_loop();

    APClient* ap_client_{nullptr};              // Non-owning
    APMessageRouter* message_router_{nullptr};  // Non-owning
    APManager* ap_manager_{nullptr};            // Non-owning (for lifecycle state checking)

    std::thread polling_thread_;
    std::atomic<bool> should_stop_{false};
    std::atomic<bool> is_running_{false};

    std::chrono::milliseconds poll_interval_{16};  // Default 60fps
};

} // namespace APFramework