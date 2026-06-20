#pragma once

#include "crow.h"
#include "../services/AnalyticsService.h"
#include "../rateLimiter/TokenBucket.h"
#include "../metrics/Metrics.h"
#include <chrono>
#include <iostream>

using namespace url_shortener;

namespace AnalyticsController {

    template<typename App>
    inline void registerRoutes(App& app,
                                AnalyticsService& analyticsService,
                                RateLimiter& rateLimiter) {

        CROW_ROUTE(app, "/analytics/<string>")
        ([&analyticsService, &rateLimiter](const crow::request& req, std::string shortCode) {
            auto start = std::chrono::steady_clock::now();

            std::string clientIP = req.remote_ip_address;
            if (!rateLimiter.allowRequest(clientIP)) {
                Metrics::instance().recordRequest("GET", "/analytics", "429");
                return crow::response(429,
                    crow::json::wvalue({
                        {"error", "Rate limit exceeded. Try again later."}
                    }).dump());
            }

            try {
                auto result = analyticsService.getAnalytics(shortCode);

                crow::json::wvalue response;
                response["shortCode"] = shortCode;
                response["clicks"] = result.clicks;
                response["lastAccess"] = result.lastAccess;

                std::vector<crow::json::wvalue> countries;
                for (const auto& country : result.topCountries) {
                    countries.push_back(crow::json::wvalue(country));
                }
                response["topCountries"] = std::move(countries);

                auto end = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(end - start).count();
                Metrics::instance().recordRequest("GET", "/analytics", "200");
                Metrics::instance().recordLatency("GET", "/analytics", elapsed);

                auto resp = crow::response(200, response.dump());
                resp.set_header("Content-Type", "application/json");
                return resp;

            } catch (const std::exception& e) {
                std::cerr << "[AnalyticsController] Error: " << e.what() << std::endl;
                Metrics::instance().recordRequest("GET", "/analytics", "500");
                return crow::response(500,
                    crow::json::wvalue({
                        {"error", "Internal server error"},
                        {"details", e.what()}
                    }).dump());
            }
        });
    }

}
