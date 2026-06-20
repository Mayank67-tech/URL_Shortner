#include <gtest/gtest.h>
#include "hashing/ConsistentHash.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <thread>
#include <cmath>

using namespace url_shortener;

TEST(ConsistentHashTest, AddServer) {
    ConsistentHash ring(10);

    ring.addServer("server-1");
    EXPECT_EQ(ring.serverCount(), 1u);

    ring.addServer("server-2");
    EXPECT_EQ(ring.serverCount(), 2u);
}

TEST(ConsistentHashTest, RemoveServer) {
    ConsistentHash ring(10);

    ring.addServer("server-1");
    ring.addServer("server-2");

    ring.removeServer("server-1");
    EXPECT_EQ(ring.serverCount(), 1u);

    EXPECT_EQ(ring.getServer("any_key"), "server-2");
}

TEST(ConsistentHashTest, EmptyRingThrows) {
    ConsistentHash ring(10);

    EXPECT_TRUE(ring.empty());
    EXPECT_THROW(ring.getServer("key"), std::runtime_error);
}

TEST(ConsistentHashTest, SingleServer) {
    ConsistentHash ring(10);
    ring.addServer("server-1");

    EXPECT_EQ(ring.getServer("key1"), "server-1");
    EXPECT_EQ(ring.getServer("key2"), "server-1");
    EXPECT_EQ(ring.getServer("key3"), "server-1");
}

TEST(ConsistentHashTest, SameKeyReturnsSameServer) {
    ConsistentHash ring(100);
    ring.addServer("server-1");
    ring.addServer("server-2");
    ring.addServer("server-3");

    std::string server1 = ring.getServer("my_key");
    std::string server2 = ring.getServer("my_key");
    std::string server3 = ring.getServer("my_key");

    EXPECT_EQ(server1, server2);
    EXPECT_EQ(server2, server3);
}

TEST(ConsistentHashTest, DifferentKeysCanMapToDifferentServers) {
    ConsistentHash ring(150);
    ring.addServer("server-1");
    ring.addServer("server-2");
    ring.addServer("server-3");

    std::unordered_map<std::string, int> distribution;
    for (int i = 0; i < 10000; ++i) {
        std::string key = "key_" + std::to_string(i);
        distribution[ring.getServer(key)]++;
    }

    EXPECT_EQ(distribution.size(), 3u);
    for (auto& [server, count] : distribution) {
        EXPECT_GT(count, 0) << server << " got zero keys";
    }
}

TEST(ConsistentHashTest, MinimalRedistributionOnAddServer) {
    ConsistentHash ring(150);
    ring.addServer("server-1");
    ring.addServer("server-2");

    const int numKeys = 10000;
    std::unordered_map<std::string, std::string> initialMapping;
    for (int i = 0; i < numKeys; ++i) {
        std::string key = "url_" + std::to_string(i);
        initialMapping[key] = ring.getServer(key);
    }

    ring.addServer("server-3");

    int changed = 0;
    for (int i = 0; i < numKeys; ++i) {
        std::string key = "url_" + std::to_string(i);
        if (ring.getServer(key) != initialMapping[key]) {
            changed++;
        }
    }

    double changeRatio = static_cast<double>(changed) / numKeys;
    EXPECT_GT(changeRatio, 0.10) << "Too few keys redistributed (" << changeRatio * 100 << "%)";
    EXPECT_LT(changeRatio, 0.60) << "Too many keys redistributed (" << changeRatio * 100 << "%)";
}

TEST(ConsistentHashTest, MinimalRedistributionOnRemoveServer) {
    ConsistentHash ring(150);
    ring.addServer("server-1");
    ring.addServer("server-2");
    ring.addServer("server-3");

    const int numKeys = 10000;
    std::unordered_map<std::string, std::string> initialMapping;
    for (int i = 0; i < numKeys; ++i) {
        std::string key = "url_" + std::to_string(i);
        initialMapping[key] = ring.getServer(key);
    }

    ring.removeServer("server-3");

    int changed = 0;
    for (int i = 0; i < numKeys; ++i) {
        std::string key = "url_" + std::to_string(i);
        if (ring.getServer(key) != initialMapping[key]) {
            changed++;
            EXPECT_EQ(initialMapping[key], "server-3")
                << "Key " << key << " moved from " << initialMapping[key] << " but only server-3 keys should move";
        }
    }

    EXPECT_GT(changed, 0) << "Some keys should have been redistributed";
}

TEST(ConsistentHashTest, ReasonablyEvenDistribution) {
    ConsistentHash ring(150);
    ring.addServer("server-1");
    ring.addServer("server-2");
    ring.addServer("server-3");

    std::unordered_map<std::string, int> distribution;
    const int numKeys = 30000;

    for (int i = 0; i < numKeys; ++i) {
        distribution[ring.getServer("key_" + std::to_string(i))]++;
    }

    double expected = numKeys / 3.0;
    for (auto& [server, count] : distribution) {
        double deviation = std::abs(count - expected) / expected;
        EXPECT_LT(deviation, 0.5)
            << server << " has " << count << " keys (expected ~" << expected
            << ", deviation: " << deviation * 100 << "%)";
    }
}

TEST(ConsistentHashTest, ConcurrentReads) {
    ConsistentHash ring(150);
    ring.addServer("server-1");
    ring.addServer("server-2");
    ring.addServer("server-3");

    const int numThreads = 8;
    const int readsPerThread = 1000;
    std::atomic<int> successCount{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&ring, &successCount, readsPerThread]() {
            for (int i = 0; i < readsPerThread; ++i) {
                std::string server = ring.getServer("key_" + std::to_string(i));
                if (!server.empty()) {
                    successCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    EXPECT_EQ(successCount.load(), numThreads * readsPerThread);
}

TEST(ConsistentHashTest, ConcurrentReadWriteMix) {
    ConsistentHash ring(50);
    ring.addServer("server-1");
    ring.addServer("server-2");

    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&ring]() {
            for (int i = 0; i < 500; ++i) {
                try {
                    ring.getServer("key_" + std::to_string(i));
                } catch (...) {
                }
            }
        });
    }

    threads.emplace_back([&ring]() {
        for (int i = 3; i < 10; ++i) {
            ring.addServer("server-" + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    threads.emplace_back([&ring]() {
        for (int i = 3; i < 8; ++i) {
            ring.removeServer("server-" + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });

    for (auto& t : threads) t.join();

    EXPECT_GE(ring.serverCount(), 1u);
}
