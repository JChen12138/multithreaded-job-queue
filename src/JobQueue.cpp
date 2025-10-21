#include "JobQueue.hpp"

void JobQueue::push(Job job) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (shutdown_) return;
        queue_.push(std::move(job));
    }
    cv_.notify_one();//Wakes up one thread waiting on cv_
}

JobQueue::Job JobQueue::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]() { return !queue_.empty() || shutdown_; });

    if (shutdown_ && queue_.empty()) {
        return nullptr;
    }

    Job job = std::move(queue_.front());
    queue_.pop();
    return job;
}

bool JobQueue::try_pop(Job& job) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return false;

    job = std::move(queue_.front());
    queue_.pop();
    return true;
}

bool JobQueue::empty() {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

void JobQueue::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    cv_.notify_all();
}

bool JobQueue::is_shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    return shutdown_;
}
