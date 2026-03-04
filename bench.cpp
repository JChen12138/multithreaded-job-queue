// bench.cpp
// Simple stress / benchmarking driver.
//
// Default workload: CPU-bound loop
// - Good for demonstrating scaling, queue contention, and scheduling overhead.
// Optional: sleep-based workload (--mode sleep) to simulate IO-bound tasks.

#include "ThreadPool.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <spdlog/spdlog.h>

using Clock = std::chrono::steady_clock;//monotonic

struct Args {
    size_t threads = std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4;
    size_t queue = 10000;
    size_t jobs = 200000;

    // Workload selection
    std::string mode = "cpu"; // cpu or sleep

    // CPU work
    uint64_t iters = 5000; // iterations per job

    // Sleep work
    int sleep_us = 200;
    int min_sleep_us = 1000;

    int shutdown_timeout_s = 120;
};

static void print_usage(const char* exe) {
    std::cout
        << "Usage: " << exe << " [options]\n\n"
        << "Options:\n"
        << "  --threads N         Worker threads (default: hw_concurrency)\n"
        << "  --queue N           Max queue size (default: 10000)\n"
        << "  --jobs N            Total jobs to submit (default: 200000)\n"
        << "  --mode cpu|sleep    Workload type (default: cpu)\n"
        << "  --iters N           CPU iterations per job (default: 5000)\n"
        << "  --sleep_us N        Sleep microseconds per job when mode=sleep (default: 200)\n"
        << "  --min_sleep_us N    Warn if sleep_us is below this threshold (default: 1000)\n"
        << "  --shutdown_s N      Shutdown wait timeout seconds (default: 120)\n"
        << "  --help              Show this message\n";
}

static bool parse_u64(const char* s, uint64_t& out) {
    if (!s || !*s) return false;//s pointer exists AND string not empty
    char* end = nullptr;
    errno = 0;
    unsigned long long v = std::strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') return false;
    out = static_cast<uint64_t>(v);
    return true;
}

static bool parse_size(const char* s, size_t& out) {
    uint64_t v = 0;
    if (!parse_u64(s, v)) return false;
    out = static_cast<size_t>(v);
    return true;
}

static bool parse_i32(const char* s, int& out) {
    if (!s || !*s) return false;
    char* end = nullptr;
    errno = 0;
    long v = std::strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

static Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        auto need_value = [&](const char* opt) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << opt << "\n";
                std::exit(2);
            }
            return argv[++i];
        };

        if (key == "--help" || key == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (key == "--threads") {
            parse_size(need_value("--threads"), a.threads);
        } else if (key == "--queue") {
            parse_size(need_value("--queue"), a.queue);
        } else if (key == "--jobs") {
            parse_size(need_value("--jobs"), a.jobs);
        } else if (key == "--mode") {
            a.mode = need_value("--mode");
        } else if (key == "--iters") {
            uint64_t v = 0;
            if (!parse_u64(need_value("--iters"), v)) {
                std::cerr << "Invalid --iters\n";
                std::exit(2);
            }
            a.iters = v;
        } else if (key == "--sleep_us") {
            if (!parse_i32(need_value("--sleep_us"), a.sleep_us)) {
                std::cerr << "Invalid --sleep_us\n";
                std::exit(2);
            }
        } else if (key == "--shutdown_s") {
            if (!parse_i32(need_value("--shutdown_s"), a.shutdown_timeout_s)) {
                std::cerr << "Invalid --shutdown_s\n";
                std::exit(2);
            }
        } else if (key == "--min_sleep_us") {
            if (!parse_i32(need_value("--min_sleep_us"), a.min_sleep_us)) {
                std::cerr << "Invalid --min_sleep_us\n";
                std::exit(2);
            }
        } else {
            std::cerr << "Unknown option: " << key << "\n";
            print_usage(argv[0]);
            std::exit(2);
        }
    }
    if (a.threads == 0) a.threads = 1;
    if (a.queue == 0) a.queue = 1;
    if (a.jobs == 0) a.jobs = 1;
    if (a.mode != "cpu" && a.mode != "sleep") {
        std::cerr << "Invalid --mode. Use cpu or sleep.\n";
        std::exit(2);
    }
    return a;
}

