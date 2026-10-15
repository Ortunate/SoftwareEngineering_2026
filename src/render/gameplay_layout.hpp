// 本文件定义全窗口战斗视口、悬浮 HUD 与演出的响应式布局；坐标均为窗口逻辑像素。
#pragma once
#include "raylib.h"
#include <algorithm>

namespace koumakyou {
/// @brief 3D 永远覆盖整个窗口；HUD 按边缘锚定，scale 仅缩放文字/装饰，不拉伸立绘。
/// 零尺寸按 1 像素处理；超小窗口继续缩放以保持布局有限且不越界，不改变世界坐标和相机视角。
struct GameplayLayout {
    float width, height, scale, margin;
    bool compact;
    GameplayLayout(float windowWidth, float windowHeight)
        : width(std::max(1.0F, windowWidth)), height(std::max(1.0F, windowHeight)),
          scale(std::min({width / 500.0F, height / 300.0F,
                          std::clamp(height / 760.0F, 0.75F, 1.6F)})),
          margin(16 * scale), compact(width < 900 * scale || height < 540) {}

    [[nodiscard]] Rectangle Viewport() const { return {0, 0, width, height}; }
    /// @brief 左上题签与右上状态卡为放大的文字预留列宽和行距，不裁掉底下的 3D 场景。
    [[nodiscard]] Rectangle StagePanel() const {
        return {margin, margin, (compact ? 208 : 232) * scale, (compact ? 138 : 158) * scale};
    }
    [[nodiscard]] Rectangle StatusPanel() const {
        const float w = (compact ? 250 : 272) * scale;
        return {width - margin - w, margin, w, (compact ? 184 : 318) * scale};
    }
    /// @brief 宽窗口放在两侧卡片之间；窄窗口放在卡片下方，矮窗口改放底部以避开准星。
    [[nodiscard]] Rectangle BossPanel() const {
        if (compact) {
            const float h = 64 * scale;
            const float y = height < 540 * scale ? height - margin - h
                : StatusPanel().y + StatusPanel().height + 12 * scale;
            return {margin, y, width - 2 * margin, h};
        }
        const float left = StagePanel().x + StagePanel().width + 20 * scale;
        const float right = StatusPanel().x - 20 * scale;
        const float w = std::min(540 * scale, right - left);
        return {left + (right - left - w) / 2, margin, w, 76 * scale};
    }
    /// @brief 台词框居中贴近底部，两行正文与说话人使用独立区域。
    [[nodiscard]] Rectangle DialoguePanel() const {
        const float w = std::min(width - 2 * margin, 1100 * scale);
        return {(width - w) / 2, height - margin - 140 * scale, w, 140 * scale};
    }
    /// @brief aspect 为去除透明留白后的可见取景宽高比，须为正；focus 为 0～1 的发言强调。
    /// 发言者等比贴齐窗口左右与底边；非发言者缩至 90%，向对应底角微移，离屏部分由窗口裁切。
    [[nodiscard]] Rectangle DialoguePortrait(float aspect, bool right, float focus = 1) const {
        const float emphasis = std::clamp(focus, 0.0F, 1.0F);
        const float baseHeight = std::min(height * 0.88F, width * (compact ? 0.60F : 0.46F) / aspect);
        const float h = baseHeight * (0.90F + 0.10F * emphasis), w = h * aspect;
        const float retreat = 1 - emphasis;
        const float offsetX = retreat * std::min(12 * scale, width * 0.025F);
        const float offsetY = retreat * std::min(18 * scale, height * 0.025F);
        return {right ? width - w + offsetX : -offsetX, height - h + offsetY, w, h};
    }
    /// @brief 符卡立绘完整等比展示；slide 为 0～1 的剩余滑入比例，离屏部分由视口裁切。
    [[nodiscard]] Rectangle SpellPortrait(float aspect, float slide = 0) const {
        const float h = std::min({height * 0.88F, 760 * scale, width * 0.58F / aspect});
        const float w = h * aspect;
        return {width - margin - w + w * std::clamp(slide, 0.0F, 1.0F), height - h, w, h};
    }
    [[nodiscard]] Rectangle SpellBanner() const {
        const float w = std::min(width - 2 * margin, 560 * scale);
        return {width - margin - w, height * 0.72F, w, 66 * scale};
    }
    /// @brief 临时提示位于下方中央；矮窗口的 Boss 条在底部时，提示向上避让。
    [[nodiscard]] Rectangle Feedback(bool bossVisible = false) const {
        const float w = std::min(width - 2 * margin, 620 * scale);
        const float bottom = compact && height < 540 * scale && bossVisible ? BossPanel().y - 8 * scale
            : height - margin - 24 * scale;
        return {(width - w) / 2, bottom - 48 * scale, w, 48 * scale};
    }
};
} // namespace koumakyou
