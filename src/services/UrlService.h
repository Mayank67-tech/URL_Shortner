#pragma once

#include <string>
#include <optional>
#include <memory>
#include <iostream>
#include <chrono>

#include "../cache/LRUCache.h"
#include "../redis/RedisClient.h"
#include "../database/PostgresPool.h"
#include "../hashing/Base62.h"
#include "../metrics/Metrics.h"
#include "../models/Url.h"

using namespace url_shortener;

class UrlService {
public:
    UrlService(PostgresPool& db, RedisClient& redis, LRUCache<std::string, std::string>& cache)
        : db_(db), redis_(redis), cache_(cache) {}

    std::string shorten(const std::string& longUrl) {
        int64_t id = db_.insertUrl(longUrl);

        std::string shortCode = Base62::encode(id);

        if (!db_.updateShortCode(id, shortCode)) {
            throw std::runtime_error("Failed to update short code for id: " + std::to_string(id));
        }

        redis_.set("url:" + shortCode, longUrl);
        cache_.put(shortCode, longUrl);

        std::cout << "[UrlService] Shortened: " << longUrl << " → " << shortCode << std::endl;
        return shortCode;
    }

    std::optional<std::string> resolve(const std::string& shortCode) {
        auto cached = cache_.get(shortCode);
        if (cached.has_value()) {
            Metrics::instance().recordCacheHit();
            return cached;
        }

        auto redisResult = redis_.get("url:" + shortCode);
        if (redisResult.has_value()) {
            Metrics::instance().recordCacheMiss();
            cache_.put(shortCode, redisResult.value());
            return redisResult;
        }

        Metrics::instance().recordCacheMiss();
        auto record = db_.findByShortCode(shortCode);
        if (record.has_value()) {
            redis_.set("url:" + shortCode, record->longUrl);
            cache_.put(shortCode, record->longUrl);
            return record->longUrl;
        }

        return std::nullopt;
    }

    bool deleteUrl(const std::string& shortCode) {
        bool deleted = db_.deleteByShortCode(shortCode);

        if (deleted) {
            redis_.del("url:" + shortCode);
            cache_.remove(shortCode);
            std::cout << "[UrlService] Deleted: " << shortCode << std::endl;
        }

        return deleted;
    }

private:
    PostgresPool& db_;
    RedisClient& redis_;
    LRUCache<std::string, std::string>& cache_;
};
