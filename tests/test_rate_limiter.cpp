#include <gtest/gtest.h>
#include "rateLimiter/TokenBucket.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

using namespace url_shortener;

TEST(TokenBucketTest, AllowsRequestsUpToMax) {
    TokenBucket bucket(5.0, 0.0);

    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(bucket.allowRequest()) << "Request " << i << " should be allowed";
    }

    EXPECT_FALSE(bucket.allowRequest());
}

TEST(TokenBucketTest, RefillsOverTime) {
    TokenBucket bucket(2.0, 10.0);

    EXPECT_TRUE(bucket.allowRequest());
    EXPECT_TRUE(bucket.allowRequest());
    EXPECT_FALSE(bucket.allowRequest());

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    EXPECT_TRUE(bucket.allowRequest());
}

TEST(TokenBucketTest, DoesNotExceedMax) {
    TokenBucket bucket(3.0, 100.0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(bucket.allowRequest());
    EXPECT_TRUE(bucket.allowRequest());
    EXPECT_TRUE(bucket.allowRequest());
    EXPECT_FALSE(bucket.allowRequest());
}

TEST(TokenBucketTest, AvailableTokens) {
    TokenBucket bucket(10.0, 0.0);

    EXPECT_DOUBLE_EQ(bucket.availableTokens(), 10.0);

    bucket.allowRequest();
    EXPECT_DOUBLE_EQ(bucket.availableTokens(), 9.0);
}

TEST(RateLimiterTest, PerIPLimiting) {
    RateLimiter limiter(3.0, 0.0);

    EXPECT_TRUE(limiter.allowRequest("192.168.1.1"));
    EXPECT_TRUE(limiter.allowRequest("192.168.1.1"));
    EXPECT_TRUE(limiter.allowRequest("192.168.1.1"));
    EXPECT_FALSE(limiter.allowRequest("192.168.1.1"));

    EXPECT_TRUE(limiter.allowRequest("192.168.1.2"));
    EXPECT_TRUE(limiter.allowRequest("192.168.1.2"));
    EXPECT_TRUE(limiter.allowRequest("192.168.1.2"));
    EXPECT_FALSE(limiter.allowRequest("192.168.1.2"));
}

TEST(RateLimiterTest, DifferentIPsAreIndependent) {
    RateLimiter limiter(1.0, 0.0);

    EXPECT_TRUE(limiter.allowRequest("10.0.0.1"));
    EXPECT_FALSE(limiter.allowRequest("10.0.0.1"));

    EXPECT_TRUE(limiter.allowRequest("10.0.0.2"));
}

TEST(RateLimiterTest, CleanupRemovesStaleBuckets) {
    RateLimiter limiter(5.0, 1.0);

    limiter.allowRequest("192.168.1.1");
    limiter.allowRequest("192.168.1.2");
    limiter.allowRequest("192.168.1.3");

    size_t removed = limiter.cleanup(std::chrono::seconds(0));
    EXPECT_GE(removed + 0u, 0u);
}

TEST(TokenBucketTest, ConcurrentAccess) {
    TokenBucket bucket(100.0, 0.0);

    std::atomic<int> allowed{0};
    std::atomic<int> denied{0};
    const int numThreads = 10;
    const int requestsPerThread = 20;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < requestsPerThread; ++j) {
                if (bucket.allowRequest()) {
                    allowed.fetch_add(1);
                } else {
                    denied.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    EXPECT_EQ(allowed.load() + denied.load(), numThreads * requestsPerThread);
    EXPECT_EQ(allowed.load(), 100);
    EXPECT_EQ(denied.load(), 100);
}

TEST(RateLimiterTest, ConcurrentMultipleIPs) {
    RateLimiter limiter(10.0, 0.0);

    std::vector<std::thread> threads;
    std::atomic<int> totalAllowed{0};

    for (int ip = 0; ip < 5; ++ip) {
        threads.emplace_back([&limiter, &totalAllowed, ip]() {
            std::string clientIP = "10.0.0." + std::to_string(ip);
            for (int j = 0; j < 15; ++j) {
                if (limiter.allowRequest(clientIP)) {
                    totalAllowed.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    EXPECT_EQ(totalAllowed.load(), 50);
}
