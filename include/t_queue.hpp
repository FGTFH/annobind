#pragma once
#include <nanobind/nanobind.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

// constexpr auto [a,b]=[]{
//     return std::pair{1,2.};
// }();

template <typename T>
class Queue {
   private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool closed_ = false;

   public:
    Queue() = default;
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    ~Queue() { close(); }

    void push(T&& item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_) return;
            queue_.push(std::forward<T>(item));
        }
        cv_.notify_one();
    }
    [[= nanobind::call_guard<nanobind::gil_scoped_release>()]] std::optional<T> pop(int timeout_ms = -1) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (timeout_ms < 0) {
            cv_.wait(lock, [this] { return closed_ || !queue_.empty(); });
        } else {
            auto timeout = std::chrono::milliseconds(timeout_ms);
            bool success = cv_.wait_for(lock, timeout, [this] { return closed_ || !queue_.empty(); });
            if (!success) {
                return std::nullopt;
            }
        }

        if (closed_) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    bool empty() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void notify() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        cv_.notify_all();
    }
    bool closed() noexcept { return closed_; }
    void close() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        std::queue<T> empty;
        std::swap(queue_, empty);
        cv_.notify_all();
    }

    void open() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = false;
        cv_.notify_all();
    }

    void clear() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<T> empty;
        std::swap(queue_, empty);
    }
};