// 本文件负责声明第一面普通妖精、暗色妖精和光球敌人。

#pragma once

#include "gameplay/actors/enemy.hpp"
#include "stages/stage1/enemy_params.hpp"
#include "stages/stage1/ids.hpp"

namespace redmist::stage1 {

/// @brief 普通妖精使用蓝衣外观并按显式轨迹移动；不攻击，当前玩家任一子弹命中即可击破。
class FairyEnemy final : public Enemy {
public:
    /// @brief 复制普通妖精的出生位置与显式轨迹；生成后立即活动。
    /// @param params 有限的世界坐标与对应敌人支持的移动枚举。
    explicit FairyEnemy(const FairyParams& params);

private:
    using Movement = FairyParams::Movement;
    void Move(float deltaTime) override;
    void UpdateSkills(float deltaTime, Vector3 playerPosition,
                      BulletSystem& bullets) override;

    Movement movement_;
};

/// @brief 暗色妖精沿 -Z 前进战场全长的五分之一，停下发射一次球面弹幕，再沿 +Z 原路退场。
/// 往返速度均为 1.7 世界单位/秒；退场在出生位置失活，不触发击破分数或掉落。
class DarkFairyEnemy final : public Enemy {
public:
    /// @brief 复制出生位置与发弹后等待时间；每个实例只发射一次弹幕。
    /// @param params 有限的世界坐标及非负有限的退场等待秒数。
    explicit DarkFairyEnemy(const DarkFairyParams& params);

private:
    void Move(float deltaTime) override;
    void UpdateSkills(float deltaTime, Vector3 playerPosition,
                      BulletSystem& bullets) override;
    /// @brief 从固定转折点向 18 个稀疏球面方向各生成一颗红弹，不随玩家位置转向。
    void FireRadialBurst(BulletSystem& bullets);

    float retreatDelay_;
    // 年龄可在一帧内跨越多个行为阶段；此标记保证跨越或停留期间均只发射一次。
    bool hasFired_ = false;
};

/// @brief 光球敌人固定使用蓝珠外观，在前进中上下浮动并释放锥形弹幕。
class OrbEnemy final : public Enemy {
public:
    /// @brief 复制光球的出生位置与显式轨迹；生成后立即活动。
    /// @param params 有限的世界坐标与对应敌人支持的移动枚举。
    explicit OrbEnemy(const OrbParams& params);

private:
    using Movement = OrbParams::Movement;
    void Move(float deltaTime) override;
    void UpdateSkills(float deltaTime, Vector3 playerPosition,
                      BulletSystem& bullets) override;
    void FireCone(BulletSystem& bullets, Vector3 playerPosition);

    Movement movement_;
    float shotTimer_;
};

}  // namespace redmist::stage1
