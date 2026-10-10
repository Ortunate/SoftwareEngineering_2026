// 本文件负责输入界面和战斗呈现共用的固定参数。

#pragma once

#include <array>

namespace redmist {
inline constexpr float kSpellCutInDuration = 2.0F;
inline constexpr std::array<const char*, 12> kBindingNames{
    "FORWARD", "BACKWARD", "MOVE LEFT", "MOVE RIGHT", "ASCEND",
    "DESCEND", "FOCUS / SLOW", "FIRE", "BOMB", "PAUSE", "FIRE (ALTERNATE)", "BOMB (ALTERNATE)",
};
/// @brief 改键页绘制与点击区域共用的像素布局，适配最小 900×620 窗口。
inline constexpr float kBindingsTop = 136.0F;
inline constexpr float kBindingsSpacing = 28.0F;
inline constexpr float kBindingsHeight = 25.0F;
}  // namespace redmist
