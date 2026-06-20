#include "crow.h"
#include "crow/middlewares/cors.h"

#include "database/PostgresPool.h"
#include "redis/RedisClient.h"
#include "cache/LRUCache.h"
#include "rateLimiter/TokenBucket.h"
#include "threadpool/ThreadPool.h"
#include "hashing/ConsistentHash.h"
#include "metrics/Metrics.h"
#include "services/UrlService.h"
#include "services/AnalyticsService.h"
#include "controllers/ShortenController.h"
#include "controllers/RedirectController.h"
#include "controllers/AnalyticsController.h"
#include "controllers/HealthController.h"

#include <iostream>
#include <string>
#include <cstdlib>
#include <csignal>

using namespace url_shortener;

static std::string getEnv(const std::string& key, const std::string& defaultValue = "") {
    const char* val = std::getenv(key.c_str());
    return val ? std::string(val) : defaultValue;
}

static int getEnvInt(const std::string& key, int defaultValue) {
    const char* val = std::getenv(key.c_str());
    return val ? std::stoi(val) : defaultValue;
}

int main() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════╗
║        Thread-Safe Distributed URL Shortener            ║
║                     v1.0.0                              ║
╚══════════════════════════════════════════════════════════╝
)" << std::endl;

    std::string dbHost     = getEnv("DB_HOST", "localhost");
    int         dbPort     = getEnvInt("DB_PORT", 5432);
    std::string dbName     = getEnv("DB_NAME", "url_shortener");
    std::string dbUser     = getEnv("DB_USER", "urlapp");
    std::string dbPassword = getEnv("DB_PASSWORD", "secretpass123");
    std::string redisHost  = getEnv("REDIS_HOST", "localhost");
    int         redisPort  = getEnvInt("REDIS_PORT", 6379);
    int         serverPort = getEnvInt("SERVER_PORT", 8080);
    int         metricsPort= getEnvInt("METRICS_PORT", 9090);
    int         cacheSize  = getEnvInt("CACHE_SIZE", 10000);
    int         dbPoolSize = getEnvInt("POOL_SIZE", 10);
    int         threadPoolSize = getEnvInt("THREAD_POOL_SIZE", 4);

    std::string connStr = "host=" + dbHost + 
                           " port=" + std::to_string(dbPort) +
                           " dbname=" + dbName + 
                           " user=" + dbUser +
                           " password=" + dbPassword;

    std::cout << "[Config] Database:    " << dbHost << ":" << dbPort << "/" << dbName << std::endl;
    std::cout << "[Config] Redis:       " << redisHost << ":" << redisPort << std::endl;
    std::cout << "[Config] Server Port: " << serverPort << std::endl;
    std::cout << "[Config] Metrics Port:" << metricsPort << std::endl;
    std::cout << "[Config] Cache Size:  " << cacheSize << std::endl;
    std::cout << "[Config] DB Pool:     " << dbPoolSize << std::endl;
    std::cout << "[Config] Thread Pool: " << threadPoolSize << std::endl;
    std::cout << std::endl;

    Metrics::instance().init("0.0.0.0:" + std::to_string(metricsPort));

    std::cout << "[Init] Starting Prometheus metrics on port " << metricsPort << "..." << std::endl;
    PostgresPool db(connStr, dbPoolSize);

    std::cout << "[Init] Connecting to PostgreSQL..." << std::endl;
    RedisClient redis(redisHost, redisPort);

    std::cout << "[Init] Connecting to Redis..." << std::endl;
    LRUCache<std::string, std::string> cache(cacheSize);

    std::cout << "[Init] Initializing LRU Cache (capacity: " << cacheSize << ")..." << std::endl;
    RateLimiter rateLimiter(100.0, 100.0 / 60.0);

    std::cout << "[Init] Setting up Rate Limiter (100 req/min per IP)..." << std::endl;
    ThreadPool analyticsPool(threadPoolSize);

    std::cout << "[Init] Starting Thread Pool (" << threadPoolSize << " workers)..." << std::endl;
    std::cout << "[Init] Setting up Consistent Hash Ring..." << std::endl;
    ConsistentHash hashRing(150);
    hashRing.addServer("server-1");
    hashRing.addServer("server-2");

    std::cout << "[Init] Wiring services..." << std::endl;
    UrlService urlService(db, redis, cache);
    AnalyticsService analyticsService(db, analyticsPool);

    crow::App<crow::CORSHandler> app;

    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .origin("*")
        .methods("GET"_method, "POST"_method, "DELETE"_method, "OPTIONS"_method)
        .headers("Content-Type", "Authorization");

    CROW_ROUTE(app, "/")
    ([]() {
        crow::json::wvalue response;
        response["service"] = "URL Shortener";
        response["version"] = "1.0.0";
        response["endpoints"]["shorten"] = "POST /shorten";
        response["endpoints"]["redirect"] = "GET /r/{shortCode}";
        response["endpoints"]["analytics"] = "GET /analytics/{shortCode}";
        response["endpoints"]["delete"] = "DELETE /r/{shortCode}";
        response["endpoints"]["health"] = "GET /health";

        auto resp = crow::response(200, response.dump());
        resp.set_header("Content-Type", "application/json");
        return resp;
    });

    std::cout << "[Init] Registering routes..." << std::endl;
    ShortenController::registerRoutes(app, urlService, rateLimiter);
    AnalyticsController::registerRoutes(app, analyticsService, rateLimiter);
    HealthController::registerRoutes(app, urlService, db, redis, rateLimiter);
    RedirectController::registerRoutes(app, urlService, analyticsService, rateLimiter);

    std::cout << std::endl;
    std::cout << "╔══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  Server starting on port " << serverPort << "                          ║" << std::endl;
    std::cout << "║  Metrics at http://0.0.0.0:" << metricsPort << "/metrics               ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════╝" << std::endl;

    app.port(serverPort)
       .multithreaded()
       .run();

    std::cout << "[Shutdown] Server stopped." << std::endl;
    return 0;
}
