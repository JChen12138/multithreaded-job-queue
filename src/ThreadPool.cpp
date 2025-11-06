#include "ThreadPool.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include "formatters/thread_id_formatter.hpp"
#include "Metrics.hpp"



ThreadPool::ThreadPool(size_t num_threads, size_t max_queue_size) : job_queue_(max_queue_size), running_(true) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::worker_loop, this);
        //constructs a new std::thread in place and adds it to the vector.
    }
}

ThreadPool::~ThreadPool() {
    if (running_) {
        shutdown();  
    }
}

void ThreadPool::submit(JobMetadata&& metadata, std::function<void()> task) {
    spdlog::info("Job submitted: ID = {}, Name = {}", metadata.id, metadata.name);
    jobs_in_progress_++;
    job_queue_.push(JobQueue::Job(std::move(metadata), std::move(task)));
    Metrics::instance().job_submitted().Increment();
    Metrics::instance().active_jobs().Increment();
}

void ThreadPool::shutdown(int timeout_seconds) {
    spdlog::info("Shutdown started...");
    spdlog::info("Waiting for {} jobs to finish", jobs_in_progress_.load());
    std::unique_lock<std::mutex> lock(done_mutex_);
    bool finished = cv_done_.wait_for(lock, std::chrono::seconds(timeout_seconds), [this]() {
        return jobs_in_progress_ == 0;
    });
    if (!finished) {
        spdlog::warn("Graceful shutdown timeout reached. Proceeding with forced shutdown.");
    } 
    else {
        spdlog::info("All jobs completed. Proceeding with shutdown.");
    }

    running_ = false;
    job_queue_.shutdown();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    spdlog::info("Shutdown complete.");
    spdlog::info("Active jobs:    {}", Metrics::instance().active_jobs().Value());


}


void ThreadPool::worker_loop() {
    while (running_ || !job_queue_.empty()) {
        JobQueue::Job job;
        if (!job_queue_.try_pop(job)) {
            std::this_thread::yield();
            continue;
        }

        spdlog::info("Running job ID = {}, Name = {}, on thread {}",
                     job.metadata.id, job.metadata.name, std::this_thread::get_id());

        if (job.metadata.cancel_requested) {
            spdlog::warn("Job {} (ID: {}) cancelled before execution",
                         job.metadata.name, job.metadata.id);
            Metrics::instance().job_failed().Increment();
            Metrics::instance().active_jobs().Decrement();
            jobs_in_progress_--;
            continue;
        }

        bool timed_out = false;

        try {
            auto start = std::chrono::steady_clock::now(); 
            // --- With timeout ---
            if (job.metadata.timeout.count() > 0) {
                auto safe_task = std::make_shared<std::function<void()>>(std::move(job.task));
                std::packaged_task<void()> wrapper_task([safe_task]() {
                    try {
                        (*safe_task)();
                    } catch (...) {
                        spdlog::warn("Unhandled error inside job task");
                    }
                });
                auto future = wrapper_task.get_future();
                std::thread worker(std::move(wrapper_task));

                // Wait for the task to finish or timeout
                if (future.wait_for(job.metadata.timeout) == std::future_status::timeout) {
                    job.metadata.cancel_requested = true;
                    timed_out = true;
                    spdlog::warn("Job {} (ID: {}) timed out after {}ms",
                                job.metadata.name, job.metadata.id,
                                job.metadata.timeout.count());
                    try {
                        worker.detach();
                    } catch (...) {
                        spdlog::warn("Failed to detach timed‑out job thread safely");
                    }
                    Metrics::instance().job_failed().Increment();
                } else {
                    worker.join();
                    Metrics::instance().job_completed().Increment();
                    auto end = std::chrono::steady_clock::now();
                    std::chrono::duration<double> latency = end - start;
                if (!timed_out && !job.metadata.cancel_requested)
                    Metrics::instance().job_latency().Observe(latency.count());                }
            }
            // --- No timeout ---
            else {
                job.task();
                Metrics::instance().job_completed().Increment();
                auto end = std::chrono::steady_clock::now();
                std::chrono::duration<double> latency = end - start;
                if (!timed_out && !job.metadata.cancel_requested)
                    Metrics::instance().job_latency().Observe(latency.count());
            }

            Metrics::instance().active_jobs().Decrement();
            
            
        }

        // --- Unified exception guard ---
        catch (const std::future_error& e) {
            spdlog::warn("Future error in job {} (ID {}): {}", job.metadata.name, job.metadata.id, e.what());
        } catch (const std::exception& ex) {
            spdlog::error("Job {} (ID: {}) failed: {}", job.metadata.name, job.metadata.id, ex.what());
            if (!job.metadata.cancel_requested && job.metadata.allow_retry && job.metadata.current_retry < job.metadata.max_retries) {
                    spdlog::warn("Retrying job {} (ID: {}) [attempt {}/{}]",job.metadata.name, job.metadata.id,
                    job.metadata.current_retry + 1, job.metadata.max_retries);
                job.metadata.current_retry++;
                job_queue_.push(std::move(job));
            } else if (job.metadata.allow_retry) {
                spdlog::info("Job {} (ID: {}) not retried: timed_out={}, current_retry={}, max_retries={}",
                    job.metadata.name, job.metadata.id, timed_out, job.metadata.current_retry, job.metadata.max_retries);
            }
        } catch (...) {
            spdlog::error("Job {} (ID: {}) failed with unknown error", job.metadata.name, job.metadata.id);
            Metrics::instance().job_failed().Increment();
            Metrics::instance().active_jobs().Decrement();
        }

        jobs_in_progress_--;
        if (jobs_in_progress_ == 0) {
            std::unique_lock lock(done_mutex_);
            cv_done_.notify_all();
        }
    }
}



