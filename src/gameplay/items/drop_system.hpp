// 本文件负责定义两类掉落物及统一生成与拾取接口。

#pragma once

#include "core/fixed_pool.hpp"
#include "gameplay/items/drop_table.hpp"

#include "raylib.h"

#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <utility>

namespace redmist {

/// @brief 掉落物种类；P 点增加 Power，分数点增加分数。
enum class PickupKind : std::uint8_t { Power, Score };

/// @brief 固定池中的道具；position 为世界坐标，age 为存活秒数。
/// value 对 P 点是 Power 增量，对分数点是得分增量；active 失活后槽位可复用。
struct Pickup {
    bool active = false;
    PickupKind kind = PickupKind::Power;
    Vector3 position{};
    int value = 1;
    float age = 0.0F;
};

/// @brief 本帧拾取所得，由 Game 应用到玩家和分数状态。
struct PickupReward {
    int power = 0;
    long long score = 0;
};

/// @brief 按敌人的权重分布抽取并生成两类道具，持有其固定池并处理吸附与拾取。
class DropSystem {
public:
    /// @brief 场上最多同时保留的道具数；沿用原有的 1024 槽容量。
    static constexpr std::size_t kCapacity = 1024;
    /// @brief 单个分数点的当前项目得分值。
    static constexpr int kScorePointValue = 1000;

    /// @brief 使用随机种子构造生成器。
    DropSystem();
    /// @brief 使用指定种子构造生成器，供可复现的测试使用。
    explicit DropSystem(std::uint32_t seed);

    /// @brief 清除场上道具；随机序列继续推进，不因重新开始而重复。
    void Clear();
    /// @brief 在敌人死亡位置抽取一次结果并生成相应数量的两类道具。
    /// @param position 敌人死亡时的世界坐标。
    /// @param distribution 由敌人持有的只读结果表；仅在调用期间借用。
    /// @note 单次生成每种道具最多 32 个，池满时多余道具被丢弃。
    void SpawnEnemyDrops(Vector3 position,
                         std::span<const DropOutcome> distribution);
    /// @brief 暂时保留道中 Boss 的固定大 P 点奖励；Boss 专属掉落规则留待后续实现。
    /// @param position 退场位置，使用世界坐标。
    /// @param power 增加的 Power 点数；非正值不生成。
    void SpawnFixedPower(Vector3 position, int power);
    /// @brief 推进道具移动并汇总本帧玩家拾取结果。
    /// @param deltaTime 本帧非负秒数。
    /// @param playerPosition 玩家判定中心的世界坐标。
    /// @return 本帧拾取的 Power 和分数，调用方负责应用上限与满火力效果。
    PickupReward Update(float deltaTime, Vector3 playerPosition);

    /// @brief 遍历活动道具，只借用对象；回调不得保留引用。
    template <typename Function>
    void ForEachActive(Function&& function) const {
        pickups_.ForEachActive(std::forward<Function>(function));
    }

private:
    void Spawn(PickupKind kind, Vector3 position, int value);

    FixedPool<Pickup, kCapacity> pickups_{};
    std::mt19937 random_;
};

}  // namespace redmist
