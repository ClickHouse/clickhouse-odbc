#pragma once

#include "driver/platform/platform.h"

#include <deque>
#include <utility>

// A pool of at most max_size movable objects, that helps to reuse their capacity but not the content.
// Makes sense to use with std::string's and std::vector's to avoid reallocations of underlying storage.
template<typename T>
class ObjectPool {
public:
    explicit ObjectPool(const std::size_t max_size)
        : max_size_(max_size)
    {
    }

    void put(T && obj) {
        cache_.emplace_back(std::move(obj));
        while (cache_.size() > max_size_) {
            cache_.pop_front();
        }
    }

    T get() {
        if (cache_.empty()) {
            return T{};
        }
        else {
            T obj = std::move(cache_.front());
            cache_.pop_front();
            return obj;
        }
    }

private:
    const std::size_t max_size_;
    std::deque<T> cache_;
};
