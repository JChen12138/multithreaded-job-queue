#include "JobQueue.hpp"
#include <spdlog/spdlog.h>

JobQueue::JobQueue(size_t max_size)
    : max_queue_size_(max_size) {}

bool JobQueue::push(Job job) { // pushing A Job struct(contains: metadata, std::function<void()> task, NOT a thread, just a function object stored in memory.
    std::unique_lock<std::mutex> lock(mutex_);
    not_full_cv_.wait(lock, [this]() {
        return queue_.size() < max_queue_size_ || shutdown_;
    });

    if (shutdown_) return false;
    queue_.push_back(std::move(job));
    std::push_heap(queue_.begin(), queue_.end(), compare_);
    spdlog::info("Queue size after push: {}", queue_.size());
    not_empty_cv_.notify_one();//Wakes up one thread waiting on cv_
    return true;
}

JobQueue::Job JobQueue::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_cv_.wait(lock, [this]() { return !queue_.empty() || shutdown_; });

    if (shutdown_ && queue_.empty()) {
        return Job(JobMetadata(-1, "empty"), []() {});
    }

    std::pop_heap(queue_.begin(), queue_.end(), compare_);
    Job job = std::move(queue_.back());
    queue_.pop_back();
    not_full_cv_.notify_one();  // signal producer
    return job;
}

bool JobQueue::try_pop(Job& job) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return false;

    std::pop_heap(queue_.begin(), queue_.end(), compare_);
    job = std::move(queue_.back());
    queue_.pop_back();
    not_full_cv_.notify_one();
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
    not_full_cv_.notify_all();

}

bool JobQueue::is_shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    return shutdown_;
}
