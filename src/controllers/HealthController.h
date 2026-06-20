#pragma once

#include "crow.h"
#include "../services/UrlService.h"
#include "../database/PostgresPool.h"
#include "../redis/RedisClient.h"
#include "../metrics/Metrics.h"
#include <chrono>
#include <iostream>

using namespace url_shortener;

namespace HealthController {

    template<typename App>
    inline void registerRoutes(App& app,
                                UrlService& urlService,
                                PostgresPool& db,
                                RedisClient& redis,
                                RateLimiter& rateLimiter) {

        CROW_ROUTE(app, "/health")
        ([&db, &redis](const crow::request& req) {
            auto start = std::chrono::steady_clock::now();

            crow::json::wvalue response;
            bool healthy = true;

            try {
                bool dbOk = db.isHealthy();
                response["database"] = dbOk ? "connected" : "disconnected";
                if (!dbOk) healthy = false;
            } catch (...) {
                response["database"] = "error";
                healthy = false;
            }

            try {
                bool redisOk = redis.isHealthy();
                response["redis"] = redisOk ? "connected" : "disconnected";
                if (!redisOk) healthy = false;
            } catch (...) {
                response["redis"] = "error";
                healthy = false;
            }

            response["status"] = healthy ? "healthy" : "degraded";
            response["service"] = "url-shortener";
            response["version"] = "1.0.0";

            auto end = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(end - start).count();
            response["responseTimeMs"] = static_cast<int>(elapsed * 1000);

            int statusCode = healthy ? 200 : 503;
            std::string statusStr = std::to_string(statusCode);
            Metrics::instance().recordRequest("GET", "/health", statusStr);
            Metrics::instance().recordLatency("GET", "/health", elapsed);

            auto resp = crow::response(statusCode, response.dump());
            resp.set_header("Content-Type", "application/json");
            return resp;
        });

        CROW_ROUTE(app, "/r/<string>").methods(crow::HTTPMethod::DELETE)
        ([&urlService, &rateLimiter](const crow::request& req, std::string shortCode) {
            auto start = std::chrono::steady_clock::now();

            std::string clientIP = req.remote_ip_address;
            if (!rateLimiter.allowRequest(clientIP)) {
                Metrics::instance().recordRequest("DELETE", "/delete", "429");
                return crow::response(429,
                    crow::json::wvalue({
                        {"error", "Rate limit exceeded. Try again later."}
                    }).dump());
            }

            try {
                bool deleted = urlService.deleteUrl(shortCode);

                if (deleted) {
                    auto end = std::chrono::steady_clock::now();
                    double elapsed = std::chrono::duration<double>(end - start).count();
                    Metrics::instance().recordRequest("DELETE", "/delete", "200");
                    Metrics::instance().recordLatency("DELETE", "/delete", elapsed);

                    auto resp = crow::response(200,
                        crow::json::wvalue({
                            {"message", "URL deleted successfully"},
                            {"shortCode", shortCode}
                        }).dump());
                    resp.set_header("Content-Type", "application/json");
                    return resp;
                } else {
                    Metrics::instance().recordRequest("DELETE", "/delete", "404");
                    return crow::response(404,
                        crow::json::wvalue({
                            {"error", "Short URL not found"},
                            {"shortCode", shortCode}
                        }).dump());
                }

            } catch (const std::exception& e) {
                std::cerr << "[HealthController] Delete error: " << e.what() << std::endl;
                Metrics::instance().recordRequest("DELETE", "/delete", "500");
                return crow::response(500,
                    crow::json::wvalue({
                        {"error", "Internal server error"}
                    }).dump());
            }
        });
    }

}
