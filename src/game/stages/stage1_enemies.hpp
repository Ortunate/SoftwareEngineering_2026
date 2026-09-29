// 本文件负责声明第一面普通妖精、暗色妖精和光球敌人。

#pragma once

#include "game/stage_content.hpp"
#include "game/stages/stage1_ids.hpp"

namespace redmist::stage1 {

/// @brief 普通妖精固定使用蓝衣外观，自行选择直行、绕行或交叉入场。
class FairyEnemy final : public Enemy {
public:
    /// @brief 按出生编队选择本妖精内部的移动状态。
    explicit FairyEnemy(const EnemySpawn& spawn);

private:
    enum class Movement { Straight, Helix, CrossFromLeft, CrossFromRight };
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
    /// @brief 按出生编队选择暗色妖精内部的移动状态。
    explicit DarkFairyEnemy(const EnemySpawn& spawn);

private:
    enum class Movement { Drift, Corkscrew, CrossFromLeft, CrossFromRight };
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
    /// @brief 根据编队上下文初始化光球运动。
    explicit OrbEnemy(const EnemySpawn& spawn);

private:
    enum class Movement { FloatForward, OrbitForward, CrossArena };
    void Move(float deltaTime) override;
    void UpdateSkills(float deltaTime, Vector3 playerPosition,
                      BulletSystem& bullets) override;
    void FireCone(BulletSystem& bullets, Vector3 playerPosition);

    Movement movement_;
    float shotTimer_;
};

}  // namespace redmist::stage1
