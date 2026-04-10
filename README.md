# Multithreaded Job Queue (C++17)

A hands-on C++17 backend/systems project focused on building a thread-safe job pipeline with bounded queues, priority scheduling, retry semantics, deadline-based job expiry, futures, observability hooks, and graceful shutdown.

## Project Goal

Build a portfolio-quality concurrency project that demonstrates:

- Correctness under contention
- Clear lifecycle and shutdown behavior
- Practical debugging and test coverage
- Benchmarking and observability basics

## Recent Additions (as of April 2026)

- Replaced the old per-job timeout thread path with deadline-based expiry checked by worker threads
- Removed detached timeout execution so shutdown no longer leaves hidden background work behind
- Clarified retry accounting so only terminal failures update failure metrics
- Made `JobQueue::push()` report enqueue rejection during shutdown so submit/retry paths do not silently lose work
- Ensured terminal pre-run failures (for example, expiry before execution or retry requeue rejection during shutdown) complete `future`-based jobs with an exception
- Fixed terminal exception handling for `future`-based jobs when retry is disabled so `future.get()` does not hang
- Added edge-case coverage showing:
  - a running job is allowed to finish even if its timeout budget is smaller
  - a queued job that misses its deadline is skipped before execution
  - a future-based queued job that expires before execution completes with an exception instead of hanging
  - a future-based job that throws with `allow_retry=false` still completes with an exception
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
- `JobQueue::push()` can reject enqueue during shutdown; submit and retry paths treat that as a real failure instead of silently dropping the job.
- `shutdown(timeout_s)` is a graceful shutdown wait budget: it waits for tracked in-flight jobs, then closes the queue and joins worker threads.
- For `future`-based jobs, terminal failures (including pre-run expiry and non-retry execution exceptions) complete the promise with an exception rather than leaving `future.get()` blocked indefinitely.
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

### Docker Demo

A minimal Docker stack is included for the app, Prometheus, and Grafana.

Run:

```bash
docker compose up --build
```

Endpoints:

```text
App metrics: http://localhost:8080/metrics
Prometheus:  http://localhost:9090
Grafana:     http://localhost:3000
```

Notes:

- The app image builds the `server` target with CMake + vcpkg manifest mode (`vcpkg.json`).
- The default compose command keeps the process alive for 300 seconds after thread-pool shutdown so Prometheus and Grafana have time to scrape exported metrics.
- Metrics bind to `127.0.0.1` for local runs and `0.0.0.0` when running inside Docker. You can also override the bind address with `JOB_QUEUE_METRICS_BIND`.

### CMake Status

The repository now includes a working CMake path for tests and bench builds with MinGW + vcpkg. The full `server` target still depends on metrics-related packages (`prometheus-cpp`, `cxxopts`) being installed for the selected triplet.

Verified locally for test targets with:

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe -DCMAKE_TOOLCHAIN_FILE=C:/Users/16210/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-mingw-static -DBUILD_SERVER=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

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
- Expired future job completes with exception
- Future job failure without retry completes with exception (no hang)
- LRU cache insert/get/eviction

Verified locally on March 26, 2026:

- `test_edge_cases.exe`: passed
- `test_job_queue.exe`: passed
- `ctest --test-dir build --output-on-failure`: passed

Verified locally on April 9, 2026:

- `test_edge_cases.exe`: passed

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
- enqueue is treated as accepted only if `JobQueue::push()` succeeds
- future-based submit uses a `std::promise`
- if retries are enabled, intermediate failures do not complete the promise; the `future` reflects the final logical outcome
- `jobs_in_progress_` is incremented so shutdown can wait for outstanding work

2. Queue (Bounded + Priority Scheduling)
- `JobQueue` owns the shared container `std::priority_queue<Job>` where lower `priority` values run first
- queue access is protected by `JobQueue::mutex_`
- `not_full_cv_` blocks producers when the queue is full
- `not_empty_cv_` blocks consumers when the queue is empty
- `push()` returns `false` if shutdown is observed before enqueue succeeds
- shutdown sets `JobQueue::shutdown_` and wakes blocked producers and consumers so they can exit cleanly

3. Worker (Execution + Retry + Deadline Expiry)
- each worker blocks on `JobQueue::pop()`
- before executing a timed job, the worker compares `enqueue_time + timeout` against the current start time
- if the deadline has already passed, the job is skipped and counted as failed without executing `job.task()`
- future-based jobs use a terminal-failure callback so pre-run expiry and failed retry requeue still complete the promise with an exception
- if the job has already started running, the pool does not try to interrupt it
- on exception, the job may be re-enqueued if retry is enabled and retry budget remains
- retry is considered successful only after the re-enqueue actually succeeds
- on terminal completion, `jobs_in_progress_` is decremented and `cv_done_` can wake shutdown waiters

> Note: this project intentionally avoids detached timeout threads. A timeout means "expired before start," not "preempted while running."

## Concurrency Guarantees

- `JobQueue::mutex_` protects queue state, including `queue_`, `shutdown_`, and the wait predicates used by `push()` and `pop()`.
- `ThreadPool::running_` is atomic because submitters and shutdown coordination read and update pool state across threads without relying on the queue lock.
- `ThreadPool::jobs_in_progress_` is atomic because submitters, workers, and shutdown logic all observe or update it concurrently.
- `ThreadPool::done_mutex_` is used only with `cv_done_` for shutdown waiting and notification; it is separate from queue-state protection.
- Worker threads block on the queue condition variable instead of spinning, so idle waiting does not depend on busy-polling.

## Why No Circular Wait Exists

- Queue coordination waits on `JobQueue::mutex_`.
- Shutdown waiting uses `done_mutex_`.
- The current design does not require a thread to hold `done_mutex_` while waiting for `JobQueue::mutex_`, or the reverse.
- Because queue coordination and shutdown coordination are separated, the lock design avoids a circular-wait cycle between those synchronization domains.

## Graceful Shutdown Design

- `shutdown(timeout_s)` first waits up to the configured graceful shutdown budget for `jobs_in_progress_` to reach zero.
- After that wait completes or times out, `ThreadPool::running_` is set to `false` and `job_queue_.shutdown()` marks the queue as closed.
- `notify_all` is required so threads blocked in `JobQueue::pop()` and producers blocked in `JobQueue::push()` can wake up, observe shutdown, and stop waiting.
- Workers exit by calling `pop()` again; when shutdown is active and the queue is empty, `pop()` returns the sentinel job and `worker_loop()` breaks out of its loop.

```text
Running
  -> shutdown(timeout_s)
Waiting For In-Flight Jobs
  -> jobs_in_progress_ == 0 or wait budget expires
Closing Queue
  -> running_ = false
  -> job_queue_.shutdown()
Waking Blocked Threads
  -> not_empty_cv_.notify_all()
  -> not_full_cv_.notify_all()
Workers Exit
  -> pop() returns sentinel when shutdown && queue empty
Stopped
```

## Roadmap

- Align CMake targets with the full runtime, test, and bench feature set
- Add a first-class cancellation token API if mid-execution cooperative cancellation becomes a goal
- Tighten logging phrasing around "picked" vs. "running" jobs in the expiry path
- Consider a non-blocking retry requeue strategy or separate retry buffer so workers do not block on bounded-queue re-enqueue under heavy pressure
- Add a pre-provisioned Grafana dashboard for the exported queue and latency metrics

## Author

**Weijia (J) Chen**  
C++ Backend / Systems Developer

## License

MIT License (c) 2025 Weijia Chen
