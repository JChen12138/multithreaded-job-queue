# 🧵 Thread Pool with Job Queue (C++17)

A lightweight, modern **C++17 thread pool** implementation with a **thread-safe job queue**, **futures for result retrieval**, **Prometheus metrics**, and **graceful shutdown**.  
This project demonstrates core concepts of multithreading, synchronization, task scheduling, and basic observability, all using modern C++ and open source tools.

---

## 🚀 Features

- ✅ Thread-safe **JobQueue** using `std::mutex` and `std::condition_variable`
- ✅ Configurable **ThreadPool** with any number of worker threads
- ✅ `submit()` API supporting **any callable** (lambda, function, functor)
- ✅ Supports **return values via `std::future`**
- ✅ Tracks active jobs using atomic counters
- ✅ **Graceful shutdown** that waits for all jobs to finish
- ✅ Integrated **logging** using [`spdlog`](https://github.com/gabime/spdlog)
- ✅ Clean, modern C++17 syntax with RAII and move semantics
- ✅ Built-in **retry mechanism** with metadata (job ID, name, retries)
- ✅ Command-line options for **thread count**, **queue size**, and **shutdown timeout**
- ✅ **Prometheus-style metrics** via [`prometheus-cpp`](https://github.com/jupp0r/prometheus-cpp)
  - Job counters: submitted / completed / failed
  - Active job gauge
  - `/metrics` HTTP endpoint powered by built-in Exposer

---

## 📁 Project Structure

```
.
├── include/
│   ├── JobQueue.hpp        # Thread-safe queue for job storage
│   ├── ThreadPool.hpp      # Core thread pool logic
│   ├── Metrics.hpp         # Lightweight wrapper around prometheus-cpp
│   └── MetricsServer.hpp   # Singleton exposer registry
├── src/
│   ├── JobQueue.cpp
│   ├── ThreadPool.cpp
│   ├── Metrics.cpp
│   └── MetricsServer.cpp
├── main.cpp                # Example usage and test driver
├── README.md               # Project documentation
└── .gitignore              # Git ignored files
```

---

## ⚙️ Build Instructions

### 🧰 Requirements
- C++17-compatible compiler (GCC ≥ 9, Clang ≥ 10, or MSVC ≥ 2019)
- [`vcpkg`](https://github.com/microsoft/vcpkg) with:
  - `prometheus-cpp` (core + pull)
  - `spdlog`
  - `fmt`
  - `cxxopts` (for CLI)
  - `sqlite3`, `hiredis`, and `redis++` if enabled later

### 🛠️ Compile Example
```bash
g++ -std=c++17 -O2   -Iinclude   -IC:/path/to/vcpkg/installed/x64-mingw-static/include   -LC:/path/to/vcpkg/installed/x64-mingw-static/lib   main.cpp src/*.cpp -o server.exe   -static -lspdlog -lfmt -lprometheus-cpp-core -lprometheus-cpp-pull -lws2_32 -lmswsock
```

### ▶️ Run
```bash
./server --threads 2 --max_queue 10 --timeout 1
```

Then, visit: [http://localhost:8080/metrics](http://localhost:8080/metrics)

---

## 💡 Example Output

```
[info] Job submitted: ID = 0, Name = Job_0
[info] Running job ID = 0, Name = Job_0, on thread 3
[info] Executing job: Job_0 (ID: 0)
[info] Waiting for 10 jobs to finish
[warning] Graceful shutdown timeout reached. Proceeding with forced shutdown.
```

---

## 📊 Metrics Overview

| Metric | Type | Description |
|--------|------|-------------|
| `jobs_submitted_total` | Counter | Total number of jobs submitted |
| `jobs_completed_total` | Counter | Jobs successfully completed |
| `jobs_failed_total`    | Counter | Jobs that threw exceptions |
| `active_jobs`          | Gauge   | Number of currently running jobs |

---

## 🧠 Key Concepts Demonstrated

| Concept | Description |
|:--|:--|
| **Condition Variables** | Efficiently wait for new jobs without busy-waiting |
| **Mutexes** | Protect shared queue access |
| **Atomic Counters** | Safely track active jobs across threads |
| **Futures / Packaged Tasks** | Retrieve results of async jobs |
| **Graceful Shutdown** | Waits for all jobs to finish or timeout |
| **Retry Mechanism** | Automatically re-enqueue jobs on failure |
| **Logging** | Structured, thread-aware logs with `spdlog` |

---

## 🔍 Future Improvements

- [ ] Add task prioritization with priority queue
- [ ] Work stealing among worker threads
- [ ] Job timeout + cancellation
- [ ] Setup **Docker + Prometheus + Grafana** stack

---

## 👩‍💻 Author

**Weijia (J) Chen**  
C++ Backend / Systems Developer  

This project was built as part of a hands-on learning path to strengthen understanding of **concurrency, backend systems design, and multithreaded C++**.

---

## 📜 License

MIT License © 2025 Weijia Chen
