#pragma once

#include "core/interfaces/Scheduler.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace ModAI {

class StdScheduler : public Scheduler {
private:
    std::atomic<bool> running_{false};
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable cv_;

public:
    StdScheduler() = default;
    ~StdScheduler() { stop(); }

    void schedule(std::chrono::milliseconds interval, std::function<void()> task) override {
        stop(); // Ensure only one task running for this simple implementation
        running_ = true;
        worker_ = std::thread([this, interval, task]() {
            while (running_) {
                auto start = std::chrono::steady_clock::now();
                task();
                auto end = std::chrono::steady_clock::now();
                
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                auto sleepTime = interval - elapsed;
                
                if (sleepTime.count() > 0) {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait_for(lock, sleepTime, [this] { return !running_; });
                }
            }
        });
    }

    void stop() override {
        running_ = false;
        cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
    }
};

} // namespace ModAI
