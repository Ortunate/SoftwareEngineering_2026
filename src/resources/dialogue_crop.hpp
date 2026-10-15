// 本文件负责从立绘的对话取景区域中排除透明留白，提供实际可见内容的归一化边界。
#pragma once

#include "raylib.h"
#include <algorithm>
#include <cmath>

namespace koumakyou {
/// @brief 在有效 CPU 图片内收紧归一化 crop；调用方保留图片所有权，完全透明或无效图片返回原 crop。
/// crop 须为 [0,1] 内的正矩形；仅加载时调用，不能在逐帧绘制中解码或回读纹理。
/// Alpha ≤16 的修复残留不作为锚点，避免近透明杂点让人物与窗口之间留下可见空隙。
inline Rectangle FindDialogueContent(Image image, Rectangle crop) {
    if (!IsImageValid(image)) return crop;
    Color* pixels = LoadImageColors(image);
    if (!pixels) return crop;
    const int left = std::clamp(static_cast<int>(std::floor(crop.x * image.width)), 0, image.width);
    const int top = std::clamp(static_cast<int>(std::floor(crop.y * image.height)), 0, image.height);
    const int right = std::clamp(static_cast<int>(std::ceil((crop.x + crop.width) * image.width)), left, image.width);
    const int bottom = std::clamp(static_cast<int>(std::ceil((crop.y + crop.height) * image.height)), top, image.height);
    int x0 = right, y0 = bottom, x1 = left, y1 = top;
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            if (pixels[y * image.width + x].a <= 16) continue;
            x0 = std::min(x0, x); y0 = std::min(y0, y);
            x1 = std::max(x1, x + 1); y1 = std::max(y1, y + 1);
        }
    }
    UnloadImageColors(pixels);
    if (x0 >= x1 || y0 >= y1) return crop;
    // 与原取景相交，保留非整数裁剪边界，不把全身差分已经排除的裙摆重新纳入。
    const float x = std::max(crop.x, static_cast<float>(x0) / image.width);
    const float y = std::max(crop.y, static_cast<float>(y0) / image.height);
    return {x, y, std::min(crop.x + crop.width, static_cast<float>(x1) / image.width) - x,
                  std::min(crop.y + crop.height, static_cast<float>(y1) / image.height) - y};
}
} // namespace koumakyou
