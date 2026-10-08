// 本文件负责声明第一面普通妖精、暗色妖精和光球敌人。

#pragma once

#include "gameplay/actors/enemy.hpp"
#include "stages/stage1/enemy_params.hpp"
#include "stages/stage1/ids.hpp"

namespace redmist::stage1 {

/// @brief 普通妖精固定使用蓝衣外观，按显式参数直行、绕行或交叉入场。
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
    void FireAimedShot(BulletSystem& bullets, Vector3 playerPosition);

    Movement movement_;
    float shotTimer_;
};

/// @brief 暗色妖精固定使用紫衣外观，并拥有自己的漂移与交叉轨迹。
class DarkFairyEnemy final : public Enemy {
public:
    /// @brief 复制暗色妖精的出生位置与显式轨迹；生成后立即活动。
    /// @param params 有限的世界坐标与对应敌人支持的移动枚举。
    explicit DarkFairyEnemy(const DarkFairyParams& params);

private:
    using Movement = DarkFairyParams::Movement;
    void Move(float deltaTime) override;
    void UpdateSkills(float deltaTime, Vector3 playerPosition,
                      BulletSystem& bullets) override;
    void FireAimedShot(BulletSystem& bullets, Vector3 playerPosition);

    Movement movement_;
    float shotTimer_;
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
