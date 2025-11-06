#include "JobQueue.hpp"
#include <spdlog/spdlog.h>

JobQueue::JobQueue(size_t max_size)
    : max_queue_size_(max_size) {}

void JobQueue::push(Job job) {
    std::unique_lock<std::mutex> lock(mutex_);
    not_full_cv_.wait(lock, [this]() {
        return queue_.size() < max_queue_size_ || shutdown_;
    });

    if (shutdown_) return;
    queue_.push(std::move(job));
    spdlog::info("Queue size after push: {}", queue_.size());
    not_empty_cv_.notify_one();//Wakes up one thread waiting on cv_
}

JobQueue::Job JobQueue::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_cv_.wait(lock, [this]() { return !queue_.empty() || shutdown_; });

    if (shutdown_ && queue_.empty()) {
        return Job(JobMetadata(-1, "empty"), []() {});
    }

    //Job job = std::move(queue_.front());
    Job job = std::move(const_cast<Job&>(queue_.top()));
    queue_.pop();
    not_full_cv_.notify_one();  // signal producer
    return job;
}

bool JobQueue::try_pop(Job& job) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return false;

    //job = std::move(queue_.front());
     job = std::move(const_cast<Job&>(queue_.top()));  
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
    not_empty_cv_.notify_all();
}

bool JobQueue::is_shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    return shutdown_;
}
