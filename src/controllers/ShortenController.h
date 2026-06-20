#pragma once

#include "crow.h"
#include "../services/UrlService.h"
#include "../rateLimiter/TokenBucket.h"
#include "../metrics/Metrics.h"
#include <chrono>
#include <iostream>

using namespace url_shortener;

namespace ShortenController {

    template<typename App>
    inline void registerRoutes(App& app, 
                                UrlService& urlService, 
                                RateLimiter& rateLimiter) {
        
        CROW_ROUTE(app, "/shorten").methods(crow::HTTPMethod::POST)
        ([&urlService, &rateLimiter](const crow::request& req) {
            auto start = std::chrono::steady_clock::now();
            
            std::string clientIP = req.remote_ip_address;
            if (!rateLimiter.allowRequest(clientIP)) {
                Metrics::instance().recordRequest("POST", "/shorten", "429");
                return crow::response(429, 
                    crow::json::wvalue({
                        {"error", "Rate limit exceeded. Try again later."},
                        {"retryAfter", 60}
                    }).dump());
            }

            auto body = crow::json::load(req.body);
            if (!body) {
                Metrics::instance().recordRequest("POST", "/shorten", "400");
                return crow::response(400, 
                    crow::json::wvalue({
                        {"error", "Invalid JSON body"}
                    }).dump());
            }

            if (!body.has("url")) {
                Metrics::instance().recordRequest("POST", "/shorten", "400");
                return crow::response(400, 
                    crow::json::wvalue({
                        {"error", "Missing 'url' field"}
                    }).dump());
            }

            std::string longUrl = body["url"].s();
            
            if (longUrl.empty() || 
                (longUrl.substr(0, 7) != "http://" && longUrl.substr(0, 8) != "https://")) {
                Metrics::instance().recordRequest("POST", "/shorten", "400");
                return crow::response(400, 
                    crow::json::wvalue({
                        {"error", "Invalid URL. Must start with http:// or https://"}
                    }).dump());
            }

            try {
                std::string shortCode = urlService.shorten(longUrl);

                crow::json::wvalue response;
                response["shortCode"] = shortCode;
                response["shortUrl"] = "http://localhost:8080/r/" + shortCode;
                response["originalUrl"] = longUrl;

                auto end = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(end - start).count();
                Metrics::instance().recordRequest("POST", "/shorten", "201");
                Metrics::instance().recordLatency("POST", "/shorten", elapsed);

                auto resp = crow::response(201, response.dump());
                resp.set_header("Content-Type", "application/json");
                return resp;

            } catch (const std::exception& e) {
                std::cerr << "[ShortenController] Error: " << e.what() << std::endl;
                Metrics::instance().recordRequest("POST", "/shorten", "500");
                return crow::response(500, 
                    crow::json::wvalue({
                        {"error", "Internal server error"},
                        {"details", e.what()}
                    }).dump());
            }
        });
    }

}
