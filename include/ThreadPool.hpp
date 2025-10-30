#pragma once

#include <vector>
#include <thread>
#include <atomic>//for thread-safe flag operations.
#include "JobQueue.hpp"
#include <sstream>
#include <future>
#include <spdlog/spdlog.h>



class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads, size_t max_queue_size = 100);
    ~ThreadPool();//called automatically when the ThreadPool object goes out of scope or is deleted.
    void submit(JobMetadata&& metadata, std::function<void()> task);
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
    auto submit(JobMetadata&& metadata, Func&& func) -> std::future<decltype(func())> {
        using ResultType = decltype(func());

        auto promise = std::make_shared<std::promise<ResultType>>();
        auto future = promise->get_future();

        std::function<void()> wrapper = [promise, f = std::forward<Func>(func)]() mutable {
            try {
                if constexpr (std::is_void_v<ResultType>) {
                    f();
                    promise->set_value();
                } else {
                    auto result = f();
                    promise->set_value(result);
                }
            } catch (const std::future_error& e) {
                spdlog::warn("Promise already fulfilled or invalid: {}", e.what());
            } catch (...) {
                try {
                    promise->set_exception(std::current_exception());
                } catch (...) {
                    spdlog::warn("Failed to set exception on promise");
                }
            }
        };

        submit(std::move(metadata), std::move(wrapper));
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
