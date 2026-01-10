#include "ap_polling_thread.h"
#include "ap_client.h"
#include "ap_message_router.h"
#include "ap_manager.h"
#include "ap_logger.h"

namespace APFramework {

APPollingThread::APPollingThread() {
}

APPollingThread::~APPollingThread() {
    stop();
}

void APPollingThread::init(
    APClient* ap_client,
    APMessageRouter* message_router,
    APManager* ap_manager,
    std::chrono::milliseconds poll_interval
) {
    ap_client_ = ap_client;
    message_router_ = message_router;
    ap_manager_ = ap_manager;
    poll_interval_ = poll_interval;
}

void APPollingThread::start() {
    if (is_running_) {
        return;  // Already running
    }

    if (!ap_client_ || !message_router_ || !ap_manager_) {
        // Cannot start without dependencies
        return;
    }

    should_stop_ = false;
    is_running_ = true;

    polling_thread_ = std::thread([this]() {
        polling_loop();
    });
}

void APPollingThread::stop() {
    if (!is_running_) {
        return;  // Not running
    }

    should_stop_ = true;

    if (polling_thread_.joinable()) {
        polling_thread_.join();
    }

    is_running_ = false;
}

bool APPollingThread::is_running() const {
    return is_running_;
}

void APPollingThread::set_poll_interval(std::chrono::milliseconds interval) {
    poll_interval_ = interval;
}

void APPollingThread::polling_loop() {
    auto last_poll = std::chrono::steady_clock::now();

    while (!should_stop_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_poll);

        // Only poll when the configured interval has elapsed
        if (elapsed >= poll_interval_) {
            last_poll = now;

            try {
                // Check lifecycle state - only poll when in appropriate states
                auto current_phase = ap_manager_->get_current_phase();

                // Only poll and route messages when connected to AP server
                if (current_phase == LifecyclePhase::CONNECTED_AND_SYNCING ||
                    current_phase == LifecyclePhase::RUNNING) {

                    // Poll the AP client (processes WebSocket events)
                    ap_client_->poll();

                    // Get any new messages
                    auto messages = ap_client_->get_messages();

                    // Route each message
                    for (const auto& msg : messages) {
                        message_router_->route_ap_message(msg);
                    }
                }
                // In other states (VALIDATING_CAPABILITIES, GENERATING_CAPABILITIES, etc.),
                // skip polling to avoid processing messages during inconsistent framework state

            } catch (const std::exception& e) {
                // Log error but continue polling
                // In production, APManager should be notified of critical errors
                // For now, just silently continue
            }
        }

        // Small sleep to prevent busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

} // namespace APFramework