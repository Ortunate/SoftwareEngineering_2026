// 本文件负责定义普通掉落物、清弹星点及统一生成与拾取接口。

#pragma once

#include "core/fixed_pool.hpp"
#include "gameplay/items/drop_table.hpp"

#include "raylib.h"

#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

namespace redmist {

class BulletSystem;

/// @brief 道具种类；P 点增加 Power，分数点与清弹星点增加分数。
enum class PickupKind : std::uint8_t { Power, Score, Star };

/// @brief 系统持有的道具；position 为世界坐标，age 为存活秒数。
/// velocityZ 为非追踪运动的世界 Z 轴速度（单位/秒），正值上抛、负值下落；追踪时忽略。
/// value 对 P 点是 Power 增量，对分数点和星点是得分增量；homing 一经触发便持续到拾取或消失。
/// active 失活后普通道具槽位可复用，星点由容器移除；复用时会重置全部运动状态。
struct Pickup {
    bool active = false;
    bool homing = false;
    PickupKind kind = PickupKind::Power;
    Vector3 position{};
    int value = 1;
    float age = 0.0F;
    float velocityZ = 0.0F;
};

/// @brief 本帧拾取所得，由 Game 应用到玩家和分数状态。
struct PickupReward {
    int power = 0;
    long long score = 0;
};

/// @brief 生成敌人掉落物和清弹星点，持有道具并处理运动、自动吸取与拾取。
class DropSystem {
public:
    /// @brief 普通掉落物的固定池容量；清弹星点另行存储，不占这些槽位。
    static constexpr std::size_t kCapacity = 1024;
    /// @brief 单个分数点的当前项目得分值。
    static constexpr int kScorePointValue = 1000;
    /// @brief 单个清弹星点的得分值；仅在被玩家吸收时结算。
    static constexpr int kStarPointValue = 100;

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
    /// @brief 将当前全部活动敌弹在原位置转成立即追踪玩家的星点，保留玩家弹。
    /// @param bullets 借用的弹幕系统；转换的敌弹失活，不再参与碰撞。
    /// @note 不受普通道具池容量限制，已有星点也会继续追踪。
    void ConvertEnemyBulletsToStars(BulletSystem& bullets);
    /// @brief 推进道具并拾取；普通掉落物先向 +Z 上抛，再沿 -Z 加速至下落速度上限。
    /// @note 玩家进入 +Z 端五分之一区域时，所有活动道具立即持续追踪玩家，优先于上抛和下落。
    /// 已触发的道具在玩家离开该区域后仍追踪；之后生成的道具重新从上抛开始。
    /// 星点生成时已触发追踪，不受玩家所处区域限制；所有道具存活超过 15 秒后消失。
    /// @param deltaTime 本帧非负秒数。
    /// @param playerPosition 玩家判定中心的世界坐标。
    /// @return 本帧近距离拾取的 Power 和分数，调用方负责应用上限与满火力效果。
    PickupReward Update(float deltaTime, Vector3 playerPosition);

    /// @brief 遍历普通道具和星点，只借用对象；回调不得保留引用或修改道具容器。
    template <typename Function>
    void ForEachActive(Function&& function) const {
        pickups_.ForEachActive(function);
        for (const Pickup& star : stars_)
            if (star.active) function(star);
    }

private:
    void Spawn(PickupKind kind, Vector3 position, int value);

    FixedPool<Pickup, kCapacity> pickups_{};
    // 星点使用可增长容器，确保弹幕池满或普通道具池满时也能逐弹转换；拾取/过期后回收。
    std::vector<Pickup> stars_;
    std::mt19937 random_;
};

}  // namespace redmist
