#pragma once

#include <string>
#include <functional>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "../database/PostgresPool.h"
#include "../threadpool/ThreadPool.h"
#include "../models/Url.h"

using namespace url_shortener;

class AnalyticsService {
public:
    AnalyticsService(PostgresPool& db, ThreadPool& pool)
        : db_(db), pool_(pool) {}

    void recordClick(const std::string& shortCode,
                     const std::string& userAgent,
                     const std::string& remoteIP) {
        std::string browser = parseBrowser(userAgent);
        std::string device = parseDevice(userAgent);
        std::string country = resolveCountry(remoteIP);

        pool_.enqueue([this, shortCode, country, browser, device]() {
            try {
                AnalyticsEvent event;
                event.shortCode = shortCode;
                event.country = country;
                event.browser = browser;
                event.device = device;
                
                db_.insertAnalytics(event);
            } catch (const std::exception& e) {
                std::cerr << "[AnalyticsService] Failed to record analytics: " 
                          << e.what() << std::endl;
            }
        });
    }

    AnalyticsResult getAnalytics(const std::string& shortCode) {
        return db_.getAnalytics(shortCode);
    }

private:
    PostgresPool& db_;
    ThreadPool& pool_;

    static std::string parseBrowser(const std::string& userAgent) {
        if (userAgent.empty()) return "Unknown";

        if (userAgent.find("Edg/") != std::string::npos || 
            userAgent.find("Edge/") != std::string::npos) {
            return "Edge";
        }
        if (userAgent.find("OPR/") != std::string::npos || 
            userAgent.find("Opera") != std::string::npos) {
            return "Opera";
        }
        if (userAgent.find("Chrome/") != std::string::npos) {
            return "Chrome";
        }
        if (userAgent.find("Firefox/") != std::string::npos) {
            return "Firefox";
        }
        if (userAgent.find("Safari/") != std::string::npos) {
            return "Safari";
        }
        if (userAgent.find("MSIE") != std::string::npos || 
            userAgent.find("Trident/") != std::string::npos) {
            return "Internet Explorer";
        }

        return "Other";
    }

    static std::string parseDevice(const std::string& userAgent) {
        if (userAgent.empty()) return "Unknown";

        std::string ua_lower = userAgent;
        std::transform(ua_lower.begin(), ua_lower.end(), ua_lower.begin(), ::tolower);

        if (ua_lower.find("mobile") != std::string::npos ||
            ua_lower.find("android") != std::string::npos ||
            ua_lower.find("iphone") != std::string::npos) {
            return "Mobile";
        }
        if (ua_lower.find("tablet") != std::string::npos ||
            ua_lower.find("ipad") != std::string::npos) {
            return "Tablet";
        }
        if (ua_lower.find("bot") != std::string::npos ||
            ua_lower.find("crawler") != std::string::npos ||
            ua_lower.find("spider") != std::string::npos) {
            return "Bot";
        }

        return "Desktop";
    }

    static std::string resolveCountry(const std::string& remoteIP) {
        if (remoteIP.empty() || remoteIP == "127.0.0.1" || remoteIP == "::1") {
            return "Localhost";
        }
        return "Unknown";
    }
};
