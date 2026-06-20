#include "PostgresPool.h"

#include <iostream>
#include <stdexcept>
#include <utility>

PostgresPool::PostgresPool(const std::string& connStr, size_t poolSize)
    : connStr_{connStr}
{
    if (poolSize == 0) {
        throw std::invalid_argument("PostgresPool: poolSize must be > 0");
    }

    for (size_t i = 0; i < poolSize; ++i) {
        auto conn = std::make_shared<pqxx::connection>(connStr_);
        if (!conn->is_open()) {
            throw std::runtime_error(
                "PostgresPool: failed to open connection #" + std::to_string(i));
        }
        pool_.push(std::move(conn));
    }
}

PostgresPool::~PostgresPool()
{
    std::lock_guard<std::mutex> lock{mtx_};
    while (!pool_.empty()) {
        pool_.pop();
    }
}

PostgresPool::ConnectionGuard::ConnectionGuard(
        PostgresPool& pool,
        std::shared_ptr<pqxx::connection> conn)
    : pool_{&pool}
    , conn_{std::move(conn)}
{}

PostgresPool::ConnectionGuard::~ConnectionGuard()
{
    if (pool_ && conn_) {
        pool_->release(std::move(conn_));
    }
}

PostgresPool::ConnectionGuard::ConnectionGuard(ConnectionGuard&& other) noexcept
    : pool_{other.pool_}
    , conn_{std::move(other.conn_)}
{
    other.pool_ = nullptr;
}

PostgresPool::ConnectionGuard&
PostgresPool::ConnectionGuard::operator=(ConnectionGuard&& other) noexcept
{
    if (this != &other) {
        if (pool_ && conn_) {
            pool_->release(std::move(conn_));
        }
        pool_       = other.pool_;
        conn_       = std::move(other.conn_);
        other.pool_ = nullptr;
    }
    return *this;
}

pqxx::connection& PostgresPool::ConnectionGuard::operator*()
{
    return *conn_;
}

pqxx::connection* PostgresPool::ConnectionGuard::operator->()
{
    return conn_.get();
}

PostgresPool::ConnectionGuard PostgresPool::acquire()
{
    std::unique_lock<std::mutex> lock{mtx_};
    cv_.wait(lock, [this] { return !pool_.empty(); });

    auto conn = pool_.front();
    pool_.pop();

    if (!conn->is_open()) {
        try {
            conn = std::make_shared<pqxx::connection>(connStr_);
        } catch (const std::exception& e) {
            std::cerr << "[PostgresPool] reconnect failed: " << e.what() << '\n';
            throw;
        }
    }

    return ConnectionGuard{*this, std::move(conn)};
}

void PostgresPool::release(std::shared_ptr<pqxx::connection> conn)
{
    {
        std::lock_guard<std::mutex> lock{mtx_};
        pool_.push(std::move(conn));
    }
    cv_.notify_one();
}

int64_t PostgresPool::insertUrl(const std::string& longUrl)
{
    try {
        auto guard = acquire();
        pqxx::work txn{*guard};

        const auto row = txn.exec_params1(
            "INSERT INTO urls (long_url) VALUES ($1) RETURNING id",
            longUrl);

        txn.commit();
        return row[0].as<int64_t>();

    } catch (const std::exception& e) {
        std::cerr << "[PostgresPool] insertUrl error: " << e.what() << '\n';
        return 0;
    }
}

bool PostgresPool::updateShortCode(int64_t id, const std::string& shortCode)
{
    try {
        auto guard = acquire();
        pqxx::work txn{*guard};

        const auto result = txn.exec_params(
            "UPDATE urls SET short_code = $1 WHERE id = $2",
            shortCode, id);

        txn.commit();
        return result.affected_rows() > 0;

    } catch (const std::exception& e) {
        std::cerr << "[PostgresPool] updateShortCode error: "
                  << e.what() << '\n';
        return false;
    }
}

std::optional<UrlRecord> PostgresPool::findByShortCode(const std::string& code)
{
    try {
        auto guard = acquire();
        pqxx::nontransaction ntx{*guard};

        const auto result = ntx.exec_params(
            "SELECT id, short_code, long_url, created_at "
            "FROM urls WHERE short_code = $1",
            code);

        if (result.empty()) {
            return std::nullopt;
        }

        const auto& row = result[0];
        UrlRecord rec;
        rec.id        = row["id"].as<int64_t>();
        rec.shortCode = row["short_code"].as<std::string>();
        rec.longUrl   = row["long_url"].as<std::string>();
        rec.createdAt = row["created_at"].as<std::string>();
        return rec;

    } catch (const std::exception& e) {
        std::cerr << "[PostgresPool] findByShortCode error: "
                  << e.what() << '\n';
        return std::nullopt;
    }
}

bool PostgresPool::deleteByShortCode(const std::string& code)
{
    try {
        auto guard = acquire();
        pqxx::work txn{*guard};

        const auto result = txn.exec_params(
            "DELETE FROM urls WHERE short_code = $1",
            code);

        txn.commit();
        return result.affected_rows() > 0;

    } catch (const std::exception& e) {
        std::cerr << "[PostgresPool] deleteByShortCode error: "
                  << e.what() << '\n';
        return false;
    }
}

void PostgresPool::insertAnalytics(const AnalyticsEvent& event)
{
    try {
        auto guard = acquire();
        pqxx::work txn{*guard};

        txn.exec_params(
            "INSERT INTO analytics "
            "(short_code, accessed_at, country, browser, device) "
            "VALUES ($1, NOW(), $2, $3, $4)",
            event.shortCode,
            event.country,
            event.browser,
            event.device);

        txn.commit();

    } catch (const std::exception& e) {
        std::cerr << "[PostgresPool] insertAnalytics error: "
                  << e.what() << '\n';
    }
}

AnalyticsResult PostgresPool::getAnalytics(const std::string& shortCode)
{
    AnalyticsResult result;

    try {
        auto guard = acquire();
        pqxx::nontransaction ntx{*guard};

        {
            const auto rows = ntx.exec_params(
                "SELECT COUNT(*) FROM analytics WHERE short_code = $1",
                shortCode);
            if (!rows.empty()) {
                result.clicks = rows[0][0].as<int64_t>();
            }
        }

        {
            const auto rows = ntx.exec_params(
                "SELECT country, COUNT(*) AS cnt "
                "FROM analytics WHERE short_code = $1 "
                "GROUP BY country ORDER BY cnt DESC LIMIT 5",
                shortCode);

            result.topCountries.reserve(
                static_cast<size_t>(rows.size()));
            for (const auto& row : rows) {
                result.topCountries.push_back(
                    row["country"].as<std::string>());
            }
        }

        {
            const auto rows = ntx.exec_params(
                "SELECT MAX(accessed_at) FROM analytics "
                "WHERE short_code = $1",
                shortCode);
            if (!rows.empty() && !rows[0][0].is_null()) {
                result.lastAccess = rows[0][0].as<std::string>();
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "[PostgresPool] getAnalytics error: "
                  << e.what() << '\n';
    }

    return result;
}

bool PostgresPool::isHealthy()
{
    try {
        auto guard = acquire();
        pqxx::nontransaction ntx{*guard};
        const auto result = ntx.exec("SELECT 1");
        return !result.empty();
    } catch (const std::exception& e) {
        std::cerr << "[PostgresPool] health check failed: "
                  << e.what() << '\n';
        return false;
    }
}
