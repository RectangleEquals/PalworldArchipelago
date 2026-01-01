#include "polling_thread.h"
#include <thread>

namespace APFramework {

PollingThread::PollingThread(APClientWrapper* client, MessageRouter* router)
    : ap_client_(client),
      router_(router),
      running_(false),
      poll_interval_(16) { // Default: 16ms (~60fps)
}

PollingThread::~PollingThread() {
    stop();
}

void PollingThread::start() {
    if (running_.load()) {
        return;
    }

    running_.store(true);
    thread_ = std::thread(&PollingThread::polling_loop, this);
}

void PollingThread::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    if (thread_.joinable()) {
        thread_.join();
    }
}

bool PollingThread::is_running() const {
    return running_.load();
}

void PollingThread::set_poll_interval(std::chrono::milliseconds interval) {
    poll_interval_ = interval;
}

std::chrono::milliseconds PollingThread::get_poll_interval() const {
    return poll_interval_;
}

void PollingThread::polling_loop() {
    while (running_.load()) {
        // Poll the AP client for new messages
        if (ap_client_) {
            ap_client_->poll();

            // Get all pending messages
            std::vector<APMessage> messages = ap_client_->get_messages();

            // Route each message
            if (router_) {
                for (const auto& msg : messages) {
                    router_->route_ap_message(msg);
                }
            }
        }

        // Sleep for the configured interval
        std::this_thread::sleep_for(poll_interval_);
    }
}

} // namespace APFramework
