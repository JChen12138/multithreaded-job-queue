#include "ThreadPool.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include "formatters/thread_id_formatter.hpp"




ThreadPool::ThreadPool(size_t num_threads) : running_(true) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::worker_loop, this);
        //constructs a new std::thread in place and adds it to the vector.
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::submit(JobQueue::Job job) {
    spdlog::info("Job submitted.");
    jobs_in_progress_++;
    job_queue_.push(std::move(job));
}

void ThreadPool::shutdown() {
    {
        spdlog::info("Shutdown started...");
        spdlog::info("Waiting for {} jobs to finish", jobs_in_progress_.load());
        std::unique_lock<std::mutex> lock(done_mutex_);
        cv_done_.wait(lock, [this]() {
            return jobs_in_progress_ == 0;
        });
    }

    running_ = false;
    job_queue_.shutdown();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}


void ThreadPool::worker_loop() {
    spdlog::debug("Worker started: thread id {}", std::this_thread::get_id());
    while (running_) {
        JobQueue::Job job = job_queue_.pop();
        if (job) {
            job();  
            if (--jobs_in_progress_ == 0) {
                std::lock_guard<std::mutex> lock(done_mutex_);
                cv_done_.notify_all();
            }
        } else if (job_queue_.is_shutdown()) {
            break;
        }
    }
}

