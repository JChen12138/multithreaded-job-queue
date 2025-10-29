#pragma once
#include <memory>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

class MetricsServer {
public:
    static MetricsServer& instance();
    void start(const std::string& address = "127.0.0.1:8080");
    std::shared_ptr<prometheus::Registry> getRegistry();

private:
    MetricsServer() = default;
    std::unique_ptr<prometheus::Exposer> exposer_;
    std::shared_ptr<prometheus::Registry> registry_;
};
