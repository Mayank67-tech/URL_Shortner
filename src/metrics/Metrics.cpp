#include "Metrics.h"

#include <iostream>

Metrics& Metrics::instance()
{
    static Metrics inst;
    return inst;
}

void Metrics::init(const std::string& bindAddress)
{
    std::call_once(initFlag_, [this, &bindAddress]() {
        try {
            exposer_ = std::make_unique<prometheus::Exposer>(bindAddress);

            registry_ = std::make_shared<prometheus::Registry>();

            requestCounter_ = &prometheus::BuildCounter()
                .Name("http_requests_total")
                .Help("Total number of HTTP requests")
                .Register(*registry_);

            latencyHistogram_ = &prometheus::BuildHistogram()
                .Name("http_request_duration_seconds")
                .Help("HTTP request latency in seconds")
                .Register(*registry_);

            cacheCounter_ = &prometheus::BuildCounter()
                .Name("cache_operations_total")
                .Help("Total cache operations (hits and misses)")
                .Register(*registry_);

            activeConnGauge_ = &prometheus::BuildGauge()
                .Name("active_connections")
                .Help("Number of currently active connections")
                .Register(*registry_);

            exposer_->RegisterCollectable(registry_);

            initialized_ = true;

            std::cerr << "[Metrics] Prometheus metrics initialised on "
                      << bindAddress << '\n';

        } catch (const std::exception& e) {
            std::cerr << "[Metrics] init failed: " << e.what() << '\n';
        }
    });
}

void Metrics::recordRequest(const std::string& method,
                            const std::string& path,
                            const std::string& status)
{
    if (!initialized_) return;

    try {
        requestCounter_->Add({{"method", method},
                              {"path",   path},
                              {"status", status}})
            .Increment();
    } catch (const std::exception& e) {
        std::cerr << "[Metrics] recordRequest error: " << e.what() << '\n';
    }
}

void Metrics::recordLatency(const std::string& method,
                            const std::string& path,
                            double seconds)
{
    if (!initialized_) return;

    try {
        static const prometheus::Histogram::BucketBoundaries buckets{
            0.001, 0.005, 0.01, 0.025, 0.05,
            0.1,   0.25,  0.5,  1.0,   2.5, 5.0};

        latencyHistogram_->Add({{"method", method},
                                {"path",   path}},
                               buckets)
            .Observe(seconds);
    } catch (const std::exception& e) {
        std::cerr << "[Metrics] recordLatency error: " << e.what() << '\n';
    }
}

void Metrics::recordCacheHit()
{
    if (!initialized_) return;

    try {
        cacheCounter_->Add({{"result", "hit"}}).Increment();
    } catch (const std::exception& e) {
        std::cerr << "[Metrics] recordCacheHit error: " << e.what() << '\n';
    }
}

void Metrics::recordCacheMiss()
{
    if (!initialized_) return;

    try {
        cacheCounter_->Add({{"result", "miss"}}).Increment();
    } catch (const std::exception& e) {
        std::cerr << "[Metrics] recordCacheMiss error: " << e.what() << '\n';
    }
}

void Metrics::setActiveConnections(double count)
{
    if (!initialized_) return;

    try {
        activeConnGauge_->Add({}).Set(count);
    } catch (const std::exception& e) {
        std::cerr << "[Metrics] setActiveConnections error: "
                  << e.what() << '\n';
    }
}
