#include "Metrics.hpp"
#include "MetricsServer.hpp"
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>

using namespace prometheus;

void Metrics::init(const std::string& endpoint) {
    endpoint_ = endpoint;
    spdlog::info("Metrics initialized at endpoint: {}", endpoint_);
    MetricsServer::instance().start("127.0.0.1:8080");

    registry_ = MetricsServer::instance().getRegistry();

    auto& counter_family = BuildCounter()
        .Name("jobs_submitted_total")
        .Help("Total number of jobs submitted")
        .Register(*registry_);
    job_submitted_ = &counter_family.Add({});

    auto& completed_family = BuildCounter()
        .Name("jobs_completed_total")
        .Help("Total number of jobs completed")
        .Register(*registry_);
    job_completed_ = &completed_family.Add({});

    auto& failed_family = BuildCounter()
        .Name("jobs_failed_total")
        .Help("Total number of jobs failed")
        .Register(*registry_);
    job_failed_ = &failed_family.Add({});

    auto& gauge_family = BuildGauge()
        .Name("active_jobs")
        .Help("Current number of active jobs")
        .Register(*registry_);
    active_jobs_ = &gauge_family.Add({});
}
