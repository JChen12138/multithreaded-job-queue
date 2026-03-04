# Thread Pool with Job Queue (C++17)

A lightweight, modern **C++17 thread pool** implementation with a **thread-safe job queue**,  **retry & timeout support**, **futures for result retrieval**, **Prometheus metrics**, and **graceful shutdown**.  
This project demonstrates core concepts of multithreading, synchronization, task scheduling, caching, and basic observability, all using modern C++ and open source tools.

---

## Recent Additions (as of February 2026)

- **Retry + unit test**: Added retry metadata (`allow_retry`, `max_retries`, `current_retry`) and a Catch2 test that verifies retries until success
- **Debug fix (API semantics)**: Found a retry test failure caused by `submit()` overload semantics mutating metadata (retry accidentally disabled). Fixed by ensuring retry behavior is fully controlled by **JobMetadata**
- **Priority scheduling**: JobQueue backed by `std::priority_queue` (min-heap behavior via comparator)
- **Per-job timeout**: Optional timeout with logging; long-running jobs can be marked as timed out/cancelled
- **More test coverage**: Edge-case tests (`test_edge_cases.cpp`) added alongside JobQueue and LRUCache tests
- **Metrics build toggle**: Added `MetricsStub.hpp` for builds/tests that compile with `-D DISABLE_METRICS`

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
│   ├── JobMetadata.hpp
│   ├── MetricsStub.hpp  
│   ├── Metrics.hpp         # Lightweight wrapper around prometheus-cpp
│   └── MetricsServer.hpp   # Singleton exposer registry
├── src/
│   ├── JobQueue.cpp
│   ├── ThreadPool.cpp
│   ├── Metrics.cpp
│   └── MetricsServer.cpp
├── test/
│   ├── test_LRUCache.cpp   # Unit test for LRU cache
│   ├── test_edge_cases.cpp
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
  - `prometheus-cpp` (core + pull) *(optional if you build with `-D DISABLE_METRICS`)*
  - `spdlog`
  - `fmt`
  - `cxxopts` (for CLI)
  - `sqlite3`, `hiredis`, and `redis++` if enabled later
  - Catch2 (for tests)


### Compile Example
```bash
g++ -std=c++17 -O2   -Iinclude   -IC:/path/to/vcpkg/installed/x64-mingw-static/include   -LC:/path/to/vcpkg/installed/x64-mingw-static/lib   main.cpp src/*.cpp -o server.exe   -static -lspdlog -lfmt -lprometheus-cpp-core -lprometheus-cpp-pull -lws2_32 -lmswsock
```

### Run
```bash
./server --threads 2 --max_queue 10 --job_timeout 150
```

