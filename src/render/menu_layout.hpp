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
/// @brief 标题页的响应式布局；使用完整窗口，方形/竖屏将标题放到菜单上方。
/// width/height 为窗口逻辑像素，须为正数；行数包含 Debug 专用项，绘制和点击必须共用。
/// canvas 坐标原点在窗口左上角，不采用其他菜单页的固定画布居中留边。
struct TitleLayout {
    Canvas canvas;
    float width;
    float height;
    bool compact;
    float menuLeft;
    float menuTop;
    float menuWidth;
    int rowCount;

    TitleLayout(int windowWidth, int windowHeight, int count)
        : canvas(windowWidth, windowHeight), compact(windowWidth < windowHeight * 1.2F), rowCount(count) {
        if (compact) canvas.scale = std::min(windowWidth / 900.0F, windowHeight / 900.0F);
        canvas.origin = {};
        width = windowWidth / canvas.scale;
        height = windowHeight / canvas.scale;
        menuWidth = compact ? 390.0F : 315.0F;
        menuLeft = width - menuWidth - (compact ? 28.0F : 40.0F);
        // 窄屏先为标题保留 240 像素；最后一行和页脚之间始终留出操作空间。
        menuTop = compact ? std::max(240.0F, (height - count * 45 - 100 + 240) / 2)
                          : 172 + (height - 760) / 2;
    }
    /// @brief 菜单行的窗口点击矩形；index 为 0～rowCount-1，行距为设计像素。
    [[nodiscard]] Rectangle Row(int index) const {
        return canvas.Rect(menuLeft, menuTop + index * 45, menuWidth, 40);
    }
    /// @brief 等比铺满窗口的素材裁剪矩形（纹理像素）；纹理尺寸须为正，保留左侧人物。
    [[nodiscard]] Rectangle BackgroundSource(int textureWidth, int textureHeight) const {
        const float cover = std::max(width / textureWidth, height / textureHeight);
        const float sourceWidth = width / cover;
        const float sourceHeight = height / cover;
        return {0, (textureHeight - sourceHeight) * 0.25F, sourceWidth, sourceHeight};
    }
};
#ifdef RED_MIST_DEBUG_FEATURES
inline constexpr int kTitleRowCount = 11;
#else
inline constexpr int kTitleRowCount = 10;
#endif
}  // namespace redmist::menu
