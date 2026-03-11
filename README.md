# Multithreaded Job Queue (C++17)

A hands-on C++17 backend/systems project focused on building a thread-safe job pipeline with bounded queues, priority scheduling, retry semantics, deadline-based job expiry, futures, observability hooks, and graceful shutdown.

## Project Goal

Build a portfolio-quality concurrency project that demonstrates:

- Correctness under contention
- Clear lifecycle and shutdown behavior
- Practical debugging and test coverage
- Benchmarking and observability basics

## Recent Additions (as of March 2026)

- Replaced the old per-job timeout thread path with deadline-based expiry checked by worker threads
- Removed detached timeout execution so shutdown no longer leaves hidden background work behind
- Clarified retry accounting so only terminal failures update failure metrics
- Added edge-case coverage showing:
  - a running job is allowed to finish even if its timeout budget is smaller
  - a queued job that misses its deadline is skipped before execution
- Kept the worker model bounded: only pool workers execute jobs

## Current Scope

- Thread-safe bounded `JobQueue` with condition variables and priority scheduling
- `ThreadPool` with blocking worker loop (`pop()`), retry flow, deadline-based expiry, and `future`-based submit
- Optional Prometheus metrics integration
- Thread-safe `LRUCache` utility
- Catch2 tests for queue, pool edge cases, retry behavior, deadline-expiry behavior, and LRU cache

## Features

- Thread-safe `JobQueue` (`std::mutex` + `std::condition_variable`)
- Bounded queue with producer backpressure
- Priority scheduling (`std::priority_queue`, lower numeric priority runs first)
- `ThreadPool::submit()` overloads for fire-and-forget and `std::future` result retrieval
- Metadata-driven retry (`allow_retry`, `max_retries`, `current_retry`)
- Per-job deadline/expiry support via `JobMetadata::timeout`
- Shutdown coordination with in-flight job accounting (`jobs_in_progress_`)
- Logging with `spdlog`
- Optional Prometheus metrics (`jobs_submitted_total`, `jobs_completed_total`, `jobs_failed_total`, `active_jobs`, `job_latency_seconds`)
- Thread-safe `LRUCache`

## Important Behavior Notes

- Timeout is implemented as a deadline check before execution, not as preemptive interruption of a running task.
- No per-job execution threads are created for timeout handling, and no detached worker threads are left behind.
- A running task is allowed to finish once it has started, even if its timeout budget would have elapsed during execution.
- `shutdown(timeout_s)` is a graceful shutdown wait budget: it waits for tracked in-flight jobs, then closes the queue and joins worker threads.
- `active_jobs` currently tracks in-flight submitted jobs (queued + running), not only jobs actively executing on CPU.
- `DISABLE_METRICS` is mainly intended for tests, bench runs, and minimal builds without Prometheus linkage.

## Project Structure

```text
.
|-- include/
|   |-- JobMetadata.hpp
|   |-- JobQueue.hpp
|   |-- LRUCache.hpp
|   |-- Metrics.hpp
|   |-- MetricsServer.hpp
|   |-- MetricsStub.hpp
|   |-- ThreadPool.hpp
|   `-- formatters/thread_id_formatter.hpp
|-- src/
|   |-- JobQueue.cpp
|   |-- Metrics.cpp
|   |-- MetricsServer.cpp
|   `-- ThreadPool.cpp
|-- test/
|   |-- test_edge_cases.cpp
|   |-- test_job_queue.cpp
|   `-- test_LRUCache.cpp
|-- main.cpp
|-- bench.cpp
|-- CMakeLists.txt
`-- README.md
```

## Build and Run

### Requirements

- C++17 compiler (GCC/Clang/MSVC)
- vcpkg packages used by this repo:
  - `spdlog`
  - `fmt`
  - `cxxopts`
  - `prometheus-cpp` (if metrics-enabled build)
  - Catch2 (for tests)

### Canonical Local Build (manual g++, current workflow)

```bash
g++ -std=c++17 -O2 -Iinclude -IC:/path/to/vcpkg/installed/x64-mingw-static/include -LC:/path/to/vcpkg/installed/x64-mingw-static/lib main.cpp src/JobQueue.cpp src/ThreadPool.cpp src/Metrics.cpp src/MetricsServer.cpp -o server.exe -static -lspdlog -lfmt -lprometheus-cpp-core -lprometheus-cpp-pull -lws2_32 -lmswsock
```

Run:

```bash
./server --threads 2 --max_queue 10 --job_timeout 150
```

Metrics endpoint:

```text
http://localhost:8080/metrics
```

