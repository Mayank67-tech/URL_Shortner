#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace url_shortener {

class ThreadPool {
public:
    explicit ThreadPool(
        size_t numThreads = std::thread::hardware_concurrency())
    {
        if (numThreads == 0) {
            numThreads = 1;
        }

        workers_.reserve(numThreads);

        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    template <typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<ReturnType> future = task->get_future();

        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (stop_) {
                throw std::runtime_error(
                    "ThreadPool::enqueue: cannot enqueue on a stopped pool");
            }

            tasks_.emplace([task = std::move(task)]() { (*task)(); });
        }

        cv_.notify_one();
        return future;
    }

    [[nodiscard]] size_t threadCount() const noexcept {
        return workers_.size();
    }

    [[nodiscard]] size_t pendingTasks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }

private:
    void workerLoop() {
        for (;;) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(mutex_);

                cv_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });

                if (stop_ && tasks_.empty()) {
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }

            task();
        }
    }

    std::vector<std::thread>            workers_;
    std::queue<std::function<void()>>   tasks_;

    mutable std::mutex                  mutex_;
    std::condition_variable             cv_;
    bool                                stop_ = false;
};

} // namespace url_shortener
