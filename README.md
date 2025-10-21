# 🧵 Thread Pool with Job Queue (C++17)

A lightweight, modern **C++17 thread pool** implementation with a **thread-safe job queue**, **futures for result retrieval**, and **graceful shutdown support**.  
This project demonstrates core concepts of multithreading, synchronization, and asynchronous task scheduling using only the C++ standard library.

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

---

## 📁 Project Structure

```
.
├── JobQueue.hpp          # Thread-safe producer-consumer queue
├── ThreadPool.hpp        # Thread pool class definition
├── ThreadPool.cpp        # Thread pool implementation
├── main.cpp              # Example usage and test driver
└── README.md             # Project documentation
```

---

## ⚙️ Build Instructions

### 🧰 Requirements
- C++17 or newer (GCC ≥ 9 / Clang ≥ 10 / MSVC ≥ 2019)
- [`spdlog`](https://github.com/gabime/spdlog) installed (or via vcpkg)

### 🛠️ Compile Example
```bash
g++ -std=c++17 -pthread -Iinclude     ThreadPool.cpp main.cpp -o threadpool_demo     -lspdlog
```

### ▶️ Run
```bash
./threadpool_demo
```

---

## 💡 Example Output

```
[2025-10-17 14:25:52.104] [thread 1235] [info] Running job 0 on thread 140703220803328
[2025-10-17 14:25:52.104] [thread 1236] [info] Running job 1 on thread 140703229196032
...
[2025-10-17 14:25:52.605] [thread 1238] [info] Computing result...
[2025-10-17 14:25:52.606] [thread 1237] [info] Result received: 42
```

---

## 🧩 Example Code (main.cpp)

```cpp
#include <iostream>
#include "ThreadPool.hpp"
#include <spdlog/spdlog.h>

int main() {
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [thread %t] [%l] %v");
    spdlog::set_level(spdlog::level::info);

    ThreadPool pool(4);

    // Normal jobs
    for (int i = 0; i < 10; ++i) {
        pool.submit([i]() {
            spdlog::info("Running job {} on thread {}", i, std::this_thread::get_id());
        });
    }

    // Job returning a result
    auto future = pool.submit([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        spdlog::info("Computing result...");
        return 42;
    });

    spdlog::info("Waiting for result...");
    int result = future.get();
    spdlog::info("Result received: {}", result);

    pool.shutdown();
}
```

---

## 🧠 Key Concepts Demonstrated

| Concept | Description |
|:--|:--|
| **Condition Variables** | Efficiently wait for new jobs without busy-waiting |
| **Mutexes** | Protect shared queue access |
| **Atomic Counters** | Safely track active jobs across threads |
| **RAII** | Automatic cleanup of threads and resources |
| **Futures / Packaged Tasks** | Retrieve results of async jobs |
| **Logging** | Thread-safe structured runtime tracing with `spdlog` |

---

## 🔍 Future Improvements

- [ ] Add task prioritization (priority queue)
- [ ] Implement work stealing between threads
- [ ] Support timed `try_pop()` for shutdown control
- [ ] Add benchmarking for throughput and latency

---

## 👩‍💻 Author

**Weijia (J) Chen**  
C++ Backend / Systems Developer  

This project was built as part of a hands-on learning path to strengthen understanding of **concurrency, backend systems design, and multithreaded C++**.

---

## 📜 License

MIT License © 2025 Weijia Chen
