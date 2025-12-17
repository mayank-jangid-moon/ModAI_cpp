#pragma once

#include <chrono>
#include <mutex>
#include <queue>

namespace ModAI {

class RateLimiter {
private:
    int maxRequests_;
    std::chrono::milliseconds timeWindow_;
    std::queue<std::chrono::steady_clock::time_point> requestTimes_;
    std::mutex mutex_;

public:
    RateLimiter(int maxRequests, std::chrono::milliseconds timeWindow);
    
    bool acquire();
    void waitIfNeeded();
};

} // namespace ModAI
