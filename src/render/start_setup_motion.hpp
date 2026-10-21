// 本文件保存 Start 各步骤的连续混合权重和卡片入场时钟，不持有纹理、不参与输入和存档。
#pragma once
#include "app/start_setup.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace koumakyou::ui {
/// @brief 页面权重在 0.30 秒内缓入缓出；首页与全部步骤的权重总和始终为 1。
/// 换页从当前混合值继续，人物和已选摘要按相关页面权重之和绘制，连续步骤不会重新显现。
class StartSetupMotion {
public:
    /// @brief page=-1 表示首页，0～Count-1 对应 StartSetupStep，-2 表示离开该流程；dt 为秒，非法值按零处理。
    /// 离开流程立即清空展示，不保留游戏/设置页内容；绘制或窗口缩放不能调用此函数推进时间。
    void Advance(int page,float dt) {
        dt=std::isfinite(dt)?std::max(0.0F,dt):0;
        if(page < -1 || page >= static_cast<int>(StartSetupStep::Count)) { *this=StartSetupMotion{}; return; }
        const int target=page+1;
        if(target!=target_) {
            source_=weights_;
            if(target>0 && weights_[target]<=0.00001F)ages_[target-1]=0;
            target_=target; elapsed_=0;
        }
        elapsed_=std::min(0.30F,elapsed_+dt);
        const float t=Ease(elapsed_/0.30F);
        for(std::size_t index=0;index<weights_.size();++index)
            weights_[index]=source_[index]*(1-t)+(int(index)==target_?t:0);
        for(std::size_t index=0;index<ages_.size();++index)
            if(weights_[index+1]>0 || int(index+1)==target_)ages_[index]=std::min(10.0F,ages_[index]+dt);
    }
    [[nodiscard]] float Title() const { return weights_[0]; }
    [[nodiscard]] float Layer(StartSetupStep step) const { return weights_[1+static_cast<int>(step)]; }
    [[nodiscard]] float Setup() const { return 1-Title(); }
    /// @brief 人物在除难度之外的页面持续可见，御札/模式/最终确认间切换时不重复播放人物入场。
    [[nodiscard]] float Character() const { return std::clamp(Setup()-Layer(StartSetupStep::Difficulty),0.0F,1.0F); }
    /// @brief 难度札 index 为 0～3；间隔 0.07 秒错峰，单张用 0.38 秒滑入及淡入，返回时可从当前进度衔接。
    [[nodiscard]] float CardEntry(int index) const {
        return Ease((ages_[static_cast<int>(StartSetupStep::Difficulty)]-0.04F-index*0.07F)/0.38F);
    }
private:
    static float Ease(float t) { t=std::clamp(t,0.0F,1.0F);return t*t*(3-2*t); }
    int target_=0;
    float elapsed_=0.30F;
    std::array<float,1+static_cast<int>(StartSetupStep::Count)> weights_{1},source_{1};
    std::array<float,static_cast<int>(StartSetupStep::Count)> ages_{}; // 各步骤独立的进入秒数；尚有可见权重时返回不会把时钟清零。
};
} // namespace koumakyou::ui
