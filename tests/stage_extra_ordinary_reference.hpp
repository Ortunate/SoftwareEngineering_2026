// 本文件独立执行Extra已审阅的普通演员源程序，保留源单位，仅供差分测试。
#pragma once
#include "stage_extra_reference_program.hpp"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace koumakyou::stage_extra::reference {
/// @brief 仅接受Sub0…14，Sub15/36仅由击破回调进入；位置/发射偏移为源像素，速率为像素/60 Hz内容帧。
/// 独立维护源时序、rank修正、发射和项目xorshift；不调用生产配方，不积分弹体或模拟原生全局随机流。
class OrdinaryReference {
public:
    /// @brief 单次源发射快照，速度为源像素/内容帧；flags保留原始显现选项。
    struct Emission {Vector3 position,velocity;int type,color,flags;};
    /// @brief seed须非零、rank为10…32；mirror仅反转最终X位移。其他入口或非法参数抛invalid_argument。
    OrdinaryReference(int sub,Vector3 point,unsigned seed,int rank,bool mirror)
        :position(point),sub_(sub),rank_(rank),random_(seed),mirror_(mirror) {
        if(sub<0||sub>14||seed==0||rank<10||rank>32)
            throw std::invalid_argument("Unsupported Extra ordinary reference entry");
    }
    /// @brief 转入源程序登记的击破后继，保留运动与随机流；无后继时拒绝调用。
    void Defeat() {
        if(death<0)throw std::logic_error("Extra ordinary has no death callback");
        sub_=death;pc_=frame_=period_=0;damageable=false;
    }
    /// @brief 推进实例xorshift并返回[0,1)乘有限range；用于精确核对随机读取顺序。
    float Random(float range) {return static_cast<float>(Next()>>8)/16777216.0F*range;}
    /// @brief 前进一个源内容帧并清空本帧发射与音效；player为源像素，先应用上一帧位移。
    void Tick(Vector3 player) {
        emissions.clear();sounds.clear();drops.clear();
        if(!active)return;
        position.x+=vx_*(mirror_?-1:1);position.y+=vy_;
        const auto program=Programs()[sub_];
        int budget=4096;
        while(active&&pc_<program.size()&&program[pc_].frame<=frame_&&--budget) {
            const auto& instruction=program[pc_++];const auto& a=instruction.args;
            const auto value=[&](int n) {return Value(a[n]);};
            const auto jump=[&] {frame_=static_cast<int>(a[0]);pc_=static_cast<std::size_t>(a[1]);};
            switch(instruction.opcode) {
                case 1:active=false;break;
                case 3:if(--Variable(a[2])>0)jump();break;
                case 4:case 5:Variable(a[0])=value(1);break;
                case 9:Variable(a[0])=Random(value(1))+value(2);break;
                case 13:Variable(a[0])=value(1)+value(2);break;
                case 20:Variable(a[0])=value(1)+value(2);break;
                case 21:Variable(a[0])=value(1)-value(2);break;
                case 45:angle_=a[0];speed_=a[1];break;
                case 46:angular_=a[0];break;
                case 47:speed_=a[0];break;
                case 48:acceleration_=a[0];break;
                case 67:case 70:case 75: {
                    shotOpcode_=instruction.opcode;shot_=a;
                    for(int n=0;n<9;++n)shot_[n]=value(n);
                    const float influence=-0.5F+rank_/32.0F;
                    shot_[4]=std::max(0.3F,value(4)+influence);shot_[5]=std::max(0.3F,value(5)+influence/2);
                    if(!suppressed_)Emit(player);
                    break;
                }
                case 77: {
                    const int base=static_cast<int>(a[0]);period_=base+base/5-(base/5*2*rank_)/32;
                    elapsed_=period_>0 ? static_cast<int>(Next()%static_cast<unsigned>(period_)) : 0;
                    break;
                }
                case 78:suppressed_=true;break;
                case 79:suppressed_=false;break;
                case 81:offset_={a[0],a[1],a[2]};break;
                case 84:sound_=static_cast<int>(a[0]);break;
                case 97:animation=static_cast<int>(a[0]);interrupt=-1;break;
                case 99:auxiliaryAnimation=static_cast<int>(a[1]);break;
                case 100:animationFlags=static_cast<int>(a[0]);break;
                case 103:radius=std::min(a[0],a[1])/2;break;
                case 106:sounds.push_back(static_cast<int>(a[0]));break;
                case 105:damageable=a[0]!=0;break;
                case 107:break; // 本组入口均选择唯一击破回调槽；不改变发射或运动。
                case 108:death=static_cast<int>(a[0]);break;
                case 119:drops.push_back(static_cast<int>(a[0]));break;
                case 117:interactable=a[0]!=0;break;
                case 128:interrupt=static_cast<int>(a[0]);break;
                case 129:auxiliaryInterrupt=static_cast<int>(a[1]);break;
                default:throw std::logic_error("Unsupported Extra ordinary opcode "+std::to_string(instruction.opcode));
            }
        }
        if(!budget)throw std::logic_error("Extra ordinary instruction budget exhausted");
        if(active&&period_>0&&++elapsed_>=period_) {elapsed_-=period_;Emit(player);}
        angle_+=angular_;speed_+=acceleration_;vx_=std::cos(angle_)*speed_;vy_=std::sin(angle_)*speed_;++frame_;
    }
    Vector3 position{};bool active=true,damageable=true,interactable=true;
    int animation=-1,animationFlags=0,death=-1,interrupt=-1;
    // 辅助槽独立保留源事实，当前生产普通演员尚未渲染该槽，不能把本体ANM比较当作辅助槽验收。
    int auxiliaryAnimation=-1,auxiliaryInterrupt=-1;float radius=0;
    std::vector<Emission> emissions;std::vector<int> sounds,drops;
private:
    float& Variable(float id) {return variables_.at(static_cast<int>(-id)-10001);}
    float Value(float id) const {
        return id<=-10001&&id>=-10012 ? variables_.at(static_cast<int>(-id)-10001) : id;
    }
    unsigned Next() {random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;return random_;}
    void Emit(Vector3 player) {
        const int count=static_cast<int>(shot_[2]),layers=static_cast<int>(shot_[3]);
        const Vector3 origin{position.x+offset_.x,position.y+offset_.y,position.z+offset_.z};
        const float aim=std::atan2(player.y-origin.y,player.x-origin.x);
        for(int layer=0;layer<layers;++layer)for(int n=0;n<count;++n) {
            float spread=count%2 ? (n+1)/2*shot_[7] : n/2*shot_[7]+shot_[7]/2;
            if(n%2)spread=-spread;
            const float angle=shotOpcode_==67 ? shot_[6]+spread+aim : shotOpcode_==75 ? Random(shot_[6]-shot_[7])+shot_[7] : shot_[6]+n*6.28318548F/count+layer*shot_[7];
            const float speed=shotOpcode_==75 ? Random(shot_[4]-shot_[5])+shot_[5] : shot_[4]-(shot_[4]-shot_[5])*layer/layers;
            emissions.push_back({origin,{std::cos(angle)*speed,std::sin(angle)*speed,0},
                static_cast<int>(shot_[0]),static_cast<int>(shot_[1]),static_cast<int>(shot_[8])});
        }
        if((static_cast<int>(shot_[8])&512)&&sound_>=0)sounds.push_back(sound_);
    }
    int sub_,rank_,frame_=0,period_=0,elapsed_=0;std::size_t pc_=0;unsigned random_;
    bool mirror_,suppressed_=false;
    float angle_=0,speed_=0,angular_=0,acceleration_=0,vx_=0,vy_=0;
    int shotOpcode_=67,sound_=-1;
    Vector3 offset_{};std::array<float,16> shot_{};std::array<float,12> variables_{};
};
}
