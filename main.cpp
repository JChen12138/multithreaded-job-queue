#include <iostream>
#include "ThreadPool.hpp"
#include <mutex>
#include <spdlog/spdlog.h>
#include "formatters/thread_id_formatter.hpp"
#include <cxxopts.hpp>

std::mutex cout_mutex;

int main(int argc, char* argv[]) {
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [thread %t] [%l] %v");
    spdlog::set_level(spdlog::level::info);

    // --- Parse command-line arguments ---
    cxxopts::Options options("server", "Multithreaded Job Queue Server");
    options.add_options()
        ("t,threads", "Number of threads", cxxopts::value<int>()->default_value("4"))
        ("q,max_queue", "Max queue size", cxxopts::value<int>()->default_value("100"))
        ("test_retry", "Enable test retry jobs")
        ("timeout", "Shutdown timeout in seconds", cxxopts::value<int>()->default_value("5"))
        ("h,help", "Print usage");

    auto options_result = options.parse(argc, argv);

    if (options_result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }
    
    int num_threads = options_result["threads"].as<int>();
    if (num_threads <= 0) {
        spdlog::error("Thread count must be positive.");
        return 1;
    }
    spdlog::info("Using {} worker threads", num_threads);

    int max_queue = options_result["max_queue"].as<int>();
    ThreadPool pool(num_threads, max_queue);

    int timeout = options_result["timeout"].as<int>();

    bool test_retry = options_result["test_retry"].as<bool>();

    if (test_retry) {
        spdlog::info("Submitting test jobs with retry logic");
        for (int i = 0; i < 10; ++i) {
            JobMetadata metadata(i, "RetryJob_" + std::to_string(i), 2); // Retry up to 2 times

            pool.submit(metadata, [metadata]() {
                spdlog::info("Executing job: {} (ID: {})", metadata.name, metadata.id);
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); 

                // Simulate failure for every 3rd job
                if (metadata.id % 3 == 0) {
                    throw std::runtime_error("Simulated failure for retry test");
                }
            });
        }
    } 
    else {
        spdlog::info("Submitting normal jobs");
        for (int i = 0; i < 10; ++i) {
            JobMetadata metadata(i, "Job_" + std::to_string(i));

            auto start = std::chrono::steady_clock::now();
            pool.submit(metadata, [metadata]() {
                spdlog::info("Executing job: {} (ID: {})", metadata.name, metadata.id);
                std::this_thread::sleep_for(std::chrono::milliseconds(300)); 
            });
            auto end = std::chrono::steady_clock::now();

            std::chrono::duration<double> duration = end - start;
            spdlog::info("Job submission {} took {:.3f} seconds", i, duration.count());
        }
    }
    

    // --- Job returning a result ---
    auto future = pool.submit({42, "ComputeAnswer"}, [] {
        spdlog::info("Computing result...");
        //std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::this_thread::sleep_for(std::chrono::seconds(2)); 
        return 42;
    });

    // call shutdown BEFORE result is ready
    /*
    spdlog::info("Initiating shutdown while job is still running...");
    pool.shutdown(timeout);  
    */

    spdlog::info("Waiting for result...");
    int result_value = future.get();
    spdlog::info("Result received: {}", result_value);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pool.shutdown(timeout);

    return 0;
}
