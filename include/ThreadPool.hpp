#pragma once

#include <vector>
#include <thread>
#include <atomic>//for thread-safe flag operations.
#include "JobQueue.hpp"
#include <sstream>
#include <future>


class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads, size_t max_queue_size = 100);
    ~ThreadPool();//called automatically when the ThreadPool object goes out of scope or is deleted.
    void submit(JobMetadata metadata, std::function<void()> task);
    template<typename Func>
    auto submit(Func&& func) -> std::future<decltype(func())> {
    //the caller receives a std::future that will later hold the result of the submitted job.
        using ResultType = decltype(func());

        // wrap function into a packaged_task so we can get its result later
        auto task = std::make_shared<std::packaged_task<ResultType()>>(std::forward<Func>(func));
        std::future<ResultType> result = task->get_future();

        jobs_in_progress_++;  // track active jobs
        job_queue_.push(JobQueue::Job(JobMetadata(-1, "packaged_task"), [task]() {
            (*task)();  // Runs the packaged task, sets future
        }));

        return result;
    }

    template<typename Func>
    auto submit(JobMetadata metadata, Func&& func) -> std::future<decltype(func())> {
        using ResultType = decltype(func());

        auto task = std::make_shared<std::packaged_task<ResultType()>>(std::forward<Func>(func));
        auto future = task->get_future();

        // Wrap the lambda into std::function to call the right overload
        std::function<void()> wrapper = [task]() { (*task)(); };
        submit(std::move(metadata), std::move(wrapper));  // Call the non-template overload

        return future;
    }



    void shutdown(int timeout_seconds = 5);

private:
    void worker_loop(); // Worker thread function

    std::vector<std::thread> workers_;
    JobQueue job_queue_;
    std::atomic<bool> running_;
    std::atomic<int> jobs_in_progress_{0};
    std::condition_variable cv_done_;
    std::mutex done_mutex_;

};
