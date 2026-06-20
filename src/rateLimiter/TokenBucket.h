#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <algorithm>

namespace url_shortener {

class TokenBucket {
public:
    explicit TokenBucket(double maxTokens = 100.0, double refillRate = 1.667)
        : tokens_{maxTokens}
        , maxTokens_{maxTokens}
        , refillRate_{refillRate}
        , lastRefill_{std::chrono::steady_clock::now()}
        , lastAccess_{std::chrono::steady_clock::now()} {
        if (maxTokens <= 0.0) {
            throw std::invalid_argument(
                "TokenBucket: maxTokens must be positive");
        }
        if (refillRate < 0.0) {
            throw std::invalid_argument(
                "TokenBucket: refillRate must be non-negative");
        }
    }

    bool allowRequest() {
        std::lock_guard<std::mutex> lock(mutex_);

        refillUnlocked();

        if (tokens_ >= 1.0) {
            tokens_ -= 1.0;
            lastAccess_ = std::chrono::steady_clock::now();
            return true;
        }

        lastAccess_ = std::chrono::steady_clock::now();
        return false;
    }

    [[nodiscard]] double availableTokens() const {
        std::lock_guard<std::mutex> lock(mutex_);

        auto now = std::chrono::steady_clock::now();
        double elapsed =
            std::chrono::duration<double>(now - lastRefill_).count();
        double projected = std::min(tokens_ + elapsed * refillRate_, maxTokens_);
        return projected;
    }

    [[nodiscard]] std::chrono::steady_clock::time_point lastAccessTime() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return lastAccess_;
    }

    [[nodiscard]] double maxTokens()  const noexcept { return maxTokens_;  }
    [[nodiscard]] double refillRate() const noexcept { return refillRate_; }

private:
    void refillUnlocked() {
        auto now = std::chrono::steady_clock::now();
        double elapsed =
            std::chrono::duration<double>(now - lastRefill_).count();

        if (elapsed > 0.0) {
            tokens_ = std::min(tokens_ + elapsed * refillRate_, maxTokens_);
            lastRefill_ = now;
        }
    }

    double                                      tokens_;
    double                                      maxTokens_;
    double                                      refillRate_;
    std::chrono::steady_clock::time_point       lastRefill_;
    std::chrono::steady_clock::time_point       lastAccess_;
    mutable std::mutex                          mutex_;
};

class RateLimiter {
public:
    explicit RateLimiter(double maxTokens = 100.0, double refillRate = 1.667)
        : maxTokens_{maxTokens}
        , refillRate_{refillRate} {
        if (maxTokens <= 0.0) {
            throw std::invalid_argument(
                "RateLimiter: maxTokens must be positive");
        }
        if (refillRate < 0.0) {
            throw std::invalid_argument(
                "RateLimiter: refillRate must be non-negative");
        }
    }

    RateLimiter(const RateLimiter&) = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;
    RateLimiter(RateLimiter&&) = delete;
    RateLimiter& operator=(RateLimiter&&) = delete;

    ~RateLimiter() = default;

    bool allowRequest(const std::string& clientIP) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = buckets_.find(clientIP);
        if (it == buckets_.end()) {
            auto [inserted, _] = buckets_.emplace(
                clientIP,
                std::make_unique<TokenBucket>(maxTokens_, refillRate_));
            return inserted->second->allowRequest();
        }

        return it->second->allowRequest();
    }

    size_t cleanup(std::chrono::seconds maxAge = std::chrono::seconds(3600)) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto now = std::chrono::steady_clock::now();
        size_t removed = 0;

        for (auto it = buckets_.begin(); it != buckets_.end(); ) {
            auto lastAccess = it->second->lastAccessTime();
            if (std::chrono::duration_cast<std::chrono::seconds>(
                    now - lastAccess) >= maxAge) {
                it = buckets_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }

        return removed;
    }

    [[nodiscard]] size_t bucketCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buckets_.size();
    }

private:
    std::unordered_map<std::string, std::unique_ptr<TokenBucket>> buckets_;
    mutable std::mutex mutex_;

    double maxTokens_;
    double refillRate_;
};

} // namespace url_shortener
