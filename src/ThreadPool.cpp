#include "ThreadPool.hpp"
#include <exception>
#include <spdlog/spdlog.h>
#include <thread>
#include "formatters/thread_id_formatter.hpp"

namespace {
std::exception_ptr make_runtime_exception_ptr(const char* message) {
    return std::make_exception_ptr(std::runtime_error(message));
}
}

ThreadPool::ThreadPool(size_t num_threads, size_t max_queue_size): job_queue_(max_queue_size), running_(true) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::worker_loop, this);
        // constructs a new std::thread in place and adds it to the vector.
    }
}

ThreadPool::~ThreadPool() {
    if (running_) {
        shutdown();
    }
}

void ThreadPool::submit(JobMetadata&& metadata, std::function<void()> task) {
    if (!running_) {
        throw std::runtime_error("Cannot submit job: ThreadPool is shut down");
    }
    spdlog::info("Job submitted: ID = {}, Name = {}", metadata.id, metadata.name);
    jobs_in_progress_++;
    if (!job_queue_.push(JobQueue::Job(std::move(metadata), std::move(task)))) {
        jobs_in_progress_--;
        throw std::runtime_error("Cannot submit job: queue rejected enqueue during shutdown");
    }
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
    } else {
        spdlog::info("All jobs completed. Proceeding with shutdown.");
    }

    running_ = false;
    job_queue_.shutdown();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();//block here until that thread finishes
        }
    }
    spdlog::info("Shutdown complete.");
    spdlog::info("Active jobs:    {}", Metrics::instance().active_jobs().Value());
}

void ThreadPool::worker_loop() {
    while (true) {
        JobQueue::Job job = job_queue_.pop();
        if (job_queue_.is_shutdown() && job.metadata.id == -1) {
            break;
        }

        // Previous non-blocking worker path kept for reference:
        // while (running_ || !job_queue_.empty()) {
        //     JobQueue::Job job;
        //     if (!job_queue_.try_pop(job)) {
        //         std::this_thread::yield();
        //         continue;
        //     }        

        if (job.metadata.cancel_requested) {
            spdlog::warn("Job {} (ID: {}) cancelled before execution",
                         job.metadata.name, job.metadata.id);
            complete_terminal_failure(job, make_runtime_exception_ptr("Job cancelled before execution"));
            Metrics::instance().job_failed().Increment();
            Metrics::instance().active_jobs().Decrement();
            notify_job_finished();
            continue;
        }

        bool timed_out = false;
        bool retried = false;

        try {
            auto start = std::chrono::steady_clock::now();
            if (job.metadata.timeout.count() > 0) {
                const auto deadline = job.metadata.enqueue_time + job.metadata.timeout;
                if (start >= deadline) {
                    job.metadata.cancel_requested = true;
                    timed_out = true;
                    spdlog::warn("Job {} (ID: {}) expired before execution after waiting {}ms",
                                 job.metadata.name, job.metadata.id, job.metadata.timeout.count());
                    complete_terminal_failure(job, make_runtime_exception_ptr("Job expired before execution"));
                    Metrics::instance().job_failed().Increment();
                }
            }

            if (!timed_out) {
                spdlog::info("Running job ID = {}, Name = {}, on thread {}",
                             job.metadata.id, job.metadata.name, std::this_thread::get_id());
                job.task();
                Metrics::instance().job_completed().Increment();
                auto end = std::chrono::steady_clock::now();
                std::chrono::duration<double> latency = end - start;
                if (!job.metadata.cancel_requested) {
                    Metrics::instance().job_latency().Observe(latency.count());
                }
            }

            Metrics::instance().active_jobs().Decrement();
        } catch (const std::future_error& e) {
            spdlog::warn("Future error in job {} (ID {}): {}", job.metadata.name, job.metadata.id, e.what());
            Metrics::instance().job_failed().Increment();
            Metrics::instance().active_jobs().Decrement();
        } catch (const std::exception& ex) {
            spdlog::error("Job {} (ID: {}) failed: {}", job.metadata.name, job.metadata.id, ex.what());
            spdlog::info("Retry check: allow_retry={}, cancel={}, cur={}, max={}",
                         job.metadata.allow_retry,
                         job.metadata.cancel_requested.load(),
                         job.metadata.current_retry,
                         job.metadata.max_retries);

            if (!job.metadata.cancel_requested &&job.metadata.allow_retry &&
                job.metadata.current_retry < job.metadata.max_retries) {
                spdlog::warn("Retrying job {} (ID: {}) [attempt {}/{}]",job.metadata.name, job.metadata.id,
                             job.metadata.current_retry + 1, job.metadata.max_retries);
                job.metadata.current_retry++;
                if (job_queue_.push(std::move(job))) {
                    retried = true;
                } else {
                    spdlog::warn("Retry requeue rejected for job {} (ID: {}) during shutdown",
                                 job.metadata.name, job.metadata.id);
                    complete_terminal_failure(job, make_runtime_exception_ptr("Retry requeue rejected during shutdown"));
                }
            } else {
                if (job.metadata.allow_retry) {
                    spdlog::info("Job {} (ID: {}) not retried: timed_out={}, current_retry={}, max_retries={}",
                                 job.metadata.name, job.metadata.id, timed_out,
                                 job.metadata.current_retry, job.metadata.max_retries);
                }
                complete_terminal_failure(job, std::current_exception());
            }

            if (!retried) {
                Metrics::instance().job_failed().Increment();
                Metrics::instance().active_jobs().Decrement();
            }
        } catch (...) {
            spdlog::error("Job {} (ID: {}) failed with unknown error", job.metadata.name, job.metadata.id);
            complete_terminal_failure(job, make_runtime_exception_ptr("Job failed with unknown error"));
            Metrics::instance().job_failed().Increment();
            Metrics::instance().active_jobs().Decrement();
        }

        if (!retried) {
            notify_job_finished();
        }
    }
}

void ThreadPool::complete_terminal_failure(JobQueue::Job& job, std::exception_ptr ex) {
    if (!job.on_terminal_failure) {
        return;
    }

    try {
        job.on_terminal_failure(std::move(ex));
    } catch (const std::future_error& e) {
        spdlog::warn("Failed to complete promise for job {} (ID: {}): {}",
                     job.metadata.name, job.metadata.id, e.what());
    }
}

void ThreadPool::notify_job_finished() {
    jobs_in_progress_--;
    if (jobs_in_progress_ == 0) {
        std::unique_lock lock(done_mutex_);
        cv_done_.notify_all();
    }
}
