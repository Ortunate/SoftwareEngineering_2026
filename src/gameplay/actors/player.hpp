// 本文件负责定义玩家移动、判定与资源状态。

#pragma once

#include "raylib.h"

namespace redmist {

/// @brief 玩家位置、移动、判定、资源与战斗计时状态；长度采用世界单位，时间采用秒。
struct Player {
    // 摄像机位置与玩家判定球中心始终相同。
    Vector3 position{0.0F, 0.0F, -6.5F};
    // yaw/pitch 均为弧度；鼠标灵敏度为每像素的角度增量。
    float yaw = 0.0F;
    float pitch = 0.0F;
    float speed = 6.5F;
    float focusSpeed = 2.8F;
    float mouseSensitivity = 0.0024F;
    float hitRadius = 0.18F;
    float grazeRadius = 0.72F;
    // 计时器均表示剩余秒数，降至零后允许再次触发对应行为。
    float fireTimer = 0.0F;
    float invulnerableTimer = 1.5F;
    float bombCooldown = 0.0F;
    int lives = 3;
    int bombs = 3;
    int power = 1;
};

}  // namespace redmist
