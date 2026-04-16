#pragma once

#include <vector>
#include <algorithm>
#include <mutex>
#include <condition_variable> //let threads wait for jobs to become available (or for shutdown)
#include <exception>
#include <functional>
#include "JobMetadata.hpp"
class JobQueue {
public:
    struct Job {
        JobMetadata metadata;
        std::function<void()> task;
        std::function<void(std::exception_ptr)> on_terminal_failure;

        Job() = default;

        Job(JobMetadata meta,
            std::function<void()> t,
            std::function<void(std::exception_ptr)> failure_handler = {})
            : metadata(std::move(meta)),
              task(std::move(t)),
              on_terminal_failure(std::move(failure_handler)) {}
    };

    explicit JobQueue(size_t max_size = 100); 

    bool push(Job job); 
    Job pop();  // Blocks until job is available
    bool try_pop(Job& job); // retrieve a job without waiting. 
    bool empty();
    void shutdown();
    bool is_shutdown();

private:
    struct JobCompare {
        bool operator()(const Job& lhs, const Job& rhs) const {
            return lhs.metadata.priority > rhs.metadata.priority;
        }
    };

    std::mutex mutex_;
    std::condition_variable not_empty_cv_;// consumer wait
    std::condition_variable not_full_cv_;   // producer wait when full
    bool shutdown_ = false;
    size_t max_queue_size_;       
    std::vector<Job> queue_;
    JobCompare compare_;
};
