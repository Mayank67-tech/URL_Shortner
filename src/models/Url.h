#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct UrlRecord {
    int64_t     id        = 0;
    std::string shortCode;
    std::string longUrl;
    std::string createdAt;
};

struct AnalyticsEvent {
    std::string shortCode;
    std::string timestamp;
    std::string country;
    std::string browser;
    std::string device;
};

struct AnalyticsResult {
    int64_t                  clicks = 0;
    std::vector<std::string> topCountries;
    std::string              lastAccess;
};
