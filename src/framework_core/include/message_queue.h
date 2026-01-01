#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <cstddef>

namespace APFramework {

/**
 * @brief Thread-safe message queue for inter-thread communication
 *
 * Provides a thread-safe queue implementation with blocking and non-blocking
 * operations. Used throughout the framework for passing messages between
 * the IPC server thread, polling thread, and main thread.
 *
 * @tparam T The type of messages stored in the queue
 */
template<typename T>
class MessageQueue {
public:
    void push(const T& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(message);
        cond_var_.notify_one();
    }

    std::optional<T> pop(bool blocking = false) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (blocking) {
            cond_var_.wait(lock, [this]{ return !queue_.empty(); });
        } else if (queue_.empty()) {
            return std::nullopt;
        }

        T message = queue_.front();
        queue_.pop();
        return message;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    std::queue<T> queue_;
};

} // namespace APFramework
