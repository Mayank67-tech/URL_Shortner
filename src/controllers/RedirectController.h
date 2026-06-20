#pragma once

#include "crow.h"
#include "../services/UrlService.h"
#include "../services/AnalyticsService.h"
#include "../rateLimiter/TokenBucket.h"
#include "../metrics/Metrics.h"
#include <chrono>
#include <iostream>

using namespace url_shortener;

namespace RedirectController {

    template<typename App>
    inline void registerRoutes(App& app,
                                UrlService& urlService,
                                AnalyticsService& analyticsService,
                                RateLimiter& rateLimiter) {

        CROW_ROUTE(app, "/r/<string>")
        ([&urlService, &analyticsService, &rateLimiter](const crow::request& req, std::string shortCode) {
            auto start = std::chrono::steady_clock::now();

            std::string clientIP = req.remote_ip_address;
            if (!rateLimiter.allowRequest(clientIP)) {
                Metrics::instance().recordRequest("GET", "/redirect", "429");
                return crow::response(429,
                    crow::json::wvalue({
                        {"error", "Rate limit exceeded. Try again later."}
                    }).dump());
            }

            try {
                auto longUrl = urlService.resolve(shortCode);

                if (!longUrl.has_value()) {
                    Metrics::instance().recordRequest("GET", "/redirect", "404");
                    return crow::response(404,
                        crow::json::wvalue({
                            {"error", "Short URL not found"},
                            {"shortCode", shortCode}
                        }).dump());
                }

                std::string userAgent = req.get_header_value("User-Agent");
                analyticsService.recordClick(shortCode, userAgent, clientIP);

                auto resp = crow::response(302);
                resp.set_header("Location", longUrl.value());
                resp.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
                resp.set_header("X-Short-Code", shortCode);

                auto end = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(end - start).count();
                Metrics::instance().recordRequest("GET", "/redirect", "302");
                Metrics::instance().recordLatency("GET", "/redirect", elapsed);

                return resp;

            } catch (const std::exception& e) {
                std::cerr << "[RedirectController] Error: " << e.what() << std::endl;
                Metrics::instance().recordRequest("GET", "/redirect", "500");
                return crow::response(500,
                    crow::json::wvalue({
                        {"error", "Internal server error"}
                    }).dump());
            }
        });
    }

}
