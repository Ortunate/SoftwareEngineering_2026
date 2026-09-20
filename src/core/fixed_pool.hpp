#pragma once

#include <array>
#include <cstddef>
#include <utility>

namespace redmist {

// A deterministic, allocation-free pool. T must expose a public bool `active`.
template <typename T, std::size_t Capacity>
class FixedPool {
public:
    T* Acquire() {
        for (T& item : items_) {
            if (!item.active) {
                item = T{};
                item.active = true;
                return &item;
            }
        }
        ++droppedSpawns_;
        return nullptr;
    }

    template <typename Function>
    void ForEachActive(Function&& function) {
        for (T& item : items_) {
            if (item.active) {
                std::forward<Function>(function)(item);
            }
        }
    }

    template <typename Function>
    void ForEachActive(Function&& function) const {
        for (const T& item : items_) {
            if (item.active) {
                std::forward<Function>(function)(item);
            }
        }
    }

    void Clear() {
        for (T& item : items_) {
            item.active = false;
        }
        droppedSpawns_ = 0;
    }

    [[nodiscard]] std::size_t ActiveCount() const {
        std::size_t count = 0;
        for (const T& item : items_) {
            count += item.active ? 1U : 0U;
        }
        return count;
    }

    [[nodiscard]] std::size_t DroppedSpawns() const { return droppedSpawns_; }

private:
    std::array<T, Capacity> items_{};
    std::size_t droppedSpawns_ = 0;
};

}  // namespace redmist
