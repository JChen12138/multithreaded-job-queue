#include <iostream>
#include "ThreadPool.hpp"
#include <mutex>
#include <spdlog/spdlog.h>
#include "formatters/thread_id_formatter.hpp"
#include <cxxopts.hpp>
#include "Metrics.hpp"
#include "LRUCache.hpp"
LRUCache<std::string, int> result_cache(100); // adjust capacity as needed


std::mutex cout_mutex;

int main(int argc, char* argv[]) {\
    spdlog::info("Initializing Prometheus metrics server...");
    Metrics::instance().init("/metrics");
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [thread %t] [%l] %v");
    spdlog::set_level(spdlog::level::info);

    // --- Parse command-line arguments ---
    cxxopts::Options options("server", "Multithreaded Job Queue Server");
    options.add_options()
        ("t,threads", "Number of threads", cxxopts::value<int>()->default_value("4"))
        ("q,max_queue", "Max queue size", cxxopts::value<int>()->default_value("100"))
        ("test_retry", "Enable test retry jobs")
        ("timeout", "Shutdown timeout in seconds", cxxopts::value<int>()->default_value("5"))
        ("job_timeout", "Per-job timeout in milliseconds", cxxopts::value<int>()->default_value("0")) 
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

        // --- Test LRU Cache ---
    spdlog::info("Testing LRUCache with capacity 3");
    LRUCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    std::string value;
    if (cache.get(2, value)) {
        spdlog::info("LRUCache: Key 2 found -> {}", value);
    }

    cache.put(4, "four"); // Evicts key 1

    if (!cache.get(1, value)) {
        spdlog::info("LRUCache: Key 1 was evicted");
    }

    if (cache.get(3, value)) {
        spdlog::info("LRUCache: Key 3 found -> {}", value);
    }

    cache.put(5, "five"); // Evicts key 2

    if (!cache.get(2, value)) {
        spdlog::info("LRUCache: Key 2 was evicted");
    }


    int timeout = options_result["timeout"].as<int>();

    bool test_retry = options_result["test_retry"].as<bool>();

    int job_timeout_ms = options_result["job_timeout"].as<int>();

    
    if (test_retry) {
        spdlog::info("Submitting test jobs with retry logic");
        for (int i = 0; i < 10; ++i) {
            JobMetadata metadata(i, "RetryJob_" + std::to_string(i), 2); // Retry up to 2 times
            std::string name_copy = metadata.name;
            int id_copy = metadata.id;
            pool.submit(std::move(metadata), [name = std::move(name_copy), id = id_copy]() {
                spdlog::info("Executing job: {} (ID: {})", name, id);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (id % 3 == 0) {
                    throw std::runtime_error("Simulated failure for retry test");
                }
            });
        }
    } 
    else {
        spdlog::info("Submitting normal jobs");
        for (int i = 0; i < 10; ++i) {
            JobMetadata metadata(i, "Job_" + std::to_string(i));
            if (job_timeout_ms > 0) {
                metadata.timeout = std::chrono::milliseconds(job_timeout_ms); 
            }
            auto start = std::chrono::steady_clock::now();
            std::string name_copy = metadata.name;
            int id_copy = metadata.id;
            pool.submit(std::move(metadata), [name = std::move(name_copy), id = id_copy]() {
                spdlog::info("Executing job: {} (ID: {})", name, id);
                std::this_thread::sleep_for(std::chrono::milliseconds(300)); 
            });

            auto end = std::chrono::steady_clock::now();

            std::chrono::duration<double> duration = end - start;
            spdlog::info("Job submission {} took {:.3f} seconds", i, duration.count());
        }
    }

    // --- Job returning a result ---
    JobMetadata metadata{42, "ComputeAnswer"};
    metadata.allow_retry = false;
    metadata.timeout = std::chrono::milliseconds(1000);  // 1 second

    std::string cache_key = metadata.name + std::to_string(metadata.id); // Unique cache key
    int cached_value;
    if (result_cache.get(cache_key, cached_value)) {
        spdlog::info("Cache hit for job: {}", metadata.name);
        spdlog::info("Cached result: {}", cached_value);
    } else {
        spdlog::info("Cache miss for job: {}", metadata.name);

        std::string name_copy = metadata.name;
        int id_copy = metadata.id;

        auto future = pool.submit(std::move(metadata), [name = std::move(name_copy), id = id_copy, cache_key]() {
            spdlog::info("Executing job: {} (ID: {})", name, id);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            int result = 42;

            result_cache.put(cache_key, result);  // Store result in cache
            return result;
        });

        spdlog::info("Waiting for result...");
        int result_value = future.get();
        spdlog::info("Result received: {}", result_value);
    }



    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Test priority behavior (overwrite job IDs so they're unique)
    pool.submit(JobMetadata(100, "prio_5", 5), [] {
        spdlog::info("Running prio_5 job");
    });
    pool.submit(JobMetadata(101, "prio_1", 1), [] {
        spdlog::info("Running prio_1 job");
    });
    pool.submit(JobMetadata(102, "prio_9", 9), [] {
        spdlog::info("Running prio_9 job");
    });

    pool.shutdown(timeout);

    return 0;
}
