// 本文件保存新典梦之御札侧针通道；由 tools/import_reimu_needle.py 只读导入，主弹不在此表。
// 输入 SHA-256：48630a42a2eb6762d0db2a7e0d151203efbe67220fef7d2f9d0d51857928ac82；火力表 RVA：0x428180。
#pragma once
#include <array>
#include <span>

namespace koumakyou::reimu_needle {
// 时序沿用 60 Hz、30 帧窗口；偏移采用原始逻辑像素，X 向屏幕右，Y 向屏幕下。
inline constexpr float kOffsetYPixels = -16.0F;
inline constexpr float kSpeedPixelsPerFrame = 22.0F;
/// @brief 左右模块各一颗的直射针束；周期/相位为 60 Hz 帧，offsetXPixels 为两侧共用的横向偏移。
/// nativeDamage 为新典每颗针的原始伤害；渲染与三维数值的换算由 Player 负责。
struct Pair { int periodFrames; int phaseFrame; float offsetXPixels; float nativeDamage; };
/// @brief Power 达到 minimumPower 后使用该档；pairs 借用下方静态数组。
struct Level { int minimumPower; std::span<const Pair> pairs; };

inline constexpr std::array<Pair, 0> kPower0{{
}};
inline constexpr std::array<Pair, 1> kPower8{{
    {15, 0, 0.0F, 12.0F},
}};
inline constexpr std::array<Pair, 1> kPower16{{
    {10, 0, 0.0F, 12.0F},
}};
inline constexpr std::array<Pair, 1> kPower32{{
    {8, 0, 0.0F, 12.0F},
}};
inline constexpr std::array<Pair, 1> kPower48{{
    {8, 0, 0.0F, 12.0F},
}};
inline constexpr std::array<Pair, 2> kPower64{{
    {5, 0, 8.0F, 12.0F},
    {8, 0, -8.0F, 10.0F},
}};
inline constexpr std::array<Pair, 2> kPower80{{
    {3, 0, 8.0F, 10.0F},
    {5, 0, -8.0F, 10.0F},
}};
inline constexpr std::array<Pair, 3> kPower96{{
    {3, 0, 12.0F, 10.0F},
    {5, 0, -12.0F, 10.0F},
    {10, 0, 0.0F, 10.0F},
}};
inline constexpr std::array<Pair, 3> kPower128{{
    {3, 0, 12.0F, 10.0F},
    {3, 0, -12.0F, 10.0F},
    {5, 0, 0.0F, 10.0F},
}};
inline constexpr std::array<Level, 9> kLevels{{
    {0, kPower0},
    {8, kPower8},
    {16, kPower16},
    {32, kPower32},
    {48, kPower48},
    {64, kPower64},
    {80, kPower80},
    {96, kPower96},
    {128, kPower128},
}};

/// @brief 返回 Power 对应的侧针档；越界值沿用首档或末档，不改变玩家状态。
inline const Level& ForPower(int power) {
    const Level* level = &kLevels.front();
    for (const auto& candidate : kLevels) {
        if (power < candidate.minimumPower) break;
        level = &candidate;
    }
    return *level;
}
}  // namespace koumakyou::reimu_needle
