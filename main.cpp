#include <iostream>
#include "ThreadPool.hpp"
#include <mutex>
#include <spdlog/spdlog.h>
#include "formatters/thread_id_formatter.hpp"



std::mutex cout_mutex;

int main() {
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [thread %t] [%l] %v");
    spdlog::set_level(spdlog::level::info);
    ThreadPool pool(4);

    //normal jobs
    for (int i = 0; i < 10; ++i) {
        pool.submit([i]() {
            //std::lock_guard<std::mutex> lock(cout_mutex);
            //spdlog handles its own locks.
            spdlog::info("Running job {} on thread {}", i, std::this_thread::get_id());
        });
    }
    //job returning a result
    auto future = pool.submit([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        spdlog::info("Computing result...");
        return 42;
    });

    spdlog::info("Waiting for result...");
    int result = future.get();
    spdlog::info("Result received: {}", result);
    pool.shutdown();

    return 0;
}
