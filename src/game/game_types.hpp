// 本文件负责定义玩家、弹幕、按键映射和游戏状态等共享数据类型。

#pragma once

#include "raylib.h"
#include "game/visual_assets.hpp"

#include <cstdint>

namespace redmist {

/// @brief 战场立方体从中心到任一面的距离，单位为世界坐标。
constexpr float kArenaHalfExtent = 10.0F;
/// @brief 玩家移动边界内侧预留的距离。
constexpr float kArenaPadding = 0.35F;

/// @brief 区分玩家弹与敌弹，用于碰撞、清弹和计分。
enum class BulletOwner : std::uint8_t { Player, Enemy };
/// @brief 对象池中的单颗弹幕，position 为球形判定中心。
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

/// @brief 玩家位置、移动、判定、资源与战斗计时状态。
struct Player {
    // 摄像机位置与玩家判定球中心始终相同。
    Vector3 position{0.0F, 0.0F, -6.5F};
    float yaw = 0.0F;
    float pitch = 0.0F;
    float speed = 6.5F;
    float focusSpeed = 2.8F;
    float mouseSensitivity = 0.0024F;
    float hitRadius = 0.18F;
    float grazeRadius = 0.72F;
    float fireTimer = 0.0F;
    float invulnerableTimer = 1.5F;
    float bombCooldown = 0.0F;
    int lives = 3;
    int bombs = 3;
    int power = 1;
};

/// @brief 可由设置页面修改的战斗按键映射。
struct KeyBindings {
    int forward = KEY_W;
    int backward = KEY_S;
    int moveLeft = KEY_A;
    int moveRight = KEY_D;
    int ascend = KEY_SPACE;
    int descend = KEY_LEFT_SHIFT;
    int focus = KEY_C;
    int fire = KEY_Z;
    int bomb = KEY_X;
    int pause = KEY_P;
};

/// @brief 游戏的菜单、战斗及结算状态。
enum class RunState : std::uint8_t {
    MainMenu,
    Options,
    Playing,
    Paused,
    Won,
    GameOver
};

}  // namespace redmist
