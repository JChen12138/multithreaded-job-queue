#pragma once
#include <thread>
#include <string>
#include <sstream>
#include <fmt/format.h>

template <>
struct fmt::formatter<std::thread::id> : fmt::formatter<std::string> {
    auto format(const std::thread::id& id, fmt::format_context& ctx) const {
        std::ostringstream oss;
        oss << id;
        return fmt::formatter<std::string>::format(oss.str(), ctx);
    }
};