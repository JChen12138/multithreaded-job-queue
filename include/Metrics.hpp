#pragma once
#include <string>
#include <memory>
#include <spdlog/spdlog.h>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>
#include <prometheus/histogram.h>


class Metrics {
public:
    static Metrics& instance() {
        static Metrics instance;
        return instance;
    }

    void init(const std::string& endpoint = "/metrics");

    // Accessors for Prometheus metrics
    prometheus::Counter& job_submitted() { return *job_submitted_; }
    prometheus::Counter& job_completed() { return *job_completed_; }
    prometheus::Counter& job_failed()    { return *job_failed_; }
    prometheus::Gauge&   active_jobs()   { return *active_jobs_; }
    prometheus::Histogram& job_latency();


private:
    Metrics() = default;

    std::shared_ptr<prometheus::Registry> registry_;
    prometheus::Counter* job_submitted_ = nullptr;
    prometheus::Counter* job_completed_ = nullptr;
    prometheus::Counter* job_failed_ = nullptr;
    prometheus::Gauge*   active_jobs_ = nullptr;
    prometheus::Histogram* job_latency_;

    std::string endpoint_;
};
