// 本文件保存首页及子页的连续混合权重；不持有资源，不推进业务或输入。
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace koumakyou::ui {
/// @brief Count为子页数量；-1表示首页，0～Count-1为子页，其他编号立即清空展示并回到首页权重。
/// 权重总和为1，0.30秒完成过渡；中途换页从当前混合继续，绘制与窗口尺寸均不改变时钟。
template<std::size_t Count> class PageMotion {
public:
    /// @brief deltaTime为秒，负值和非有限值按零处理；零时间换页只更新目标，不改变可见权重。
    void Advance(int page,float deltaTime) {
        if(page < -1 || page >= static_cast<int>(Count)){*this=PageMotion{};return;}
        const int target=page+1;
        if(target!=target_){source_=weights_;target_=target;elapsed_=0;}
        const float dt=std::isfinite(deltaTime)?std::max(0.0F,deltaTime):0;
        elapsed_=std::min(0.30F,elapsed_+dt);
        const float t=elapsed_/0.30F,ease=t*t*(3-2*t);
        for(std::size_t i=0;i<weights_.size();++i)weights_[i]=source_[i]*(1-ease)+(int(i)==target_?ease:0);
    }
    [[nodiscard]] float Title() const {return weights_[0];}
    /// @brief page越界时返回0；返回值为0～1，调用方据此绘制整页及其背景。
    [[nodiscard]] float Layer(int page) const {return page>=0 && page<static_cast<int>(Count)?weights_[page+1]:0;}
    [[nodiscard]] bool Visible() const {return Title()<1;}
private:
    int target_=0;
    float elapsed_=0.30F;
    std::array<float,Count+1> weights_{1},source_{1};
};
} // namespace koumakyou::ui