### CMake Status

The current `CMakeLists.txt` is not yet fully aligned with the full runtime feature set in `main.cpp` (metrics, cxxopts, and test targets are not fully modeled there yet). For now, the manual build commands above are the source of truth.

## Tests

Example edge-case build with metrics disabled:

```bash
g++ -std=c++17 src/JobQueue.cpp src/ThreadPool.cpp test/test_edge_cases.cpp -Iinclude -I"C:/path/to/vcpkg/installed/x64-mingw-static/include" -L"C:/path/to/vcpkg/installed/x64-mingw-static/lib" -lCatch2 -lspdlog -lfmt -D DISABLE_METRICS -o test_edge_cases.exe
```

Covered areas:

- Queue push/pop semantics and shutdown behavior
- Bounded queue blocking behavior
- Submit-after-shutdown error path
- Future-returning submit
- Retry-until-success behavior
- Running-job timeout semantics (started work is not preempted)
- Deadline expiry / skip-before-run coverage
- LRU cache insert/get/eviction

Verified locally on March 11, 2026:

- `test_edge_cases.exe`: passed
- `test_job_queue.exe`: passed

## Benchmark Notes

- `bench.cpp` reports `total_end_to_end_ms` as the main throughput metric.
- `submit_phase_ms` is not pure enqueue-only time because bounded queue backpressure can block producers while workers execute.
- `post_submit_wait_ms` covers remaining runtime until shutdown completes.
- Microsecond sleep workloads on Windows are scheduler/timer limited and are illustrative only.

## Metrics Overview

| Metric | Type | Description |
|--------|------|-------------|
| `jobs_submitted_total` | Counter | Total jobs accepted by submit |
| `jobs_completed_total` | Counter | Jobs completed successfully |
| `jobs_failed_total` | Counter | Terminal failure paths recorded by the implementation |
| `active_jobs` | Gauge | In-flight submitted jobs (queued + running) |
| `job_latency_seconds` | Histogram | Observed job execution latency |

## Architecture (Submit -> Queue -> Worker)

```text
Layer 1: Submit
  ThreadPool::submit(...)
  - validates running state
  - increments jobs_in_progress_
  - enqueues Job{metadata, task}

Layer 2: Queue
  JobQueue (bounded + priority)
  - producer waits on not_full_cv_
  - consumer waits on not_empty_cv_
  - shutdown wakes blocked producers/consumers

Layer 3: Worker
  ThreadPool::worker_loop()
  - blocking pop()
  - optional deadline-expiry check before execution
  - optional retry re-enqueue path
  - decrements jobs_in_progress_ on terminal completion
```

## Execution Flow Overview

This project follows a simple three-layer pipeline: Submit -> Queue -> Worker.

1. Submit (Producer)
- `ThreadPool::submit(...)` checks the pool-level stop flag `running_`
- the task is wrapped and pushed into the shared `JobQueue`
- future-based submit uses a `std::promise`
- if retries are enabled, intermediate failures do not complete the promise; the `future` reflects the final logical outcome
- `jobs_in_progress_` is incremented so shutdown can wait for outstanding work

2. Queue (Bounded + Priority Scheduling)
- `JobQueue` owns the shared container `std::priority_queue<Job>` where lower `priority` values run first
- queue access is protected by `JobQueue::mutex_`
- `not_full_cv_` blocks producers when the queue is full
- `not_empty_cv_` blocks consumers when the queue is empty
- shutdown sets `JobQueue::shutdown_` and wakes blocked producers and consumers so they can exit cleanly

3. Worker (Execution + Retry + Deadline Expiry)
- each worker blocks on `JobQueue::pop()`
- before executing a timed job, the worker compares `enqueue_time + timeout` against the current start time
- if the deadline has already passed, the job is skipped and counted as failed without executing `job.task()`
- if the job has already started running, the pool does not try to interrupt it
- on exception, the job may be re-enqueued if retry is enabled and retry budget remains
- on terminal completion, `jobs_in_progress_` is decremented and `cv_done_` can wake shutdown waiters

> Note: this project intentionally avoids detached timeout threads. A timeout means "expired before start," not "preempted while running."

## Roadmap

- Align CMake targets with the full runtime, test, and bench feature set
- Add a first-class cancellation token API if mid-execution cooperative cancellation becomes a goal
- Tighten logging phrasing around "picked" vs. "running" jobs in the expiry path
- Add Docker + Prometheus + Grafana demo stack

## Author

**Weijia (J) Chen**  
C++ Backend / Systems Developer

## License

MIT License (c) 2025 Weijia Chen
