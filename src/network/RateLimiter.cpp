#include "network/RateLimiter.h"
#include <thread>
#include <chrono>

namespace ModAI {

RateLimiter::RateLimiter(int maxRequests, std::chrono::milliseconds timeWindow)
    : maxRequests_(maxRequests)
    , timeWindow_(timeWindow) {
}

bool RateLimiter::acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    // Remove old requests outside the time window
    while (!requestTimes_.empty() && 
           (now - requestTimes_.front()) > timeWindow_) {
        requestTimes_.pop();
    }
    
    if (requestTimes_.size() < static_cast<size_t>(maxRequests_)) {
        requestTimes_.push(now);
        return true;
    }
    
    return false;
}

void RateLimiter::waitIfNeeded() {
    while (!acquire()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace ModAI

