# Thread Pool with Job Queue (C++17)

A lightweight, modern **C++17 thread pool** implementation with a **thread-safe job queue**, **futures for result retrieval**, **Prometheus metrics**, and **graceful shutdown**.  
This project demonstrates core concepts of multithreading, synchronization, task scheduling, caching, and basic observability, all using modern C++ and open source tools.

---

## Recent Additions (as of December 2025)

- **Thread-safe LRU Cache** module added for result memoization
- **Unit tests for LRU Cache** with Catch2 framework
- **Debug build setup verified**
- **Unit tests for JobQueue** using Catch2

---

## Features

- Thread-safe **JobQueue** using `std::mutex` and `std::condition_variable`
- Configurable **ThreadPool** with any number of worker threads
- `submit()` API supporting **any callable** (lambda, function, functor)
- Supports **return values via `std::future`**
- Tracks active jobs using atomic counters
- **Graceful shutdown** that waits for all jobs to finish
- Integrated **logging** using [`spdlog`](https://github.com/gabime/spdlog)
- Clean, modern C++17 syntax with RAII and move semantics
- Built-in **retry mechanism** with metadata (job ID, name, retries)
- Per-job **timeout support** with logging and cancellation
- Command-line options for **thread count**, **queue size**, and **shutdown timeout**
- **Prometheus-style metrics** via [`prometheus-cpp`](https://github.com/jupp0r/prometheus-cpp)
  - Job counters: submitted / completed / failed
  - Active job gauge
  - `/metrics` HTTP endpoint powered by built-in Exposer
- **Priority-based scheduling** using a **min-heap priority queue**
  - Lower numeric value = higher priority
  - Example: priority `1` runs before `5`
- **Thread-safe LRU Cache** for memoization
- **Unit test coverage** for core components

---

## Project Structure

```
.
├── include/
│   ├── JobQueue.hpp        # Thread-safe queue for job storage
│   ├── ThreadPool.hpp      # Core thread pool logic
│   ├── LRUCache.hpp        # Thread-safe LRU cache module 
│   ├── Metrics.hpp         # Lightweight wrapper around prometheus-cpp
│   └── MetricsServer.hpp   # Singleton exposer registry
├── src/
│   ├── JobQueue.cpp
│   ├── ThreadPool.cpp
│   ├── LRUCache.cpp        # LRU cache implementation
│   ├── Metrics.cpp
│   └── MetricsServer.cpp
├── test/
│   ├── test_LRUCache.cpp   # Unit test for LRU cache
│   └── test_job_queue.cpp  # Unit test for job queue
├── main.cpp                # Example usage and test driver
├── README.md               # Project documentation
└── .gitignore              # Git ignored files
```

---

## Build Instructions

### Requirements
- C++17-compatible compiler (GCC ≥ 9, Clang ≥ 10, or MSVC ≥ 2019)
- [`vcpkg`](https://github.com/microsoft/vcpkg) with:
  - `prometheus-cpp` (core + pull)
  - `spdlog`
  - `fmt`
  - `cxxopts` (for CLI)
  - `sqlite3`, `hiredis`, and `redis++` if enabled later

### Compile Example
```bash
g++ -std=c++17 -O2   -Iinclude   -IC:/path/to/vcpkg/installed/x64-mingw-static/include   -LC:/path/to/vcpkg/installed/x64-mingw-static/lib   main.cpp src/*.cpp -o server.exe   -static -lspdlog -lfmt -lprometheus-cpp-core -lprometheus-cpp-pull -lws2_32 -lmswsock
```

### Run
```bash
./server --threads 2 --max_queue 10 --job_timeout 150
```

Then, visit: [http://localhost:8080/metrics](http://localhost:8080/metrics)

---

## Example Output

```
[info] Submitting normal jobs
[info] Queue size after push: 5
[info] Running job ID = 3, Name = Job_3, on thread 2
[info] Executing job: Job_3 (ID: 3)
[info] Running prio_1 job
[info] Running prio_5 job
[info] Running prio_9 job
[warning] Job Job_3 (ID: 3) timed out after 150ms
[info] Result received: 42
[info] Shutdown complete.
```

---

## Metrics Overview

| Metric | Type | Description |
|--------|------|-------------|
| `jobs_submitted_total` | Counter | Total number of jobs submitted |
| `jobs_completed_total` | Counter | Jobs successfully completed |
| `jobs_failed_total`    | Counter | Jobs that threw exceptions |
| `active_jobs`          | Gauge   | Number of currently running jobs |
| `job_latency_seconds`  | Histogram | Job execution latency (seconds) |

---

## Key Concepts Demonstrated

| Concept | Description |
|:--|:--|
| **Condition Variables** | Efficiently wait for new jobs without busy-waiting |
| **Mutexes** | Protect shared queue access |
| **Atomic Counters** | Safely track active jobs across threads |
| **Futures / Packaged Tasks** | Retrieve results of async jobs |
| **Graceful Shutdown** | Waits for all jobs to finish or timeout |
| **Retry Mechanism** | Automatically re-enqueue jobs on failure |
| **Timeout Logic** | Terminates long-running jobs after N ms |
| **Logging** | Structured, thread-aware logs with `spdlog` |
| **Priority Scheduling** | Higher-priority jobs are executed first |
| **Thread-safe Caching** | LRU cache to store computed results |
| **Test Coverage** | LRUCache & JobQueue tested with Catch2 |

---

## Future Improvements

- [ ] Setup **Docker + Prometheus + Grafana** observability stack
- [ ] Add performance benchmarking with `std::chrono` or Google Benchmark

---

## Author

**Weijia (J) Chen**  
C++ Backend / Systems Developer  

This project was built as part of a hands-on learning path to strengthen understanding of **concurrency, backend systems design, and multithreaded C++**.

---

## License

MIT License © 2025 Weijia Chen


## System Architecturex

```
┌──────────────┐
│  Main Thread │ ◀──── CLI options, job submission, shutdown
└──────┬───────┘
       │
       ▼
┌────────────────────────────┐
│        ThreadPool          │ ◀───── submit() API
│ ┌────────────────────────┐ │
│ │      Worker Threads    │ │ ◀───── std::thread pool
│ │  (loop + wait + exec)  │ │
│ └────────┬───────────────┘ │
│          ▼                 │
│     ┌────────────┐         │
│     │  JobQueue  │ ◀─────── bounded, thread-safe, supports metadata
│     └────────────┘         │
│     ▲        │             │
│     │        ▼             │
│ Timeout  Retry Logic       │ ◀───── optional wrappers
└────────┬───────────────────┘
         │
         ▼
  ┌────────────┐
  │ LRU Cache  │ ◀───── optionally queried before expensive jobs
  └────────────┘

         │
         ▼
  ┌────────────────────┐
  │  Metrics Server    │ ◀──── prometheus-cpp + Exposer
  └────────────────────┘
         ▲
         │
 Exposes /metrics endpoint
```
