// 本文件定义标题菜单和选项的绘制/点击布局，防止窗口缩放后交互区域与文字错位。
#pragma once

#include "raylib.h"
#include <algorithm>
#include <array>

namespace redmist::menu {
/// @brief 以 1100×760 为设计画布，等比缩放并居中；坐标单位为设计像素。
struct Canvas {
    float scale;
    Vector2 origin;
    Canvas(int width, int height)
        : scale(std::min(width / 1100.0F, height / 760.0F)),
          origin{(width - 1100 * scale) / 2, (height - 760 * scale) / 2} {}
    [[nodiscard]] Vector2 Point(float x, float y) const {
        return {origin.x + x * scale, origin.y + y * scale};
    }
    [[nodiscard]] Rectangle Rect(float x, float y, float width, float height) const {
        const auto point = Point(x, y);
        return {point.x, point.y, width * scale, height * scale};
    }
};

inline constexpr int kOptionsCount = 9;
inline constexpr int kNumericOptions = 6;
inline constexpr int kBindingsIndex = 6;
inline constexpr int kResetIndex = 7;
inline constexpr int kBackIndex = 8;
// 前三项沿用原有顺序；新增项目位于其后，编号不属于持久化配置格式。
inline constexpr std::array<const char*, kOptionsCount> kOptionLabels{
    "FPS LIMIT", "BGM Vol", "MOUSE SENSITIVITY", "SE Vol", "Player", "Bomb",
    "KEY BINDINGS", "RESET DEFAULTS", "Quit"};
/// @brief 第 index 行的屏幕矩形；index 为 0～8，菜单的文字与命中共用此处定义。
inline Rectangle OptionRow(const Canvas& canvas, int index) {
    return canvas.Rect(310, 160 + index * 48, 625, 42);
}
/// @brief 数值行独立箭头按钮；direction 为 -1（减小）或 1（增大）。
inline Rectangle OptionArrow(const Canvas& canvas, int index, int direction) {
    return canvas.Rect(direction < 0 ? 670 : 886, 165 + index * 48, 32, 32);
}
/// @brief 标题右侧菜单的屏幕点击矩形；index 为 0～9（Debug 增加第 10 行）。
inline Rectangle MainRow(const Canvas& canvas, int index) {
    return canvas.Rect(745, 172 + index * 45, 315, 40);
}
}  // namespace redmist::menu
