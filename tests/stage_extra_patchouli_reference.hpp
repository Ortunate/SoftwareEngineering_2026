// 本文件独立执行Extra帕秋莉三符、使魔源程序及原生13，仅供像素单位差分，不调用生产行为。
#pragma once
#include "stage_extra_reference_program.hpp"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace koumakyou::stage_extra::reference {
/// @brief 仅接受Sub17…19与31…35，包含准备和完整循环；位置为源像素，速度为像素/60 Hz帧。
/// 发射只保存出生快照；不积分弹体或模拟原生全局随机流，生命回调与辅助ANM另行验收。
class PatchouliReference {
public:
    /// @brief 加速度为源像素/帧²，accelerationFrames为显现结束后实际加速的内容帧数。
    struct Emission {
        Vector3 position,velocity,acceleration;int type,color,flags;float accelerationFrames;
        int turnCount=1;float turnDelay=0,turnAngle=0,turnSpeed=0;
    };
    /// @brief 源95出生快照，生命采用已核验的负短载荷保留模板规则，variables复制父当前上下文。
    struct Spawn {int sub,health,item,score;Vector3 position;std::array<float,12> variables;};
    /// @brief sub仅17…19/31…35，seed非零，rank为10…32；其他参数拒绝，不提供四种Extra难度。
    PatchouliReference(int sub,Vector3 point,unsigned seed,int rank):position(point),random_(seed),rank_(rank) {
        if(!((sub>=17&&sub<=19)||(sub>=31&&sub<=35))||seed==0||rank<10||rank>32)throw std::invalid_argument("Unsupported Extra Patchouli reference");
        context_.sub=sub;clamped_=posesEnabled_=sub<=19;
        if(sub>=31)health=1; // 原生95的负短生命载荷保留模板生命，不采用反编译显示的-5536。
    }
    /// @brief 使魔转入源登记的Sub36击破后继，保留发射/运动上下文；其他入口拒绝，不执行本体阶段回调。
    void Defeat() {
        if(death!=36)throw std::logic_error("Extra reference has no stone death callback");
        context_.sub=death;context_.pc=context_.frame=0;stack_.clear();
    }
    /// @brief 消费实例xorshift，返回[0,range)；range可负，用于源随机速率的反向区间。
    float Random(float range) {random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;return static_cast<float>(random_>>8)/16777216.0F*range;}
    /// @brief 推进一个源内容帧并清本帧输出；player为源像素，调用期间父上下文时钟冻结。
    void Tick(Vector3 player) {
        emissions.clear();sounds.clear();drops.clear();spawns.clear();defeatActors=false;player_=player;
        if(!active)return;
        position.x+=velocity_.x;position.y+=velocity_.y;
        if(clamped_) {position.x=std::clamp(position.x,32.0F,352.0F);position.y=std::clamp(position.y,48.0F,120.0F);}
        int budget=4096;
        while(active&&--budget) {
            const auto program=Programs()[context_.sub];
            if(context_.pc>=static_cast<int>(program.size())||program[context_.pc].frame>context_.frame)break;
            Execute(program[context_.pc++],player);
        }
        if(!budget)throw std::logic_error("Extra Patchouli reference instruction budget exhausted");
        if(interpolating_) {
            --left_;const float fraction=1-std::pow(std::max(0.0F,static_cast<float>(left_)/duration_),2.0F);
            const Vector3 next{start_.x+delta_.x*fraction,start_.y+delta_.y*fraction,0};
            velocity_={next.x-position.x,next.y-position.y,0};
            if(left_<=0) {position=next;velocity_={};speed_=0;interpolating_=false;}
        } else velocity_={std::cos(direction_)*speed_,std::sin(direction_)*speed_,0};
        const int pose=velocity_.x<0?-2:velocity_.x>0?2:0;
        if(posesEnabled_&&pose!=pose_) {animation=pose<0?poses_[3]:pose>0?poses_[4]:pose_==-2?poses_[1]:pose_==2?poses_[2]:poses_[0];pose_=pose;}
        ++context_.frame;
    }
    Vector3 position{};int health=0,timeout=0,card=-1,animation=-1,death=-1,timeoutCallback=-1,interrupt=-1,auxiliaryAnimation=-1,auxiliaryInterrupt=-1;
    float radius=0;bool active=true,damageable=true,interactable=true,defeatActors=false;
    std::vector<Emission> emissions;std::vector<int> sounds,drops;
    std::vector<Spawn> spawns;
private:
    struct Context {int sub=0,pc=0,frame=0,compare=0;std::array<float,12> variables{};};
    float& Variable(float id) {return context_.variables.at(static_cast<int>(-id)-10001);}
    float Value(float id) const {
        if(id<=-10001&&id>=-10012)return context_.variables.at(static_cast<int>(-id)-10001);
        if(id==-10015)return position.x;if(id==-10016)return position.y;
        if(id==-10021)return std::atan2(player_.y-position.y,player_.x-position.x);return id;
    }
    void Interpolate(int frames,Vector3 target) {
        start_=position;delta_={target.x-position.x,target.y-position.y,0};duration_=left_=frames;velocity_={};interpolating_=true;
    }
    void Emit(Vector3 origin) {
        const auto& a=shot_.args;const int count=static_cast<int>(a[2]),layers=static_cast<int>(a[3]);
        for(int layer=0;layer<layers;++layer)for(int n=0;n<count;++n) {
            float angle=a[6],speed=a[4]-(a[4]-a[5])*layer/layers;
            if(shot_.opcode==75) {angle=Random(a[6]-a[7])+a[7];speed=Random(a[4]-a[5])+a[5];}
            else if(shot_.opcode==67||shot_.opcode==68) {
                float spread=count%2?(n+1)/2*a[7]:n/2*a[7]+a[7]/2;if(n%2)spread=-spread;angle+=spread;
                if(shot_.opcode==67)angle+=std::atan2(player_.y-origin.y,player_.x-origin.x);
            } else if(shot_.opcode==70)angle+=n*6.28318548F/count+layer*a[7];
            else throw std::logic_error("Unsupported Extra Patchouli formation");
            const bool accelerates=static_cast<int>(a[8])&16;
            const float accelerationAngle=accelerationAngle_==-999?angle:accelerationAngle_;
            emissions.push_back({origin,{std::cos(angle)*speed,std::sin(angle)*speed,0},
                accelerates?Vector3{std::cos(accelerationAngle)*acceleration_,std::sin(accelerationAngle)*acceleration_,0}:Vector3{},
                static_cast<int>(a[0]),static_cast<int>(a[1]),static_cast<int>(a[8]),accelerates?accelerationFrames_:-1});
            if(static_cast<int>(a[8])&(64|128|256)) {
                auto& emitted=emissions.back();emitted.turnCount=turnCount_;emitted.turnDelay=accelerationFrames_;
                emitted.turnAngle=acceleration_;emitted.turnSpeed=accelerationAngle_;
            }
        }
        if((static_cast<int>(a[8])&512)&&sound_>=0)sounds.push_back(sound_);
    }
    void Execute(const Instruction& ins,Vector3) {
        const auto& a=ins.args;const auto v=[&](int n){return Value(a[n]);};
        const auto jump=[&]{context_.frame=static_cast<int>(a[0]);context_.pc=static_cast<int>(a[1]);};
        switch(ins.opcode) {
            case 1:active=false;break;
            case 2:jump();break;
            case 3:if(--Variable(a[2])>0)jump();break;
            case 4:case 5:Variable(a[0])=v(1);break;
            case 8:Variable(a[0])=Random(v(1));break;
            case 9:Variable(a[0])=Random(v(1))+v(2);break;
            case 13:Variable(a[0])=static_cast<int>(v(1))+static_cast<int>(v(2));break;
            case 16:Variable(a[0])=static_cast<int>(v(1))/static_cast<int>(v(2));break;
            case 17:Variable(a[0])=static_cast<int>(v(1))%static_cast<int>(v(2));break;
            case 18:++Variable(a[0]);break;
            case 20:Variable(a[0])=v(1)+v(2);break;
            case 21:Variable(a[0])=v(1)-v(2);break;
            case 27:context_.compare=v(0)<v(1)?-1:v(0)>v(1)?1:0;break;
            case 34:if(context_.compare!=0)jump();break;
            case 35:stack_.push_back(context_);context_.sub=static_cast<int>(a[0]);context_.pc=context_.frame=0;Variable(-10001)=a[1];Variable(-10005)=a[2];break;
            case 36:if(stack_.empty())throw std::logic_error("Unexpected Extra return");context_=stack_.back();stack_.pop_back();break;
            case 47:speed_=v(0);interpolating_=false;break;
            case 50:
                direction_=Random(a[1]-a[0])+a[0];
                if(position.x<128&&std::abs(direction_)>3.14159274F/2)direction_=std::copysign(3.14159274F,direction_)-direction_;
                if(position.x>256&&std::abs(direction_)<3.14159274F/2)direction_=std::copysign(3.14159274F,direction_)-direction_;
                if(position.y<96&&direction_<0)direction_=-direction_;
                if(position.y>72&&direction_>0)direction_=-direction_;
                interpolating_=false;break;
            case 57:Interpolate(static_cast<int>(a[0]),{v(1),v(2),v(3)});break;
            case 61:Interpolate(static_cast<int>(a[0]),{position.x+std::cos(direction_)*speed_*a[0]/2,position.y+std::sin(direction_)*speed_*a[0]/2,0});break;
            case 65:
                if(a[0]!=32||a[1]!=48||a[2]!=352||a[3]!=120)throw std::logic_error("Unexpected Extra Patchouli bounds");
                clamped_=true;break;
            case 67:case 68:case 70:case 75:
                shot_=ins;for(int n=0;n<9;++n)shot_.args[n]=v(n);
                shot_.args[4]=v(4)==0?0:std::max(0.3F,v(4)-0.5F+rank_/32.0F);
                shot_.args[5]=std::max(0.3F,v(5)-0.25F+rank_/64.0F);
                if(!suppressed_)Emit({position.x+offset_.x,position.y+offset_.y,0});break;
            case 76:if(a[0]!=0)throw std::logic_error("Unexpected Extra periodic fire");break;
            case 78:suppressed_=true;break;
            case 79:suppressed_=false;break;
            case 81:offset_={v(0),v(1),v(2)};break;
            case 82:acceleration_=v(4);accelerationFrames_=v(0);accelerationAngle_=v(5);turnCount_=static_cast<int>(v(1));break;
            case 84:sound_=static_cast<int>(a[0]);break;
            case 93:card=static_cast<int>(a[1]);break;
            case 95:spawns.push_back({static_cast<int>(a[0]),v(4)<0?1:static_cast<int>(v(4)),static_cast<int>(a[5]),static_cast<int>(a[6]),{v(1),v(2),v(3)},context_.variables});break;
            case 96:defeatActors=true;break;
            case 97:animation=static_cast<int>(a[0]);interrupt=-1;break;
            case 98:for(int n=0;n<5;++n)poses_[n]=static_cast<int>(a[n]);pose_=-1;posesEnabled_=true;break;
            case 99:auxiliaryAnimation=static_cast<int>(a[1]);break;
            case 103:radius=std::min(a[0],a[1])/2;break;
            case 105:damageable=a[0]!=0;break;
            case 106:sounds.push_back(static_cast<int>(a[0]));break;
            case 108:death=static_cast<int>(a[0]);break;
            case 111:health=static_cast<int>(a[0]);break;
            case 115:timeout=static_cast<int>(a[0]);break;
            case 117:interactable=a[0]!=0;break;
            case 119:drops.push_back(static_cast<int>(a[0]));break;
            case 128:interrupt=static_cast<int>(a[0]);break;
            case 129:auxiliaryInterrupt=static_cast<int>(a[1]);break;
            case 133:timeoutCallback=death;break;
            case 121: {
                if(a[0]!=13)throw std::logic_error("Unsupported Extra native extension");
                int counter=static_cast<int>(Variable(-10004));
                if(counter%6==0) {
                    float angle=Variable(-10007);const int directions=static_cast<int>(a[1]);
                    const float step=6.28318548F/directions;
                    for(int n=0;n<directions;++n) {
                        // 原生复制当前配方后覆盖位置与角度，不改变演员持有的下一次发射配置。
                        const float saved=shot_.args[6];shot_.args[6]=angle+Variable(-10006);
                        Emit({192+std::cos(angle)*Variable(-10008),224+std::sin(angle)*Variable(-10008),0});
                        shot_.args[6]=saved;angle+=step;
                    }
                }
                Variable(-10004)=counter+1;break;
            }
            // 本体生命周期、清弹与辅助槽另验；这些指令不改变被比较的源出生快照或运动。
            case 83:case 94:case 100:case 101:case 104:case 107:case 112:case 126:break;
            default:throw std::logic_error("Unsupported Extra Patchouli opcode "+std::to_string(ins.opcode));
        }
    }
    Context context_;std::vector<Context> stack_;unsigned random_;int rank_,sound_=-1,pose_=-1,duration_=0,left_=0;
    std::array<int,5> poses_{65,69,70,67,68};
    bool suppressed_=false,interpolating_=false,clamped_=false,posesEnabled_=false;
    float direction_=0,speed_=0,acceleration_=0,accelerationFrames_=-1,accelerationAngle_=-999;int turnCount_=1;
    Vector3 velocity_{},offset_{},start_{},delta_{},player_{};Instruction shot_{};
};
}
