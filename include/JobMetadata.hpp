#pragma once

#include <string>
#include <chrono>
#include <atomic>

struct JobMetadata {
    int id = -1;
    std::string name;
    std::chrono::system_clock::time_point timestamp;
    int max_retries = 0;
    int current_retry = 0;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(0); // 0 = no timeout
    std::atomic<bool> cancel_requested{false};
    bool allow_retry = true;

    JobMetadata() = default;

    JobMetadata(int id_, std::string name_, int max_retries_ = 0)
        : id(id_), name(std::move(name_)),
          timestamp(std::chrono::system_clock::now()),
          max_retries(max_retries_) {}

    // --- Custom Move Constructor ---
    JobMetadata(JobMetadata&& other) noexcept
        : id(other.id),
          name(std::move(other.name)),
          timestamp(other.timestamp),
          max_retries(other.max_retries),
          current_retry(other.current_retry),
          timeout(other.timeout) {
        cancel_requested.store(other.cancel_requested.load());
    }

    // --- Custom Move Assignment ---
    JobMetadata& operator=(JobMetadata&& other) noexcept {
        if (this != &other) {
            id = other.id;
            name = std::move(other.name);
            timestamp = other.timestamp;
            max_retries = other.max_retries;
            current_retry = other.current_retry;
            timeout = other.timeout;
            cancel_requested.store(other.cancel_requested.load());
        }
        return *this;
    }

    // --- Delete copy constructor and assignment (can't copy atomic) ---
    JobMetadata(const JobMetadata&) = delete;
    JobMetadata& operator=(const JobMetadata&) = delete;
};

