// 本文件保存对话双方的发言强调和显现过渡；只由 Update 推进，不持有立绘资源或改变台词时序。
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace koumakyou::ui {
/// @brief 对话角色的瞬时展示参数；focus 为 0～1，opacity 已合并显现进度与非发言者的 30% 不透明度。
struct DialoguePose {
    float focus, opacity;
};

/// @brief false/true 分别表示左侧玩家与右侧 Boss；独立于帧数和窗口尺寸，暂停时不要调用 Advance。
/// 位置及尺寸由布局根据 focus 计算，Draw 只读取 Sample，不可在绘制期间推进过渡。
class DialogueMotion {
public:
    /// @brief 新对话从本句的稳定状态开始；隐藏角色的显现进度为零，不继承上一段对话。
    void Reset(bool bossSpeaking, bool showBoss) {
        target_ = Targets(bossSpeaking, showBoss);
        current_ = from_ = target_;
        elapsed_ = kDuration;
        initialized_ = true;
    }
    /// @brief 离开对话时清空旧状态；下一次 Advance 按新对话初始化。
    void Clear() { *this = {}; }
    /// @brief 跟随当前句的发言者与 Boss 可见性；seconds 为秒，非法时间按零处理。
    /// 快速切换时从当前采样重新过渡，不能跳回上一次动画起点。
    void Advance(bool bossSpeaking, bool showBoss, float seconds) {
        if (!initialized_) { Reset(bossSpeaking, showBoss); return; }
        const auto next = Targets(bossSpeaking, showBoss);
        if (next != target_) {
            from_ = current_;
            target_ = next;
            elapsed_ = 0;
        }
        const float dt = std::isfinite(seconds) ? std::max(0.0F, seconds) : 0;
        elapsed_ = std::min(kDuration, elapsed_ + dt);
        const float t = elapsed_ / kDuration;
        const float blend = t * t * (3 - 2 * t);
        for (std::size_t index = 0; index < current_.size(); ++index)
            current_[index] = from_[index] + (target_[index] - from_[index]) * blend;
    }
    /// @brief 返回借用动画时钟的值采样；非发言者稳定 opacity=0.3，发言者为 1，未登场角色为 0。
    [[nodiscard]] DialoguePose Sample(bool right) const {
        const float focus = current_[right ? 1 : 0];
        return {focus, (0.3F + 0.7F * focus) * (right ? current_[2] : 1.0F)};
    }

private:
    /// @brief 三个通道依次为玩家强调、Boss 强调、Boss 显现；不允许隐藏角色成为发言者。
    static std::array<float, 3> Targets(bool bossSpeaking, bool showBoss) {
        const bool activeBoss = bossSpeaking && showBoss;
        return {activeBoss ? 0.0F : 1.0F, activeBoss ? 1.0F : 0.0F, showBoss ? 1.0F : 0.0F};
    }
    static constexpr float kDuration = 0.26F; // 秒；同一缓动驱动透明度、缩放和角落位移，0.26 秒后精确归位。
    std::array<float, 3> current_{1, 0, 0}, from_{1, 0, 0}, target_{1, 0, 0};
    float elapsed_ = 0;
    bool initialized_ = false;
};
} // namespace koumakyou::ui
