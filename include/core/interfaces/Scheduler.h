#pragma once

#include <functional>
#include <chrono>

namespace ModAI {

class Scheduler {
public:
    virtual ~Scheduler() = default;
    
    // Schedule a task to run repeatedly
    virtual void schedule(std::chrono::milliseconds interval, std::function<void()> task) = 0;
    
    // Stop the scheduler
    virtual void stop() = 0;
};

} // namespace ModAI
