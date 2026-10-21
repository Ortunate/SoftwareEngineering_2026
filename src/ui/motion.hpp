// 本文件保存菜单的展示时钟、选择渐变和确认闪光；不参与输入命中或战斗时间轴。
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace koumakyou::ui {
/// @brief 菜单展示状态；page 为调用方的页面编号，selection 为该页局部行号（0～15，-1 表示无选择）。
/// 时钟单位为秒，只由 Update 推进，Draw 不修改状态；换页清空旧行的强调，避免跨页残影。
class MenuMotion {
public:
    /// @brief 在输入处理后推进展示；deltaTime 为非负秒数，非法值按零处理，不延迟输入生效。
    void Advance(int page, int selection, float deltaTime) {
        const float dt = std::isfinite(deltaTime) ? std::max(0.0F, deltaTime) : 0.0F;
        if (page != page_) {
            page_ = page;
            age_ = 0;
            weights_.fill(0);
            if (selection >= 0 && selection < static_cast<int>(weights_.size())) weights_[selection] = 1;
        }
        age_ = std::min(age_ + dt, 10.0F);
        phase_ += dt; // 不按任意秒数回绕，避免太极和环纹周期不同时产生可见跳变。
        flash_ = std::max(0.0F, flash_ - dt / 0.18F);
        // 指数趋近对 FPS 不敏感；快速连按时保留各行当前强度，不跳回动画起点。
        const float blend = 1 - std::exp(-dt / 0.055F);
        for (int index = 0; index < static_cast<int>(weights_.size()); ++index)
            weights_[index] += ((index == selection ? 1.0F : 0.0F) - weights_[index]) * blend;
    }
    /// @brief 当前行强调强度（0～1）；越界行不强调。
    [[nodiscard]] float Weight(int index) const {
        return index >= 0 && index < static_cast<int>(weights_.size()) ? weights_[index] : 0;
    }
    /// @brief 页面元素的缓出进场进度（0～1）；delay 为秒，供相邻行错开进场。
    [[nodiscard]] float Reveal(float delay = 0) const {
        const float t = std::clamp((age_ - delay) / 0.26F, 0.0F, 1.0F);
        return 1 - (1 - t) * (1 - t) * (1 - t);
    }
    /// @brief 低频呼吸亮度与装饰旋转相位；独立于真实墙钟，隐藏窗口回放可重复。
    [[nodiscard]] float Pulse() const { return 0.85F + 0.15F * std::sin(phase_ * 4.0F); }
    [[nodiscard]] float Phase() const { return phase_; }
    /// @brief 值调整或确认后的短促亮光；不阻止输入，不影响设置保存。
    void Confirm() { flash_ = 1; }
    [[nodiscard]] float Flash() const { return flash_; }
private:
    int page_ = -1;
    float age_ = 0, phase_ = 0, flash_ = 0;
    std::array<float, 16> weights_{}; // 容量覆盖最多 16 组符卡，以及 10 项绑定和恢复按钮组成的改键页。
};
} // namespace koumakyou::ui
