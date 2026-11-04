// 本文件按游戏时钟采样原生对话入退场及发言强调，并为无原生时间轴的关卡保留通用过渡。
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "stage/content.hpp"

namespace koumakyou::ui {
/// @brief 对话角色的瞬时展示参数；focus/opacity 为 0～1，slide 为向窗口外退场的比例。
struct DialoguePose {
    float focus, opacity;
    float slide = 0;
};

/// @brief false/true 分别表示左侧玩家与右侧 Boss；不持有纹理，暂停时保持已有采样。
/// 位置及尺寸由布局根据 focus 计算，Draw 只读取 Sample，不可在绘制期间推进过渡。
class DialogueMotion {
public:
    /// @brief 按整段对话的游戏秒数重建原生中断标签，支持跳句和任意帧率；暂停时保持已有采样。
    /// 标签 1/2/5 在 30 帧内入退场，2退到场外后仍可重用，5对应源动画结束；3/4 的 Alpha 用 16 帧变为 255/128。
    void Seek(std::span<const DialogueCue> cues, float seconds) {
        struct Channel {
            float from=0,to=0,at=0,duration=0;
            float Sample(float frame,bool ease) const {
                float t=duration>0 ? std::clamp((frame-at)/duration,0.0F,1.0F) : 1;
                if(ease)t=1-(1-t)*(1-t);
                return from+(to-from)*t;
            }
            void Set(float frame,float value,float frames,bool ease) {
                from=Sample(frame,ease);to=value;at=frame;duration=frames;
            }
        };
        std::array<Channel,2> focus{},alpha{},slide{};
        std::array<bool,2> visible{};
        const float frame=std::floor(std::max(0.0F,seconds)*60+0.001F);
        for(const auto& cue:cues) {
            if(cue.frame>frame)break;
            if((cue.kind!=DialogueCueKind::Portrait&&cue.kind!=DialogueCueKind::PortraitStart) || cue.slot<0 || cue.slot>1)continue;
            if(cue.kind==DialogueCueKind::PortraitStart&&cue.value!=0)continue;
            const int slot=cue.slot;
            // 普通脚本创建后从头执行并自然落入标签1；没有额外中断也会入场，不能只靠MSG5判定可见性。
            if(cue.kind==DialogueCueKind::PortraitStart || cue.value==1) {
                visible[slot]=true;focus[slot]={1,1,0,0};alpha[slot]={1,1,0,0};
                slide[slot]={1,0,static_cast<float>(cue.frame),30};
            } else if(cue.value==3 || cue.value==4) {
                const bool speaking=cue.value==3;
                focus[slot].Set(cue.frame,speaking?1:0,slot==0?16:30,true);
                alpha[slot].Set(cue.frame,speaking?1:128.0F/255,16,false);
            } else if(cue.value==2 || cue.value==5) {
                slide[slot].Set(cue.frame,1,30,true);
            }
        }
        for(int slot=0;slot<2;++slot) {
            const float offset=slide[slot].Sample(frame,true);
            nativePoses_[slot]={focus[slot].Sample(frame,true),
                visible[slot] && offset<1 ? std::floor(alpha[slot].Sample(frame,false)*255+0.001F)/255 : 0,offset};
        }
        native_=true;
    }
    /// @brief 新对话从本句的稳定状态开始；隐藏角色的显现进度为零，不继承上一段对话。
    void Reset(bool bossSpeaking, bool showBoss) {
        native_=false;
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
    /// @brief 返回已更新的值采样；原生非发言者 opacity=128/255，通用过渡为 0.3，隐藏角色为 0。
    [[nodiscard]] DialoguePose Sample(bool right) const {
        if(native_)return nativePoses_[right?1:0];
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
    bool native_ = false;
    std::array<DialoguePose,2> nativePoses_{};
};
} // namespace koumakyou::ui
