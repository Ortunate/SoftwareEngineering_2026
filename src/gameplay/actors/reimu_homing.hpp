// 本文件保存从本机新典解析的灵梦追踪弹发射通道；由 tools/import_reimu_homing.py 导入。
// 输入 SHA-256：48630a42a2eb6762d0db2a7e0d151203efbe67220fef7d2f9d0d51857928ac82；火力表 RVA：0x427a00。
#pragma once
#include <array>
#include <span>

namespace koumakyou::reimu_homing {
// 原作按 60 Hz 推进，射击计时在 30 帧后归零；满火力的 16 帧周期也在该窗口内取模。
inline constexpr int kFramesPerSecond = 60;
inline constexpr int kCycleFrames = 30;
/// @brief 左右模块同时各发一颗的通道；cycleFrame % periodFrames == phaseFrame 时发射。
/// halfAngleDegrees 为相对准星的单侧横向角（度），左负右正；周期和相位单位为 60 Hz 帧。
struct Pair { int periodFrames; int phaseFrame; float halfAngleDegrees; };
/// @brief Power 达到 minimumPower 时使用该档；pairs 借用下方静态数组，不随玩家状态变化。
struct Level { int minimumPower; std::span<const Pair> pairs; };

inline constexpr std::array<Pair, 0> kPower0{{
}};
inline constexpr std::array<Pair, 1> kPower8{{
    {30, 0, 30.0F},
}};
inline constexpr std::array<Pair, 1> kPower16{{
    {30, 0, 30.0F},
}};
inline constexpr std::array<Pair, 1> kPower32{{
    {30, 0, 30.0F},
}};
inline constexpr std::array<Pair, 1> kPower48{{
    {15, 0, 30.0F},
}};
inline constexpr std::array<Pair, 2> kPower64{{
    {15, 0, 30.0F},
    {30, 0, 60.0F},
}};
inline constexpr std::array<Pair, 3> kPower80{{
    {30, 0, 20.0F},
    {30, 10, 40.0F},
    {30, 20, 60.0F},
}};
inline constexpr std::array<Pair, 3> kPower96{{
    {15, 0, 20.0F},
    {15, 5, 40.0F},
    {15, 10, 60.0F},
}};
inline constexpr std::array<Pair, 4> kPower128{{
    {16, 0, 20.0F},
    {16, 4, 40.0F},
    {16, 8, 60.0F},
    {16, 12, 80.0F},
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

/// @brief 返回 Power 对应的追踪通道；Power 单位为点，低于 0 或高于 128 时沿用首档/末档。
inline const Level& ForPower(int power) {
    const Level* level = &kLevels.front();
    for (const auto& candidate : kLevels) {
        if (power < candidate.minimumPower) break;
        level = &candidate;
    }
    return *level;
}
}  // namespace koumakyou::reimu_homing
