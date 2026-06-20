#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace url_shortener {

class ConsistentHash {
public:
    explicit ConsistentHash(int virtualNodes = 150)
        : virtualNodes_{virtualNodes} {
        if (virtualNodes_ <= 0) {
            throw std::invalid_argument(
                "ConsistentHash: virtualNodes must be positive (got " +
                std::to_string(virtualNodes) + ")");
        }
    }

    ConsistentHash(const ConsistentHash&) = delete;
    ConsistentHash& operator=(const ConsistentHash&) = delete;

    void addServer(const std::string& server) {
        std::unique_lock lock(mutex_);

        if (servers_.count(server)) {
            return;
        }

        servers_.insert(server);
        for (int i = 0; i < virtualNodes_; ++i) {
            size_t hash = hashNode(server, i);
            ring_[hash] = server;
        }
    }

    void removeServer(const std::string& server) {
        std::unique_lock lock(mutex_);

        if (!servers_.count(server)) {
            return;
        }

        for (int i = 0; i < virtualNodes_; ++i) {
            size_t hash = hashNode(server, i);
            ring_.erase(hash);
        }
        servers_.erase(server);
    }

    [[nodiscard]] std::string getServer(const std::string& key) const {
        std::shared_lock lock(mutex_);

        if (ring_.empty()) {
            throw std::runtime_error(
                "ConsistentHash::getServer: ring is empty — no servers "
                "have been added");
        }

        size_t hash = hasher_(key);

        auto it = ring_.lower_bound(hash);
        if (it == ring_.end()) {
            it = ring_.begin();
        }

        return it->second;
    }

    [[nodiscard]] size_t serverCount() const {
        std::shared_lock lock(mutex_);
        return servers_.size();
    }

    [[nodiscard]] bool empty() const {
        std::shared_lock lock(mutex_);
        return servers_.empty();
    }

    [[nodiscard]] size_t ringSize() const {
        std::shared_lock lock(mutex_);
        return ring_.size();
    }

    [[nodiscard]] std::vector<std::string> servers() const {
        std::shared_lock lock(mutex_);
        return {servers_.begin(), servers_.end()};
    }

private:
    [[nodiscard]] size_t hashNode(const std::string& server, int index) const {
        std::string virtualKey = server + "#" + std::to_string(index);
        return hasher_(virtualKey);
    }

    std::map<size_t, std::string>  ring_;
    std::unordered_set<std::string> servers_;
    int                             virtualNodes_;

    mutable std::shared_mutex       mutex_;
    std::hash<std::string>          hasher_;
};

} // namespace url_shortener
