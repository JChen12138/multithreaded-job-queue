#pragma once

#include <queue>
#include <mutex>
#include <condition_variable> //let threads wait for jobs to become available (or for shutdown)
#include <functional>

class JobQueue {
public:
    using Job = std::function<void()>;

    void push(Job job); 
    Job pop();  // Blocks until job is available
    bool try_pop(Job& job); // retrieve a job without waiting. 
    bool empty();
    void shutdown();
    bool is_shutdown();

private:
    std::queue<Job> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool shutdown_ = false;
};
