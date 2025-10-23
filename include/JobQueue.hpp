#pragma once

#include <queue>
#include <mutex>
#include <condition_variable> //let threads wait for jobs to become available (or for shutdown)
#include <functional>
#include "JobMetadata.hpp"


class JobQueue {
public:
    struct Job {
        JobMetadata metadata;
        std::function<void()> task;

        Job() = default;
        
        Job(JobMetadata meta, std::function<void()> t)
            : metadata(std::move(meta)), task(std::move(t)) {}
    };

    explicit JobQueue(size_t max_size = 100); 

    void push(Job job); 
    Job pop();  // Blocks until job is available
    bool try_pop(Job& job); // retrieve a job without waiting. 
    bool empty();
    void shutdown();
    bool is_shutdown();

private:
    std::queue<Job> queue_;
    std::mutex mutex_;
    std::condition_variable not_empty_cv_;// consumer wait
    std::condition_variable not_full_cv_;   // producer wait when full
    bool shutdown_ = false;
    size_t max_queue_size_;                 
};
