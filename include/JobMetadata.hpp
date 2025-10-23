#pragma once

#include <string>
#include <chrono>

struct JobMetadata {
    JobMetadata() = default;
    int id;
    std::string name;
    std::chrono::system_clock::time_point timestamp;
    int max_retries = 0;
    int current_retry = 0;

    JobMetadata(int id_, std::string name_, int max_retries_ = 0)
        : id(id_), name(std::move(name_)), timestamp(std::chrono::system_clock::now()), max_retries(max_retries_) {}
};
