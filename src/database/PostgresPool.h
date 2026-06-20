#pragma once

#include <pqxx/pqxx>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>

#include "../models/Url.h"

class PostgresPool {
public:
    explicit PostgresPool(const std::string& connStr, size_t poolSize = 10);
    ~PostgresPool();

    PostgresPool(const PostgresPool&)            = delete;
    PostgresPool& operator=(const PostgresPool&) = delete;
    PostgresPool(PostgresPool&&)                 = delete;
    PostgresPool& operator=(PostgresPool&&)      = delete;

    class ConnectionGuard {
    public:
        ConnectionGuard(PostgresPool& pool,
                        std::shared_ptr<pqxx::connection> conn);
        ~ConnectionGuard();

        ConnectionGuard(const ConnectionGuard&)            = delete;
        ConnectionGuard& operator=(const ConnectionGuard&) = delete;
        ConnectionGuard(ConnectionGuard&& other) noexcept;
        ConnectionGuard& operator=(ConnectionGuard&& other) noexcept;

        pqxx::connection& operator*();
        pqxx::connection* operator->();

    private:
        PostgresPool*                    pool_;
        std::shared_ptr<pqxx::connection> conn_;
    };

    [[nodiscard]] ConnectionGuard acquire();

    int64_t insertUrl(const std::string& longUrl);

    bool updateShortCode(int64_t id, const std::string& shortCode);

    std::optional<UrlRecord> findByShortCode(const std::string& code);

    bool deleteByShortCode(const std::string& code);

    void insertAnalytics(const AnalyticsEvent& event);

    AnalyticsResult getAnalytics(const std::string& shortCode);

    bool isHealthy();

private:
    void release(std::shared_ptr<pqxx::connection> conn);

    std::string                                   connStr_;
    std::queue<std::shared_ptr<pqxx::connection>>  pool_;
    std::mutex                                     mtx_;
    std::condition_variable                        cv_;
};
