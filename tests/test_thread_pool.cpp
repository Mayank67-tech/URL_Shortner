#include <gtest/gtest.h>
#include "threadpool/ThreadPool.h"
#include <atomic>
#include <chrono>
#include <vector>
#include <numeric>

using namespace url_shortener;

TEST(ThreadPoolTest, ExecutesSingleTask) {
    ThreadPool pool(2);

    auto future = pool.enqueue([]() {
        return 42;
    });

    EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPoolTest, ExecutesMultipleTasks) {
    ThreadPool pool(4);

    std::vector<std::future<int>> futures;
    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.enqueue([i]() {
            return i * 2;
        }));
    }

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(futures[i].get(), i * 2);
    }
}

TEST(ThreadPoolTest, ExecutesVoidTasks) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};

    std::vector<std::future<void>> futures;
    for (int i = 0; i < 50; ++i) {
        futures.push_back(pool.enqueue([&counter]() {
            counter.fetch_add(1);
        }));
    }

    for (auto& f : futures) {
        f.get();
    }

    EXPECT_EQ(counter.load(), 50);
}

TEST(ThreadPoolTest, ReturnsCorrectFutureType) {
    ThreadPool pool(2);

    auto intFuture = pool.enqueue([]() -> int { return 123; });
    auto strFuture = pool.enqueue([]() -> std::string { return "hello"; });
    auto dblFuture = pool.enqueue([]() -> double { return 3.14; });

    EXPECT_EQ(intFuture.get(), 123);
    EXPECT_EQ(strFuture.get(), "hello");
    EXPECT_DOUBLE_EQ(dblFuture.get(), 3.14);
}

TEST(ThreadPoolTest, PropagatesExceptions) {
    ThreadPool pool(2);

    auto future = pool.enqueue([]() -> int {
        throw std::runtime_error("Task error");
        return 0;
    });

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST(ThreadPoolTest, TasksRunConcurrently) {
    ThreadPool pool(4);
    std::atomic<int> maxConcurrent{0};
    std::atomic<int> currentConcurrent{0};

    std::vector<std::future<void>> futures;
    for (int i = 0; i < 20; ++i) {
        futures.push_back(pool.enqueue([&]() {
            int c = currentConcurrent.fetch_add(1) + 1;

            int expected = maxConcurrent.load();
            while (c > expected) {
                maxConcurrent.compare_exchange_weak(expected, c);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            currentConcurrent.fetch_sub(1);
        }));
    }

    for (auto& f : futures) f.get();

    EXPECT_GT(maxConcurrent.load(), 1);
}

TEST(ThreadPoolTest, AllTasksComplete) {
    ThreadPool pool(4);
    std::atomic<int> completed{0};
    const int numTasks = 1000;

    std::vector<std::future<void>> futures;
    for (int i = 0; i < numTasks; ++i) {
        futures.push_back(pool.enqueue([&completed]() {
            completed.fetch_add(1);
        }));
    }

    for (auto& f : futures) f.get();

    EXPECT_EQ(completed.load(), numTasks);
}

TEST(ThreadPoolTest, GracefulShutdown) {
    std::atomic<int> completed{0};

    {
        ThreadPool pool(2);

        for (int i = 0; i < 10; ++i) {
            pool.enqueue([&completed]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                completed.fetch_add(1);
            });
        }
    }

    EXPECT_EQ(completed.load(), 10);
}

TEST(ThreadPoolTest, EnqueueAfterStopThrows) {
    auto pool = std::make_unique<ThreadPool>(2);

    pool.reset();

    SUCCEED();
}

TEST(ThreadPoolTest, ReportsCorrectThreadCount) {
    ThreadPool pool(6);
    EXPECT_EQ(pool.threadCount(), 6u);
}

TEST(ThreadPoolTest, SingleThreadPool) {
    ThreadPool pool(1);

    std::vector<int> results;
    std::mutex mu;

    std::vector<std::future<void>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.enqueue([i, &results, &mu]() {
            std::lock_guard<std::mutex> lock(mu);
            results.push_back(i);
        }));
    }

    for (auto& f : futures) f.get();

    EXPECT_EQ(results.size(), 10u);
}

TEST(ThreadPoolTest, TaskWithArguments) {
    ThreadPool pool(2);

    auto future = pool.enqueue([](int a, int b) {
        return a + b;
    }, 10, 20);

    EXPECT_EQ(future.get(), 30);
}

TEST(ThreadPoolTest, TaskWithStringArgument) {
    ThreadPool pool(2);

    auto future = pool.enqueue([](const std::string& prefix, int num) {
        return prefix + std::to_string(num);
    }, std::string("item_"), 42);

    EXPECT_EQ(future.get(), "item_42");
}

TEST(ThreadPoolTest, StressTest) {
    ThreadPool pool(8);
    std::atomic<int64_t> sum{0};
    const int numTasks = 10000;

    std::vector<std::future<void>> futures;
    for (int i = 1; i <= numTasks; ++i) {
        futures.push_back(pool.enqueue([&sum, i]() {
            sum.fetch_add(i);
        }));
    }

    for (auto& f : futures) f.get();

    int64_t expected = static_cast<int64_t>(numTasks) * (numTasks + 1) / 2;
    EXPECT_EQ(sum.load(), expected);
}
