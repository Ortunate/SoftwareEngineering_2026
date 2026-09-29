// 本文件负责提供固定容量对象池，复用槽位并统计溢出生成请求。

#pragma once

#include <array>
#include <cstddef>
#include <utility>

namespace redmist {

/// @brief 使用定长数组复用对象槽位；T 必须具有公开的 active 标记。
/// @tparam T 对象类型。
/// @tparam Capacity 最大同时活动数量。
template <typename T, std::size_t Capacity>
class FixedPool {
public:
    /// @brief 找到失活槽位并重置为新对象。
    /// @return 新槽位指针；池满时返回 nullptr 并增加丢弃计数。
    T* Acquire() {
        for (T& item : items_) {
            if (!item.active) {
                // 复用槽位时重置全部字段，避免沿用上一个对象的状态。
                item = T{};
                item.active = true;
                return &item;
            }
        }
        ++droppedSpawns_;
        return nullptr;
    }

    /// @brief 对所有活动对象执行可修改回调。
    /// @tparam Function 接收 T& 的可调用类型。
    /// @param function 接收 T& 的回调。
    template <typename Function>
    void ForEachActive(Function&& function) {
        for (T& item : items_) {
            if (item.active) {
                std::forward<Function>(function)(item);
            }
        }
    }

    /// @brief 对所有活动对象执行只读回调。
    /// @tparam Function 接收 const T& 的可调用类型。
    /// @param function 接收 const T& 的回调。
    template <typename Function>
    void ForEachActive(Function&& function) const {
        for (const T& item : items_) {
            if (item.active) {
                std::forward<Function>(function)(item);
            }
        }
    }

    /// @brief 停用所有槽位并清零丢弃计数。
    void Clear() {
        for (T& item : items_) {
            item.active = false;
        }
        droppedSpawns_ = 0;
    }

    /// @brief 统计当前活动对象数。
    /// @return active 为 true 的槽位数量。
    [[nodiscard]] std::size_t ActiveCount() const {
        std::size_t count = 0;
        for (const T& item : items_) {
            count += item.active ? 1U : 0U;
        }
        return count;
    }

    /// @brief 查询因容量耗尽而失败的申请次数。
    /// @return 最近一次 Clear 之后的丢弃次数。
    [[nodiscard]] std::size_t DroppedSpawns() const { return droppedSpawns_; }

private:
    std::array<T, Capacity> items_{};
    std::size_t droppedSpawns_ = 0;
};

}  // namespace redmist
