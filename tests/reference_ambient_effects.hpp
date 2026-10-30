// 本文件独立执行原生6的60Hz像素公式；仅供源差分，不调用生产环境发射器或世界坐标算法。
#pragma once
#include "raylib.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace koumakyou::reference {
/// @brief 单个原生19环境请求；位置为源像素，速度为像素/帧，加速度为像素/帧²。
struct AmbientEffect {Vector3 position,velocity,acceleration;};
/// @brief 原生6及生命保护包装10的独立状态；clock/previous/phase依实际出生模板从零开始。
/// seed=0按1处理；视觉xorshift是项目适配输入，不证明原生全局随机读取交织等价。
class AmbientReference {
public:
    explicit AmbientReference(std::uint32_t seed):random_(seed?seed:1) {}
    /// @brief 由调用方在源行为步末传入源像素位置与实际122身份；仅6或正生命的10推进。
    /// 隐藏/未安装/生命保护期间冻结时钟与随机流；技能交接不得重新构造本实例。
    [[nodiscard]] std::vector<AmbientEffect> Step(int callback,int health,bool visible,Vector3 position) {
        std::vector<AmbientEffect> result;
        if(!visible||(callback!=6&&(callback!=10||health<=0)))return result;
        phase_+=0.01745329238474369F;
        if(phase_>=.7853981852531433F)phase_-=1.5707963705062866F;
        const int period=clock_<=30?8:clock_<=60?4:clock_<=120?2:1;
        if(clock_!=previous_&&clock_%period==0) {
            const unsigned bound=(clock_%16)/2;
            const unsigned n=bound+(bound?(Next()&65535U)%bound:0);
            const float radius=n*10.0F+32,angle=phase_-n*3.1415927410125732F/40;
            const float x=std::cos(angle)*radius,y=std::sin(angle)*radius;
            for(float sign:{1.0F,-1.0F}) {
                const float u=static_cast<float>(Next()>>8)/16777216.0F;
                const Vector3 velocity{(u*40-20)/60,n*8.0F/60-.2666666805744171F,0};
                result.push_back({{position.x+sign*x,position.y+y,position.z},velocity,
                    {-velocity.x*.008333333767950535F,-velocity.y*.008333333767950535F,0}});
            }
        }
        previous_=clock_++;
        return result;
    }
private:
    std::uint32_t Next() {random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;return random_;}
    std::uint32_t random_;
    int clock_=0,previous_=0;
    float phase_=0;
};
} // namespace koumakyou::reference
