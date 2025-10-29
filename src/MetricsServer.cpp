#include "MetricsServer.hpp"

MetricsServer& MetricsServer::instance() {
    static MetricsServer instance;
    return instance;
}

void MetricsServer::start(const std::string& address) {
    if (exposer_) return;  // Prevent multiple starts
    registry_ = std::make_shared<prometheus::Registry>();
    exposer_ = std::make_unique<prometheus::Exposer>(address);
    exposer_->RegisterCollectable(registry_);
}

std::shared_ptr<prometheus::Registry> MetricsServer::getRegistry() {
    return registry_;
}
