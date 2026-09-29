// 本文件负责定义战场边界尺寸。

#pragma once

#include "raylib.h"

namespace redmist {

/// @brief 战场立方体从中心到任一面的距离，单位为世界坐标。
constexpr float kArenaHalfExtent = 10.0F;
/// @brief 玩家移动边界内侧预留的距离。
constexpr float kArenaPadding = 0.35F;

}  // namespace redmist
