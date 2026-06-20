#pragma once

#include <atomic>
#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace url_shortener {

template <typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(size_t capacity = 10000)
        : capacity_{capacity}
        , hits_{0}
        , misses_{0} {
        if (capacity_ == 0) {
            throw std::invalid_argument(
                "LRUCache: capacity must be at least 1");
        }
        map_.reserve(capacity_);
    }

    LRUCache(const LRUCache&) = delete;
    LRUCache& operator=(const LRUCache&) = delete;
    LRUCache(LRUCache&&) = delete;
    LRUCache& operator=(LRUCache&&) = delete;

    ~LRUCache() = default;

    [[nodiscard]] std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = map_.find(key);
        if (it == map_.end()) {
            misses_.fetch_add(1, std::memory_order_relaxed);
            return std::nullopt;
        }

        order_.splice(order_.begin(), order_, it->second);
        hits_.fetch_add(1, std::memory_order_relaxed);
        return it->second->second;
    }

    void put(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = value;
            order_.splice(order_.begin(), order_, it->second);
            return;
        }

        if (map_.size() >= capacity_) {
            auto& lru = order_.back();
            map_.erase(lru.first);
            order_.pop_back();
        }

        order_.emplace_front(key, value);
        map_[key] = order_.begin();
    }

    bool remove(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = map_.find(key);
        if (it == map_.end()) {
            return false;
        }

        order_.erase(it->second);
        map_.erase(it);
        return true;
    }

    [[nodiscard]] size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        map_.clear();
        order_.clear();
    }

    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }

    [[nodiscard]] uint64_t hitCount() const noexcept {
        return hits_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] uint64_t missCount() const noexcept {
        return misses_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] double hitRate() const noexcept {
        const uint64_t h = hits_.load(std::memory_order_relaxed);
        const uint64_t m = misses_.load(std::memory_order_relaxed);
        const uint64_t total = h + m;
        return (total == 0) ? 0.0 : static_cast<double>(h) / static_cast<double>(total);
    }

    void resetCounters() noexcept {
        hits_.store(0, std::memory_order_relaxed);
        misses_.store(0, std::memory_order_relaxed);
    }

private:
    using ListType = std::list<std::pair<K, V>>;
    using ListIterator = typename ListType::iterator;

    std::unordered_map<K, ListIterator> map_;
    ListType                            order_;
    const size_t                        capacity_;

    mutable std::mutex mutex_;

    std::atomic<uint64_t> hits_;
    std::atomic<uint64_t> misses_;
};

} // namespace url_shortener
