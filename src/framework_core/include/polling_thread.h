#pragma once
#include <thread>
#include <atomic>
#include <chrono>
#include "ap_client.h"
#include "message_router.h"

namespace APFramework {

/**
 * @brief Background thread for continuous AP server polling
 *
 * Runs a dedicated thread that continuously polls the AP client for incoming
 * messages at a configurable interval (default: 16ms for ~60fps).
 *
 * Responsibilities:
 * - Call APClientWrapper::poll() at regular intervals
 * - Retrieve pending AP messages from the client
 * - Route received messages to appropriate mods via MessageRouter
 * - Handle thread lifecycle (start, stop, graceful shutdown)
 *
 * The polling thread ensures non-blocking AP communication - the main thread
 * and IPC server thread never block waiting for AP messages.
 */
class PollingThread {
public:
    PollingThread(APClientWrapper* client, MessageRouter* router);
    ~PollingThread();

    // Thread lifecycle
    void start();
    void stop();
    bool is_running() const;

    // Configuration
    void set_poll_interval(std::chrono::milliseconds interval);
    std::chrono::milliseconds get_poll_interval() const;

private:
    void polling_loop();

    APClientWrapper* ap_client_;
    MessageRouter* router_;
    std::thread thread_;
    std::atomic<bool> running_;
    std::chrono::milliseconds poll_interval_;
};

} // namespace APFramework
