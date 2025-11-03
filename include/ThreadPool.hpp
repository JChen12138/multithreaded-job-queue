#pragma once

#include <vector>
#include <thread>
#include <atomic>//for thread-safe flag operations.
#include "JobQueue.hpp"
#include <sstream>
#include <future>
#include <spdlog/spdlog.h>
#include "Metrics.hpp"



class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads, size_t max_queue_size = 100);
    ~ThreadPool();//called automatically when the ThreadPool object goes out of scope or is deleted.
    void submit(JobMetadata&& metadata, std::function<void()> task);
    template<typename Func>
    auto submit(JobMetadata&& metadata, Func&& func) -> std::future<decltype(func())> {
        using ResultType = decltype(func());

        metadata.allow_retry = false;

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

        //DIRECTLY enqueue the job instead of calling another submit()
        jobs_in_progress_++;
        job_queue_.push(JobQueue::Job(std::move(metadata), std::move(wrapper)));
        Metrics::instance().job_submitted().Increment();
        Metrics::instance().active_jobs().Increment();

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
