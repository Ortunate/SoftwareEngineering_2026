// 本文件定义新典风格 Boss 同行信息布局、血条颜色和倒计时规则，坐标均为窗口逻辑像素。
#pragma once
#include "gameplay/actors/boss_health.hpp"
#include "raylib.h"
#include <algorithm>
#include <cmath>

namespace koumakyou::ui {
/// @brief panel 来自 GameplayLayout::BossPanel，scale 为其正缩放；Enemy、血条及倒计时共享中心线。
/// 符卡名称单独占下方一行，不能占用血条宽度或遮挡倒计时；所有矩形仅描述布局，不持有资源。
struct BossHudLayout {
    Rectangle enemy, bar, timer, spell;
    BossHudLayout(Rectangle panel, float scale)
        : enemy{panel.x + 14 * scale, panel.y + 4 * scale, 84 * scale, 32 * scale},
          bar{panel.x + 108 * scale, panel.y + 16 * scale, panel.width - 174 * scale, 8 * scale},
          timer{panel.x + panel.width - 56 * scale, panel.y + 4 * scale, 42 * scale, 32 * scale},
          spell{panel.x + 14 * scale, panel.y + 39 * scale, panel.width - 28 * scale, 20 * scale} {}
};

/// @brief limit 为有限秒数，elapsed 为非负有限秒数；向下取整显示剩余整秒，钳制到 00～99。
/// limit 为负时返回 -1，表示尚无计时；零上限仍显示 00。
inline int BossSeconds(float limit, float elapsed) {
    if (limit < 0) return -1;
    // skillTimer 从整数 60 Hz 帧换算，容许浮点减法在整秒边界产生的微小误差。
    return static_cast<int>(std::clamp(std::floor(limit - elapsed + 0.0001F), 0.0F, 99.0F));
}
/// @brief 倒计时沿用蓝、紫、粉、红的 20/10/5 秒警示分界；负数时使用无计时占位的弱色。
inline Color BossTimerColor(int seconds) {
    if (seconds < 0) return {183, 165, 165, 255};
    if (seconds >= 20) return {160, 208, 255, 255};
    if (seconds >= 10) return {160, 128, 255, 255};
    if (seconds >= 5) return {224, 128, 192, 255};
    return {255, 64, 64, 255};
}
} // namespace koumakyou::ui
