// 本文件执行六面普通演员已审阅的源指令，仅供独立差分测试。
#pragma once
#include "stage6_reference_program.hpp"
#include "stage6_effect_reference.hpp"
#include "raylib.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>
namespace koumakyou::stage6::reference {
namespace ordinary_detail {
inline constexpr float pi=std::numbers::pi_v<float>;
inline unsigned Next(unsigned& state) {state^=state<<13;state^=state>>17;state^=state<<5;return state;}
}
/// @brief 只执行六面普通演员可达的源指令；位置为像素，速率为像素/帧，帧为 60 Hz 局部时钟。
/// 独立维护发射、rank 快照和项目 xorshift；不调用生产移动/弹幕配方，也不代表原生全局 RNG。
class OrdinaryReference {
public:
    /// @brief 单帧新生弹快照；位置为源像素，速度为源像素/帧。
    struct Emission {Vector3 position,velocity;int type,color;};
    /// @brief difficulty 为 E/N/H/L 的 0…3，入口为 Sub0…5 或 Sub7；rank 为 10…32，seed 非零。
    /// mirror 只反转最终 X 位移，不反转发射方向；记录118请求，不执行粒子或弹体积分。
    OrdinaryReference(int difficulty,int sub,Vector3 point,unsigned seed,int rank,bool mirror)
        :position(point),difficulty_(difficulty),sub_(sub),rank_(rank),random_(seed),mirror_(mirror) {
        if(difficulty<0||difficulty>3||sub<0||sub>7||sub==6||rank<10||rank>32||seed==0)
            throw std::invalid_argument("Invalid Stage 6 ordinary reference entry");
    }
    /// @brief 消费一次项目实例随机流，返回 [0,range)；调用方用下一随机值精确检查读取顺序。
    float Random(float range) {return static_cast<float>(ordinary_detail::Next(random_)>>8)/16777216.0F*range;}
    /// @brief 转入已登记的死亡后继，保留运动和随机流；仅用于带 Sub6 死亡回调的蓝环。
    void Defeat() {assert(death>=0);sub_=death;pc_=frame_=period_=0;}
    /// @brief 推进一帧并重置本帧输出；player 为源像素，回拨局部时钟不回拨运动状态。
    void Tick(Vector3 player) {
        emissions.clear();sounds.clear();drops.clear();effects.clear();position.x+=vx_*(mirror_?-1:1);position.y+=vy_;
        const auto program=ProgramsForDifficulty(difficulty_)[sub_];int budget=4096;
        while(active&&pc_<int(program.size())&&program[pc_].frame<=frame_&&--budget) {
            const auto& ins=program[pc_++];const auto& a=ins.args;
            const auto v=[&](int n){return Value(a[n]);};
            const auto jump=[&]{frame_=int(a[0]);pc_=int(a[1]);};
            switch(ins.opcode) {
                case 1:active=false;break;
                case 2:jump();break;
                case 3:if(--Variable(a[2])>0)jump();break;
                case 4:case 5:Variable(a[0])=v(1);break;
                case 8:Variable(a[0])=Random(v(1));break;
                case 9:Variable(a[0])=Random(v(1))+v(2);break;
                case 45:angle_=v(0);speed_=v(1);break;
                case 46:angular_=v(0);break;
                case 48:acceleration_=v(0);break;
                case 69:case 75: {
                    shot_=ins;for(int n=0;n<9;++n)shot_.args[n]=v(n);
                    const float influence=-0.5F+rank_/32.0F;
                    shot_.args[4]=std::max(0.3F,v(4)+influence);shot_.args[5]=std::max(0.3F,v(5)+influence/2);
                    if(!suppressed_)Emit(player);break;
                }
                case 77:
                    period_=int(a[0]);period_+=period_/5-(period_/5*2*rank_)/32;
                    elapsed_=period_>0 ? int(ordinary_detail::Next(random_)%static_cast<unsigned>(period_)) : 0;break;
                case 78:suppressed_=true;break;
                case 79:suppressed_=false;break;
                case 106:sounds.push_back(int(a[0]));break;
                case 97:animation=int(a[0]);break;
                case 103:radius=std::min(a[0],a[1])/2;break;
                case 107:break; // 回调槽选择不影响普通演员仅有的一条死亡后继。
                case 108:death=int(a[0]);break;
                case 117:interactable=int(a[0])!=0;break;
                case 118:effects.push_back({int(a[0]),int(a[1]),ins.effectColor,position});break;
                case 119:drops.push_back(int(a[0]));break;
                case 132:visible=int(a[0])==0;break;
                default:throw std::logic_error("Unsupported Stage 6 ordinary opcode "+std::to_string(ins.opcode));
            }
        }
        assert(budget>0);if(active&&period_>0&&++elapsed_>=period_) {elapsed_-=period_;Emit(player);}
        angle_+=angular_;speed_+=acceleration_;vx_=std::cos(angle_)*speed_;vy_=std::sin(angle_)*speed_;++frame_;
    }
    Vector3 position{};bool active=true,visible=true,interactable=true;int animation=-1,death=-1;float radius=0;
    std::vector<Emission> emissions;std::vector<int> sounds,drops;
    std::vector<ExternalEffect> effects;
private:
    float& Variable(float id) {
        if(id==-10015)return position.x;if(id==-10016)return position.y;
        return variables_.at(int(-id)-10001);
    }
    float Value(float id) const {
        if(id<=-10001&&id>=-10012)return variables_.at(int(-id)-10001);
        if(id==-10013)return float(difficulty_);if(id==-10015)return position.x;if(id==-10016)return position.y;
        return id;
    }
    void Emit(Vector3 player) {
        const auto& a=shot_.args;const int count=int(a[2]),layers=int(a[3]);
        for(int layer=0;layer<layers;++layer)for(int n=0;n<count;++n) {
            float angle=a[6],speed=a[4]-(a[4]-a[5])*layer/layers;
            if(shot_.opcode==69)angle+=n*2*ordinary_detail::pi/count+layer*a[7];
            else if(shot_.opcode==75) {angle=Random(a[6]-a[7])+a[7];speed=Random(a[4]-a[5])+a[5];}
            else throw std::logic_error("Unsupported Stage 6 ordinary formation");
            if(shot_.opcode==69)angle+=std::atan2(player.y-position.y,player.x-position.x);
            emissions.push_back({position,{std::cos(angle)*speed,std::sin(angle)*speed,0},int(a[0]),int(a[1])});
        }
    }
    int difficulty_,sub_,rank_,pc_=0,frame_=0,period_=0,elapsed_=0;unsigned random_;
    bool mirror_,suppressed_=false;float speed_=0,angle_=0,acceleration_=0,angular_=0,vx_=0,vy_=0;
    std::array<float,12> variables_{};Instruction shot_{};
};
}
