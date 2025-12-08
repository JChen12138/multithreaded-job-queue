#pragma once
#include <unordered_map>
#include <list>
#include <mutex>

template <typename Key, typename Value>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}

    // Get value by key. Returns true if found.
    bool get(const Key& key, Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_items_map_.find(key);
        if (it == cache_items_map_.end()) return false;

        // Move accessed item to front
        cache_items_list_.splice(cache_items_list_.begin(), cache_items_list_, it->second);
        value = it->second->second;
        return true;
    }

    // Insert or update
    void put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_items_map_.find(key);
        if (it != cache_items_map_.end()) {
            // Update and move to front
            it->second->second = value;
            cache_items_list_.splice(cache_items_list_.begin(), cache_items_list_, it->second);
            return;
        }

        // Evict if needed
        if (cache_items_list_.size() == capacity_) {
            auto last = cache_items_list_.end();
            --last;
            cache_items_map_.erase(last->first);
            cache_items_list_.pop_back();
        }

        cache_items_list_.emplace_front(key, value);
        cache_items_map_[key] = cache_items_list_.begin();
    }

    // Optional: check if key exists
    bool exists(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_items_map_.count(key) > 0;
    }

private:
    std::list<std::pair<Key, Value>> cache_items_list_;
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> cache_items_map_;
    size_t capacity_;
    std::mutex mutex_;
};