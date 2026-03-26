#pragma once

#include <vector>
#include <thread>
#include <atomic>//for thread-safe flag operations.
#include "JobQueue.hpp"
#include <sstream>
#include <future>
#include <stdexcept>
#include <type_traits>
#include <spdlog/spdlog.h>
#ifdef DISABLE_METRICS
#include "MetricsStub.hpp"
#else
#include "Metrics.hpp"
#endif


class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads, size_t max_queue_size = 100);
    ~ThreadPool();//called automatically when the ThreadPool object goes out of scope or is deleted.
    void submit(JobMetadata&& metadata, std::function<void()> task);
    template<typename Func>
    auto submit(JobMetadata&& metadata, Func&& func) -> std::future<decltype(func())> {
        if (!running_) {
            throw std::runtime_error("Cannot submit job: ThreadPool is shut down");
        }
        using ResultType = decltype(func());

        //metadata.allow_retry = false;

        auto promise = std::make_shared<std::promise<ResultType>>();
        auto future = promise->get_future();
        auto retries_left = std::make_shared<std::atomic<int>>(metadata.allow_retry ? metadata.max_retries : 0);
        //Caller gets a future. ThreadPool keeps the promise.

        auto failure_handler = [promise](std::exception_ptr ex) {
            promise->set_exception(std::move(ex));
        };

        std::function<void()> wrapper = [promise, retries_left, f = std::forward<Func>(func)]() mutable {
            try {
                if constexpr (std::is_void_v<ResultType>) {
                    f();
                    promise->set_value();
                } else {
                    auto result = f();
                    promise->set_value(result);//sets the result.
                }
            } catch (const std::future_error& e) {
                spdlog::warn("Promise already fulfilled or invalid: {}", e.what());
            } catch (...) {
                if (retries_left->load() > 0) {
                    retries_left->fetch_sub(1);//atomic decrement operation
                }
                throw;
            }
        };

        //DIRECTLY enqueue the job instead of calling another submit()
        jobs_in_progress_++;
        if (!job_queue_.push(JobQueue::Job(std::move(metadata), std::move(wrapper), std::move(failure_handler)))) {
            jobs_in_progress_--;
            throw std::runtime_error("Cannot submit job: queue rejected enqueue during shutdown");
        }
        Metrics::instance().job_submitted().Increment();
        Metrics::instance().active_jobs().Increment();

        return future;
    }


    void shutdown(int timeout_seconds = 5);

private:
    void worker_loop(); // Worker thread function
    void complete_terminal_failure(JobQueue::Job& job, std::exception_ptr ex);
    void notify_job_finished();

    std::vector<std::thread> workers_;
    JobQueue job_queue_;
    std::atomic<bool> running_;
    std::atomic<int> jobs_in_progress_{0};
    std::condition_variable cv_done_;
    std::mutex done_mutex_;

};
