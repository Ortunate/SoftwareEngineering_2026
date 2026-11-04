// 本文件负责定义战场边界尺寸。

#pragma once

#include "raylib.h"

namespace koumakyou {

/// @brief 以原点为中心的长方体战场各轴半尺寸，单位为世界坐标。
/// X/Y 截面保持正方形，Z 为长轴；参考图长宽比约 1.15，完整尺寸为 20×20×23。
/// 渲染和边界判定必须逐轴使用对应分量，不能用同一半尺寸代替三个方向。
constexpr Vector3 kArenaHalfExtents{10.0F, 10.0F, 11.5F};
/// @brief 玩家移动边界内侧预留的距离。
constexpr float kArenaPadding = 0.35F;

}  // namespace koumakyou
