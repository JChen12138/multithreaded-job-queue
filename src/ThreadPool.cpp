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

void ThreadPool::submit(JobMetadata metadata, std::function<void()> task) {
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

        spdlog::info("Running job ID = {}, Name = {}, on thread {}", job.metadata.id, job.metadata.name, std::this_thread::get_id());


        try {
            job.task(); // Attempt to run the job
            Metrics::instance().job_completed().Increment();
            Metrics::instance().active_jobs().Decrement();
        } catch (const std::exception& ex) {
            spdlog::error("Job {} (ID: {}) failed with exception: {}", job.metadata.name, job.metadata.id, ex.what());
            if (job.metadata.current_retry < job.metadata.max_retries) {
                job.metadata.current_retry++;
                spdlog::warn("Retrying job {} (ID: {}) [attempt {}/{}]",
                             job.metadata.name, job.metadata.id,
                             job.metadata.current_retry, job.metadata.max_retries);
                job_queue_.push(std::move(job));
            } else {
                spdlog::error("Job {} (ID: {}) failed permanently after {} retries",
                              job.metadata.name, job.metadata.id,
                              job.metadata.max_retries);
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


