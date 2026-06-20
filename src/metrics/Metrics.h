#pragma once

#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>

#include <memory>
#include <mutex>
#include <string>

class Metrics {
public:
    static Metrics& instance();

    void init(const std::string& bindAddress = "0.0.0.0:9090");

    void recordRequest(const std::string& method,
                       const std::string& path,
                       const std::string& status);

    void recordLatency(const std::string& method,
                       const std::string& path,
                       double seconds);

    void recordCacheHit();

    void recordCacheMiss();

    void setActiveConnections(double count);

private:
    Metrics() = default;
    ~Metrics() = default;

    Metrics(const Metrics&)            = delete;
    Metrics& operator=(const Metrics&) = delete;

    std::once_flag                                initFlag_;
    std::unique_ptr<prometheus::Exposer>          exposer_;
    std::shared_ptr<prometheus::Registry>         registry_;

    prometheus::Family<prometheus::Counter>*   requestCounter_    = nullptr;
    prometheus::Family<prometheus::Histogram>*  latencyHistogram_  = nullptr;
    prometheus::Family<prometheus::Counter>*   cacheCounter_      = nullptr;
    prometheus::Family<prometheus::Gauge>*     activeConnGauge_   = nullptr;

    bool initialized_ = false;
};
