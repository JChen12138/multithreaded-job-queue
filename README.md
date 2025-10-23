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
- ✅ Built-in **retry mechanism** with metadata (job ID, name, retries)
- ✅ Command-line options for **thread count**, **queue size**, and **shutdown timeout**

---

## 📁 Project Structure

```
.
├── include/
│   ├── JobQueue.hpp        # Thread-safe producer-consumer queue
│   └── ThreadPool.hpp      # Thread pool class definition
├── src/
│   ├── JobQueue.cpp        # JobQueue implementation (if needed)
│   └── ThreadPool.cpp      # Thread pool implementation
├── main.cpp                # Example usage and test driver
├── README.md               # Project documentation
└── .gitignore              # Git ignored files
```

---

## ⚙️ Build Instructions

### 🧰 Requirements
- C++17 or newer (GCC ≥ 9 / Clang ≥ 10 / MSVC ≥ 2019)
- [`spdlog`](https://github.com/gabime/spdlog) installed (or via vcpkg)

### 🛠️ Compile Example
```bash
g++ -std=c++17 -O2 -Iinclude -IC:/path/to/vcpkg/installed/x64-mingw-static/include \
    -LC:/path/to/vcpkg/installed/x64-mingw-static/lib \
    main.cpp src/JobQueue.cpp src/ThreadPool.cpp -o server.exe \
    -static -lspdlog -lfmt -lws2_32 -lmswsock
```

### ▶️ Run
```bash
./server --threads 2 --max_queue 10 --timeout 1
```

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

## 🧩 Example Code (main.cpp)

```cpp
int main(int argc, char* argv[]) {
    cxxopts::Options options("server", "Multithreaded Job Queue Demo");
    options.add_options()
        ("threads", "Number of worker threads", cxxopts::value<int>()->default_value("4"))
        ("max_queue", "Max job queue size", cxxopts::value<int>()->default_value("16"))
        ("timeout", "Shutdown wait timeout", cxxopts::value<int>()->default_value("5"));
    auto result = options.parse(argc, argv);

    int num_threads = result["threads"].as<int>();
    int max_queue = result["max_queue"].as<int>();
    int timeout = result["timeout"].as<int>();

    ThreadPool pool(num_threads, max_queue);
    for (int i = 0; i < 10; ++i) {
        pool.submit({i, "Job_" + std::to_string(i)}, [i]() {
            spdlog::info("Executing job: Job_{} (ID: {})", i, i);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        });
    }

    auto future = pool.submit({42, "ComputeAnswer"}, []() -> int {
        spdlog::info("Computing result...");
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        return 42;
    });

    spdlog::info("Waiting for result...");
    int result_value = future.get();
    spdlog::info("Result received: {}", result_value);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    pool.shutdown(timeout);
}
```

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
- [ ] Metrics exporter (e.g., Prometheus)
- [ ] Job timeout + cancellation

---

## 👩‍💻 Author

**Weijia (J) Chen**  
C++ Backend / Systems Developer  

This project was built as part of a hands-on learning path to strengthen understanding of **concurrency, backend systems design, and multithreaded C++**.

---

## 📜 License

MIT License © 2025 Weijia Chen
