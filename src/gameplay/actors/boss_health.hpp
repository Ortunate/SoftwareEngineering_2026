// 本文件定义 Boss 已解析的血条快照及公共画笔；段宽与颜色由配置提供，HUD 不参与计算。
#pragma once
#include "raylib.h"
#include <cstddef>
#include <vector>
namespace koumakyou {
/// @brief 快照比例相对于整条血条，0≤remaining≤capacity≤1；颜色直接来自配置。
struct BossHealthSegment {float capacity=0,remaining=0;Color color=WHITE;};
/// @brief 值快照不持有角色和资源引用；segments 从左到右排列，数量不限，count 与 size 保持一致。
/// remainingBars 为当前阶段之后的阶段数；count=0 隐藏血条。
struct BossHealthBar {
    std::vector<BossHealthSegment> segments;
    std::size_t count=0;
    int remainingBars=0;
    Color background{64,35,50,160};
};
/// @brief 在窗口逻辑像素 area 内绘制快照；调用方须处于有效绘制上下文，颜色和位置完全取快照。
void DrawBossHealthBar(const BossHealthBar& bar,Rectangle area);
}
