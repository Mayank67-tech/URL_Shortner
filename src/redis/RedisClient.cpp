#include "RedisClient.h"

#include <iostream>

RedisClient::RedisClient(const std::string& host, int port, int poolSize)
{
    sw::redis::ConnectionOptions connOpts;
    connOpts.host = host;
    connOpts.port = port;
    connOpts.socket_timeout = std::chrono::milliseconds{200};
    connOpts.connect_timeout = std::chrono::milliseconds{200};

    sw::redis::ConnectionPoolOptions poolOpts;
    poolOpts.size = static_cast<std::size_t>(poolSize);
    poolOpts.wait_timeout  = std::chrono::milliseconds{100};
    poolOpts.connection_lifetime = std::chrono::minutes{10};

    redis_ = std::make_unique<sw::redis::Redis>(connOpts, poolOpts);
}

std::optional<std::string> RedisClient::get(const std::string& key)
{
    try {
        return redis_->get(key);
    } catch (const sw::redis::Error& e) {
        std::cerr << "[RedisClient] GET error for key \""
                  << key << "\": " << e.what() << '\n';
        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "[RedisClient] GET unexpected error: "
                  << e.what() << '\n';
        return std::nullopt;
    }
}

void RedisClient::set(const std::string& key,
                      const std::string& value,
                      std::chrono::seconds ttl)
{
    try {
        redis_->setex(key, ttl, value);
    } catch (const sw::redis::Error& e) {
        std::cerr << "[RedisClient] SETEX error for key \""
                  << key << "\": " << e.what() << '\n';
    } catch (const std::exception& e) {
        std::cerr << "[RedisClient] SETEX unexpected error: "
                  << e.what() << '\n';
    }
}

void RedisClient::del(const std::string& key)
{
    try {
        redis_->del(key);
    } catch (const sw::redis::Error& e) {
        std::cerr << "[RedisClient] DEL error for key \""
                  << key << "\": " << e.what() << '\n';
    } catch (const std::exception& e) {
        std::cerr << "[RedisClient] DEL unexpected error: "
                  << e.what() << '\n';
    }
}

int64_t RedisClient::increment(const std::string& key)
{
    try {
        return static_cast<int64_t>(redis_->incr(key));
    } catch (const sw::redis::Error& e) {
        std::cerr << "[RedisClient] INCR error for key \""
                  << key << "\": " << e.what() << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[RedisClient] INCR unexpected error: "
                  << e.what() << '\n';
        return 0;
    }
}

bool RedisClient::isHealthy()
{
    try {
        const auto reply = redis_->ping();
        return reply == "PONG";
    } catch (const sw::redis::Error& e) {
        std::cerr << "[RedisClient] PING error: " << e.what() << '\n';
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[RedisClient] PING unexpected error: "
                  << e.what() << '\n';
        return false;
    }
}
