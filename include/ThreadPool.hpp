#pragma once

#include <vector>
#include <thread>
#include <atomic>//for thread-safe flag operations.
#include "JobQueue.hpp"
#include <sstream>
#include <future>


class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();//called automatically when the ThreadPool object goes out of scope or is deleted.

    void submit(JobQueue::Job job);
    template<typename Func>
    auto submit(Func&& func) -> std::future<decltype(func())> {
    //the caller receives a std::future that will later hold the result of the submitted job.
        using ResultType = decltype(func());

        // wrap function into a packaged_task so we can get its result later
        auto task = std::make_shared<std::packaged_task<ResultType()>>(std::forward<Func>(func));
        std::future<ResultType> result = task->get_future();

        jobs_in_progress_++;  // track active jobs
        job_queue_.push([task]() {
            (*task)();//runs the packaged task, which calls func() and sets the value for the future.
        });

        return result;
    }

    void shutdown();

private:
    void worker_loop(); // Worker thread function

    std::vector<std::thread> workers_;
    JobQueue job_queue_;
    std::atomic<bool> running_;
    std::atomic<int> jobs_in_progress_{0};
    std::condition_variable cv_done_;
    std::mutex done_mutex_;

};
