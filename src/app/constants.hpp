// 本文件负责输入界面和战斗呈现共用的固定参数。

#pragma once

#include "raylib.h"

#include <array>

namespace redmist {
inline constexpr Color kPlayerBulletColor{90, 225, 255, 255};
inline constexpr float kSpellCutInDuration = 2.6F;
inline constexpr std::array<const char*, 12> kBindingNames{
    "FORWARD", "BACKWARD", "MOVE LEFT", "MOVE RIGHT", "ASCEND",
    "DESCEND", "FOCUS / SLOW", "FIRE", "BOMB", "PAUSE", "FIRE (ALTERNATE)", "BOMB (ALTERNATE)",
};
/// @brief 设置菜单绘制与点击区域共用的像素布局，适配最小 900×620 窗口。
inline constexpr float kOptionsTop = 185.0F;
inline constexpr float kOptionsSpacing = 62.0F;
inline constexpr float kBindingsTop = 136.0F;
inline constexpr float kBindingsSpacing = 28.0F;
inline constexpr float kBindingsHeight = 25.0F;
inline constexpr int kBindingsOptionIndex = 3;
/// @brief 数值项箭头按钮的可见矩形与点击区域；direction 为 -1（减小）或 1（增大）。
/// @param centerX 当前窗口水平中心（像素）。
/// @param index 数值设置行索引 0～2。
inline Rectangle OptionArrowBounds(float centerX, int index, int direction) {
    return {centerX + (direction < 0 ? 56.0F : 218.0F),
            kOptionsTop + index * kOptionsSpacing + 9.0F, 32.0F, 32.0F};
}
}  // namespace redmist