// Prevent compiler from optimizing away the loop.
static inline uint64_t cpu_work(uint64_t iters) {
    volatile uint64_t x = 0x123456789abcdefULL;//volatile prevents compiler from optimizing away the loop.
    for (uint64_t i = 0; i < iters; ++i) {
        x = x * 1664525ULL + 1013904223ULL; // LCG step
        x ^= (x >> 13);
    }
    return static_cast<uint64_t>(x);
}

int main(int argc, char** argv) {
    // Reduce logging overhead
    spdlog::set_level(spdlog::level::warn);

    const Args args = parse_args(argc, argv);

    std::cout << "=== ThreadPool Benchmark ===\n";
    std::cout << "threads=" << args.threads
              << " queue=" << args.queue
              << " jobs=" << args.jobs
              << " mode=" << args.mode
              << (args.mode == "cpu" ? (" iters=" + std::to_string(args.iters))
                                      : (" sleep_us=" + std::to_string(args.sleep_us)))
              << "\n\n";

    if (args.mode == "sleep" && args.sleep_us > 0 && args.sleep_us < args.min_sleep_us) {
        std::cout << "Note: sleep_us=" << args.sleep_us
                  << " is below the recommended benchmark threshold of "
                  << args.min_sleep_us
                  << "us. Short sleeps can be scheduler/timer limited and are not reliable for throughput claims.\n\n";
    }

    ThreadPool pool(args.threads, args.queue);

    std::atomic<size_t> completed{0};
    std::atomic<uint64_t> checksum{0};

    // Submit phase timing
    const auto t_submit_start = Clock::now();

    if (args.mode == "cpu") {
        for (size_t i = 0; i < args.jobs; ++i) {
            JobMetadata meta(static_cast<int>(i), "bench_cpu");
            meta.allow_retry = false;
            pool.submit(std::move(meta), [&completed, &checksum, iters = args.iters]() {
                uint64_t v = cpu_work(iters);
                checksum.fetch_add(v, std::memory_order_relaxed);//don't care about ordering
                completed.fetch_add(1, std::memory_order_release);
            });
        }
    } else { // sleep
        for (size_t i = 0; i < args.jobs; ++i) {
            JobMetadata meta(static_cast<int>(i), "bench_sleep");
            meta.allow_retry = false;
            pool.submit(std::move(meta), [&completed, us = args.sleep_us]() {
                std::this_thread::sleep_for(std::chrono::microseconds(us));
                completed.fetch_add(1, std::memory_order_release);
            });
        }
    }

    const auto t_submit_end = Clock::now();

    // Wait for completion using your built-in shutdown wait.
    const auto t_run_start = Clock::now();
    pool.shutdown(args.shutdown_timeout_s);
    const auto t_run_end = Clock::now();

    const auto submit_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_submit_end - t_submit_start).count();
    const auto post_submit_wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_run_end - t_run_start).count();
    const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_run_end - t_submit_start).count();

    const double total_s = total_ms / 1000.0;
    const double throughput = args.jobs / (total_s > 0 ? total_s : 1e-9);

    std::cout << "Results:\n";
    std::cout << "  completed=" << completed.load(std::memory_order_acquire) << "/" << args.jobs << "\n";
    if (args.mode == "cpu") {
        std::cout << "  checksum=" << checksum.load(std::memory_order_relaxed) << "\n";
    }
    std::cout << "  submit_phase_ms=" << submit_ms << "\n";
    std::cout << "  post_submit_wait_ms=" << post_submit_wait_ms << "\n";
    std::cout << "  total_end_to_end_ms=" << total_ms << "\n";
    std::cout << "  throughput_jobs_per_sec=" << throughput << "\n";

    if (submit_ms > total_ms / 2) {
        std::cout << "  note=submission overlaps execution because the bounded queue applies backpressure\n";
    }

    // Simple sanity warning
    if (completed.load(std::memory_order_acquire) != args.jobs) {
        std::cerr << "\nWARNING: Not all jobs completed within shutdown timeout.\n"
                  << "Try increasing --shutdown_s or reducing --jobs/--iters.\n";
        return 1;
    }

    return 0;
}