Then, visit: [http://localhost:8080/metrics](http://localhost:8080/metrics)

### Run tests (example)
Disable metrics for unit tests if you want a minimal link:
```bash
g++ -std=c++17 src/JobQueue.cpp src/ThreadPool.cpp test/test_edge_cases.cpp ^
  -Iinclude ^
  -I"C:/path/to/vcpkg/installed/x64-mingw-static/include" ^
  -L"C:/path/to/vcpkg/installed/x64-mingw-static/lib" ^
  -lCatch2 -lspdlog -lfmt -D DISABLE_METRICS -o test_edge_cases.exe
```

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

## Benchmark Notes

- `bench.cpp` reports `total_end_to_end_ms` as the primary throughput metric.
- `submit_phase_ms` is not pure enqueue overhead: with a bounded queue, producers can block in `push()` while workers are already executing jobs.
- `post_submit_wait_ms` measures only the remaining time after submission finishes and before `shutdown()` returns.
- Very short sleep-based workloads, especially microsecond sleeps on Windows, are scheduler/timer limited and should be treated as illustrative rather than authoritative.

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
| **Test Coverage** | LRUCache & JobQueue & Edge cases tested with Catch2 |

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


## System Architecture

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
│     │  JobQueue  │ ◀─────── bounded, thread-safe, metadata + priority
│     └────────────┘         │
│     ▲        │             │
│     │        ▼             │
│ Timeout  Retry Logic       │ ◀───── optional behaviors driven by metadata
└────────┬───────────────────┘
         │
         ▼
  ┌────────────┐
  │ LRU Cache  │ ◀───── memoize expensive results (header-only)
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

## Three-Layer Architecture

```
┌────────────────────────────────────────────────────────────────────┐
│ Layer 1: Submit (Producer API)                                      │
│  ThreadPool::submit(...)                                            │
│   - stop flag: ThreadPool::running_ (std::atomic<bool>)             │
│   - jobs_in_progress_++                                             │
│   - enqueue JobQueue::Job{metadata, task}                           │
└───────────────┬────────────────────────────────────────────────────┘
                │
                ▼
┌────────────────────────────────────────────────────────────────────┐
│ Layer 2: Queue (Bounded + Priority)                                 │
│  JobQueue                                                           │
│   - container: std::priority_queue<Job> queue_  (min-heap by prio)   │
│   - mutex: JobQueue::mutex_                                         │
│   - CVs:   JobQueue::not_full_cv_  (push waits when full)           │
│           JobQueue::not_empty_cv_ (pop waits when empty)            │
│   - stop flag: JobQueue::shutdown_ (guarded by mutex_)              │
│                                                                     │
│  push(): lock(mutex_) → wait(not_full_cv_) → queue_.push → notify   │
│  pop():  lock(mutex_) → wait(not_empty_cv_) → pop → notify          │
│  shutdown(): set shutdown_=true → not_empty_cv_.notify_all()        │
└───────────────┬────────────────────────────────────────────────────┘
                │
                ▼
┌────────────────────────────────────────────────────────────────────┐
│ Layer 3: Worker (Execution)                                         │
│  ThreadPool::worker_loop()                                          │
│   - loop stop flag: ThreadPool::running_                            │
│   - pulls jobs via job_queue_.try_pop(...) + std::this_thread::yield │
│   - per-job cancel: JobMetadata::cancel_requested                    │
│   - per-job timeout: JobMetadata::timeout                            │
│   - retry: allow_retry/current_retry/max_retries                     │
│   - shutdown wait: cv_done_ + done_mutex_ (wait jobs_in_progress_=0) │
└────────────────────────────────────────────────────────────────────┘

LRUCache is independent (mutex only), not part of the core queue pipeline.
```

---

## Execution Flow Overview

This project follows a simple **three-layer pipeline**: **Submit → Queue → Worker**.

1) **Submit (Producer)**
- The main thread calls `ThreadPool::submit(...)`.
- `submit()` first checks the pool-level stop flag `running_`.
- The task is wrapped (future-based submit uses a `std::promise`) and pushed into the shared `JobQueue`.
- `jobs_in_progress_` is incremented so shutdown can wait for outstanding work.

2) **Queue (Bounded + Priority Scheduling)**
- `JobQueue` owns the shared container `std::priority_queue<Job>` (lower `priority` value runs first).
- Queue access is protected by `JobQueue::mutex_`.
- The queue supports backpressure with condition variables:
  - `not_full_cv_`: producers block in `push()` when the queue is full
  - `not_empty_cv_`: consumers block in `pop()` when the queue is empty
- Shutdown sets `JobQueue::shutdown_` and broadcasts `not_empty_cv_` to wake sleeping consumers.

3) **Worker (Execution + Retry/Timeout)**
- Each worker runs `ThreadPool::worker_loop()` until `running_` becomes false and the queue drains.
- Workers pull jobs from the queue, execute them, and update metrics.
- Optional behaviors are metadata-driven:
  - **Timeout**: jobs with `metadata.timeout > 0` are treated as time-limited
  - **Retry**: on exception, jobs may be re-enqueued if `allow_retry` and retry budget remains
  - **Cancel**: `cancel_requested` can prevent execution
- On completion (or final failure), `jobs_in_progress_` is decremented. When it reaches 0, workers notify `cv_done_` so `shutdown()` can proceed.

> Note: workers currently use `try_pop()` + `yield()`, so the queue's blocking `pop()` path is not yet used by the pool. This keeps the control flow simple, but a production-oriented revision would typically switch workers to blocking `pop()` to avoid busy-waiting and make condition-variable wakeups central to idle behavior.

---
