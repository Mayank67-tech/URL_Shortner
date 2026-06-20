#include <gtest/gtest.h>
#include "cache/LRUCache.h"
#include <thread>
#include <vector>
#include <atomic>
#include <string>

using namespace url_shortener;

TEST(LRUCacheTest, PutAndGet) {
    LRUCache<std::string, std::string> cache(3);

    cache.put("key1", "value1");
    cache.put("key2", "value2");

    auto v1 = cache.get("key1");
    auto v2 = cache.get("key2");

    ASSERT_TRUE(v1.has_value());
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(v1.value(), "value1");
    EXPECT_EQ(v2.value(), "value2");
}

TEST(LRUCacheTest, GetMissReturnsNullopt) {
    LRUCache<std::string, std::string> cache(3);

    auto result = cache.get("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST(LRUCacheTest, UpdateExistingKey) {
    LRUCache<std::string, std::string> cache(3);

    cache.put("key1", "value1");
    cache.put("key1", "updated_value1");

    auto v = cache.get("key1");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v.value(), "updated_value1");
    EXPECT_EQ(cache.size(), 1u);
}

TEST(LRUCacheTest, EvictsLRUWhenFull) {
    LRUCache<std::string, std::string> cache(3);

    cache.put("key1", "value1");
    cache.put("key2", "value2");
    cache.put("key3", "value3");

    cache.put("key4", "value4");

    EXPECT_FALSE(cache.get("key1").has_value()) << "key1 should be evicted";
    EXPECT_TRUE(cache.get("key2").has_value());
    EXPECT_TRUE(cache.get("key3").has_value());
    EXPECT_TRUE(cache.get("key4").has_value());
}

TEST(LRUCacheTest, AccessPreventEviction) {
    LRUCache<std::string, std::string> cache(3);

    cache.put("key1", "value1");
    cache.put("key2", "value2");
    cache.put("key3", "value3");

    cache.get("key1");

    cache.put("key4", "value4");

    EXPECT_TRUE(cache.get("key1").has_value()) << "key1 should NOT be evicted (recently accessed)";
    EXPECT_FALSE(cache.get("key2").has_value()) << "key2 should be evicted";
    EXPECT_TRUE(cache.get("key3").has_value());
    EXPECT_TRUE(cache.get("key4").has_value());
}

TEST(LRUCacheTest, RemoveExistingKey) {
    LRUCache<std::string, std::string> cache(3);

    cache.put("key1", "value1");
    EXPECT_TRUE(cache.remove("key1"));
    EXPECT_FALSE(cache.get("key1").has_value());
    EXPECT_EQ(cache.size(), 0u);
}

TEST(LRUCacheTest, RemoveNonExistentKey) {
    LRUCache<std::string, std::string> cache(3);
    EXPECT_FALSE(cache.remove("nonexistent"));
}

TEST(LRUCacheTest, SizeTracksCorrectly) {
    LRUCache<std::string, std::string> cache(10);

    EXPECT_EQ(cache.size(), 0u);

    cache.put("k1", "v1");
    EXPECT_EQ(cache.size(), 1u);

    cache.put("k2", "v2");
    EXPECT_EQ(cache.size(), 2u);

    cache.remove("k1");
    EXPECT_EQ(cache.size(), 1u);
}

TEST(LRUCacheTest, ClearEmptiesCache) {
    LRUCache<std::string, std::string> cache(10);

    cache.put("k1", "v1");
    cache.put("k2", "v2");
    cache.put("k3", "v3");

    cache.clear();

    EXPECT_EQ(cache.size(), 0u);
    EXPECT_FALSE(cache.get("k1").has_value());
    EXPECT_FALSE(cache.get("k2").has_value());
    EXPECT_FALSE(cache.get("k3").has_value());
}

TEST(LRUCacheTest, HitMissCounters) {
    LRUCache<std::string, std::string> cache(3);
    cache.resetCounters();

    cache.put("key1", "value1");

    cache.get("key1");
    cache.get("missing");
    cache.get("key1");
    cache.get("also_missing");

    EXPECT_EQ(cache.hitCount(), 2u);
    EXPECT_EQ(cache.missCount(), 2u);
}

TEST(LRUCacheTest, ConcurrentPutAndGet) {
    LRUCache<int, int> cache(1000);
    const int numThreads = 8;
    const int opsPerThread = 1000;

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&cache, t, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                int key = t * opsPerThread + i;
                cache.put(key, key * 10);
            }
        });
    }

    for (auto& t : threads) t.join();
    threads.clear();

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&cache, &successCount, t, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                int key = t * opsPerThread + i;
                auto val = cache.get(key);
                if (val.has_value() && val.value() == key * 10) {
                    successCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    EXPECT_GT(successCount.load(), 0);
}

TEST(LRUCacheTest, ConcurrentMixedOperations) {
    LRUCache<int, int> cache(100);
    const int numThreads = 4;
    const int opsPerThread = 500;

    std::vector<std::thread> threads;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&cache, t, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                int key = (t * opsPerThread + i) % 200;

                if (i % 3 == 0) {
                    cache.put(key, i);
                } else if (i % 3 == 1) {
                    cache.get(key);
                } else {
                    cache.remove(key);
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    EXPECT_LE(cache.size(), 100u);
}

TEST(LRUCacheTest, CapacityOne) {
    LRUCache<std::string, std::string> cache(1);

    cache.put("k1", "v1");
    EXPECT_TRUE(cache.get("k1").has_value());

    cache.put("k2", "v2");
    EXPECT_FALSE(cache.get("k1").has_value());
    EXPECT_TRUE(cache.get("k2").has_value());
}

TEST(LRUCacheTest, IntegerKeys) {
    LRUCache<int, std::string> cache(5);

    cache.put(1, "one");
    cache.put(2, "two");

    EXPECT_EQ(cache.get(1).value(), "one");
    EXPECT_EQ(cache.get(2).value(), "two");
}
