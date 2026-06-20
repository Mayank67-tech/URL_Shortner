#pragma once

#include <sw/redis++/redis++.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>

class RedisClient {
public:
    explicit RedisClient(const std::string& host,
                         int port     = 6379,
                         int poolSize = 10);

    ~RedisClient() = default;

    RedisClient(const RedisClient&)            = delete;
    RedisClient& operator=(const RedisClient&) = delete;
    RedisClient(RedisClient&&)                 = default;
    RedisClient& operator=(RedisClient&&)      = default;

    std::optional<std::string> get(const std::string& key);

    void set(const std::string& key,
             const std::string& value,
             std::chrono::seconds ttl = std::chrono::seconds{3600});

    void del(const std::string& key);

    int64_t increment(const std::string& key);

    bool isHealthy();

private:
    std::unique_ptr<sw::redis::Redis> redis_;
};
