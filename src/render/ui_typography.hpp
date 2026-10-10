// 本文件统一 UI 的宋体/衬线文字绘制与测量，避免菜单、战斗和弹窗混用像素字体或不同字距。
#pragma once

#include "raylib.h"

namespace koumakyou::ui {
// 字距按实际字号缩放；标题与 HUD 共用同一比例，测量和绘制必须保持一致。
inline constexpr float kTrackingRatio = 0.015F;

/// @brief 测量 UTF-8 UI 文本的屏幕尺寸；font 借用 CommonAssets::uiFont，size 为正数屏幕像素。
inline Vector2 MeasureText(Font font, const char* text, float size) {
    return MeasureTextEx(font, text, size, size * kTrackingRatio);
}

/// @brief 绘制 UTF-8 UI 文本；position/size 使用屏幕像素，字体仅借用，调用方负责画布坐标转换。
inline void DrawText(Font font, const char* text, Vector2 position, float size, Color color) {
    DrawTextEx(font, text, position, size, size * kTrackingRatio, color);
}

/// @brief 按 centerX 水平居中绘制；centerX/y/size 为屏幕像素，使用与 DrawText 相同的字距。
inline void DrawCentered(Font font, const char* text, float centerX, float y, float size, Color color) {
    DrawText(font, text, {centerX - MeasureText(font, text, size).x / 2, y}, size, color);
}
} // namespace koumakyou::ui
