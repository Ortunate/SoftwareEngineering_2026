// 本文件负责定义弹幕归属与对象池中的单颗弹幕。

#pragma once

#include "raylib.h"
#include "resources/visual_assets.hpp"
#include <cstdint>

namespace redmist {

/// @brief 区分玩家弹与敌弹，用于碰撞、清弹和计分。
enum class BulletOwner : std::uint8_t { Player, Enemy };
/// @brief 对象池中的单颗弹幕；位置和半径采用世界单位，速度为世界单位/秒，加速度为世界单位/秒²。
/// position 为球形判定中心；active 为 false 后槽位可被下次 Spawn 复用。
/// age 和 maxAge 为秒；rotationAxis 是旋转轴，angularVelocity 为弧度/秒。
struct Bullet {
    bool active = false;
    bool grazed = false;
    BulletOwner owner = BulletOwner::Enemy;
    VisualId visual = kInvalidVisualId;
    Vector3 position{};
    Vector3 velocity{};
    Vector3 acceleration{};
    Vector3 rotationAxis{0.0F, 1.0F, 0.0F};
    float radius = 0.16F;
    float age = 0.0F;
    float maxAge = 12.0F;
    float angularVelocity = 0.0F;
    Color color = WHITE;
};

}  // namespace redmist
