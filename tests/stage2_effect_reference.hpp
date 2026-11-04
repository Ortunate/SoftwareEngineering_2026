// 本文件只在测试中解释二面102/118所在的入场、非符开场、闪光和退场窗口，不调用生产移动或效果配方。
#pragma once
#include "stage2_reference_program.hpp"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>
namespace koumakyou::stage2::reference {
/// @brief 源请求的像素位置/半径、无量纲源XYZ轴和整数ARGB；不执行粒子运动或原生全局随机数。
struct ExternalEffect {int type,count;std::uint32_t color;Vector3 position;Vector3 axis{};float radius=0;};
/// @brief 固定60Hz的有限窗口执行器；入口20截至129帧、22截至69帧、28截至60帧、39截至59帧。
/// 窗口后的攻击调用须由已有战斗差分验证，本类遇到未支持指令即拒绝，不能作为完整二面解释器。
class EffectReference {
public:
    /// @brief difficulty为0…3，sub仅接受上述四入口；point为源像素，seed为非零实例xorshift种子（零按1）。
    EffectReference(unsigned difficulty,int sub,Vector3 point,unsigned seed):position(point),difficulty_(difficulty),sub_(sub),random_(seed?seed:1) {
        if(difficulty>3||(sub!=20&&sub!=22&&sub!=28&&sub!=39))throw std::invalid_argument("Unsupported Stage 2 effect entry");
    }
    /// @brief 清空逐帧快照后推进一个内容帧；clearBullets只记录本帧请求，不改测试调用方的弹池。
    void Tick() {
        const int end=sub_==20?129:sub_==22?69:sub_==28?60:59;
        if(total_>end)throw std::logic_error("Stage 2 effect reference window exhausted");
        effects.clear();sounds.clear();clearBullets=false;
        position={position.x+velocity_.x,position.y+velocity_.y,position.z+velocity_.z};
        const auto program=ProgramsForDifficulty(difficulty_)[sub_];int budget=128;
        while(pc_<int(program.size())&&program[pc_].frame<=frame_&&--budget) {
            const auto& i=program[pc_++];const auto& a=i.args;
            switch(i.opcode) {
                case 4:if(sub_==22&&a[0]==-10012)break;
                    if(a[0]!=-10009)throw std::logic_error("Unknown effect loop register");remaining_=int(a[1]);break;
                case 3:if(a[2]!=-10009)throw std::logic_error("Unknown effect loop register");
                    if(--remaining_>0) {frame_=int(a[0]);pc_=int(a[1]);}break;
                case 43:position={a[0],a[1],a[2]};break;
                case 47:speed_=a[0];break;
                case 50: {
                    random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
                    direction_=float(random_>>8)/16777216.0F*(a[1]-a[0])+a[0];
                    constexpr float pi=std::numbers::pi_v<float>;
                    // 退场继承第一非符的32/48…352/134边界；源随机反射在插值目标求值前完成。
                    if(position.x<128&&std::abs(direction_)>pi/2)direction_=std::copysign(pi,direction_)-direction_;
                    if(position.x>256&&std::abs(direction_)<pi/2)direction_=std::copysign(pi,direction_)-direction_;
                    if(position.y<96&&direction_<0)direction_=-direction_;
                    if(position.y>86&&direction_>0)direction_=-direction_;break;
                }
                case 57:Interpolate(int(a[0]),{a[1],a[2],a[3]});break;
                case 61:Interpolate(int(a[0]),{position.x+std::cos(direction_)*speed_*a[0]/2,position.y+std::sin(direction_)*speed_*a[0]/2,position.z});break;
                case 94:clearBullets=true;break;
                case 102:
                    if(a[0]<3||a[0]>5)throw std::logic_error("Unknown Stage 2 attached palette");
                    effects.push_back({13,1,0xffffe0e0U,position,{a[1],a[2],a[3]},a[4]});break;
                case 105:damageable=a[0]!=0;break;
                case 106:sounds.push_back(int(a[0]));break;
                case 118:effects.push_back({int(a[0]),int(a[1]),i.effectColor,position});break;
                case 98:case 100:case 101:case 103:case 104:case 107:case 108:case 111:case 112:case 113:case 114:case 115:case 116:case 119:case 126:case 131:break;
                case 97:break;
                default:throw std::logic_error("Unsupported Stage 2 effect opcode "+std::to_string(i.opcode));
            }
        }
        if(!budget)throw std::logic_error("Stage 2 effect instruction budget exhausted");
        if(left_>0) {
            --left_;const float fraction=1-std::pow(float(left_)/duration_,2.0F);
            const Vector3 next{start_.x+delta_.x*fraction,start_.y+delta_.y*fraction,start_.z+delta_.z*fraction};
            velocity_={next.x-position.x,next.y-position.y,next.z-position.z};
            if(!left_) {position=next;velocity_={};}
        }
        ++frame_;++total_;
    }
    Vector3 position{};bool clearBullets=false,damageable=false;
    std::vector<ExternalEffect> effects;std::vector<int> sounds;
private:
    void Interpolate(int duration,Vector3 target) {
        start_=position;delta_={target.x-position.x,target.y-position.y,target.z-position.z};duration_=left_=duration;velocity_={};
    }
    unsigned difficulty_;int sub_,pc_=0,frame_=0,total_=0,remaining_=0,duration_=0,left_=0;
    unsigned random_;float speed_=0,direction_=0;Vector3 velocity_{},start_{},delta_{};
};
}
