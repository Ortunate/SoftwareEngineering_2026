// 本文件独立执行Extra芙兰朵露两段入场、八段非符、十符及额外三符源指令；像素/帧基准不调用生产配方。
#pragma once
#include "stage_extra_reference_program.hpp"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
namespace koumakyou::stage_extra::reference {
/// @brief 仅接受已审阅的入场/非符及39…119调用链，保留独立局部栈、阶段总钟、周期计数和激光槽位。
/// 环境请求用独立视觉随机输入验证源公式；不积分普通弹/粒子，也不模拟原生全局随机流。
class FlandreReference {
public:
    struct Emission {Vector3 position,velocity;int type,color,flags;float acceleration=0,accelerationFrames=-1;Vector3 accelerationVector{};int bounces=0;float angularVelocity=0,angularFrames=-1;};
    // variables是12个有编号的源变量，storedAngle单独保存95还会复制的+68浮点槽，不能与整数i4混用。
    struct Spawn {int sub,health,item,score;Vector3 position;std::array<float,12> variables;float storedAngle=0;};
    // 颜色使用独立ARGB字；102附加无量纲源轴与像素半径，位置为指令执行当刻的源坐标。
    struct Effect {int type,count;std::uint32_t color;Vector3 position;Vector3 axis{};float radius=0;};
    // 只供环境粒子源对照，位置/速度/加速度分别为源像素、像素/帧、像素/帧²；不读取生产世界配方。
    struct AmbientEffect {Vector3 position,velocity,acceleration;};
    // 激光以源像素保存基点/起距/终距，角度为源平面弧度；引用覆盖不删除历史光束，forcedFade记录92的独立淡出时钟。
    struct Beam {Vector3 anchor;float angle,start,end,width;int slot,grow,sustain,fade,warning,tail,age=0,forcedFade=-1;bool active=true;};
    /// @brief 位置为源像素，rank为10…32，seed非零；入场初始位置由源43覆盖。
    /// initialHealth保留未带111入口的原生命寄存器，42以其十进制位选择路线；不自动补前置阶段状态。
    FlandreReference(int sub,Vector3 point,unsigned seed,int rank,int initialHealth=0):position(point),health(initialHealth),random_(seed),rank_(rank) {
        ambientRandom_=seed;
        if((sub!=37&&sub!=38&&sub!=39&&sub!=42&&sub!=43&&sub!=44&&sub!=48&&sub!=49&&sub!=52&&sub!=53&&sub!=54&&sub!=55&&sub!=56&&sub!=59&&sub!=61&&sub!=62&&sub!=65&&sub!=66&&sub!=69&&sub!=70&&sub!=71&&sub!=72&&sub!=76&&sub!=77&&sub!=80&&sub!=81&&(sub<82||sub>119))||seed==0||rank<10||rank>32)throw std::invalid_argument("Unsupported Flandre reference");
        context_.sub=sub;boundaryEmitter_=sub==59||sub==69||sub==70||sub==105||sub==110||sub==111||(sub>=116&&sub<=118);clamped_=sub!=37&&sub!=42&&sub!=80&&sub!=90&&!boundaryEmitter_&&!(sub>=52&&sub<=54);posesEnabled_=sub!=42&&sub!=80&&sub!=90&&!boundaryEmitter_;
        handles_.fill(-1);
        if(sub>=97&&sub<=100) {clamped_=posesEnabled_=false;radius=6;}
        if(sub==105||sub==110||sub==111||(sub>=116&&sub<=118))radius=6;
        if(sub==107||sub==108||sub==109)boundsMax_.y=192;
        if(sub==112)boundsMax_.y=192;
        if((sub>=113&&sub<=115)||sub==119)boundsMax_.y=224;
        if(sub==119)poses_={160,166,166,161,162};
    }
    float Random(float range) {random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;return static_cast<float>(random_>>8)/16777216.0F*range;}
    /// @brief 复制源95出生时的父变量快照，调用前不得Tick；不共享后续写入或父随机流。
    void Inherit(const std::array<float,12>& variables,float storedAngle=0) {context_.variables=variables;storedAngle_=storedAngle;}
    /// @brief 每次推进一个60 Hz内容帧，player为当前自机源像素坐标；更新后的输出仅属于该帧，回跳不会重置周期发射计数。
    void Tick(Vector3 player) {
        if(!active)return;player_=player;
        emissions.clear();sounds.clear();drops.clear();items.clear();spawns.clear();effects.clear();ambientEffects.clear();effectRequests=0;defeatActors=false;
        // 先推进已发光束，再执行本帧ECL。淡出末帧仍可被原生14看到，之后失活；不采用生产Bullet的年龄判定。
        for(auto& beam:beams)if(beam.active) {
            ++beam.age;
            if(beam.forcedFade>=0) {if(++beam.forcedFade>beam.fade)beam.active=false;}
            else if(beam.age>beam.grow+beam.sustain+beam.fade)beam.active=false;
        }
        position.x+=velocity_.x;position.y+=velocity_.y;
        // 59/69/70/105/110/111/116…118的动画167固定使用64×80精灵；完全越出扩展矩形后直接释放槽，不执行死亡60。
        // 所有源出生点均已与可见场地相交，故首次进入条件在本入口始终成立。
        if(boundaryEmitter_&&(position.x<-32||position.x>416||position.y<-40||position.y>488)) {active=false;return;}
        if(clamped_) {position.x=std::clamp(position.x,boundsMin_.x,boundsMax_.x);position.y=std::clamp(position.y,boundsMin_.y,boundsMax_.y);}
        int budget=1024;
        while(active&&handoff<0) {
            const auto program=Programs()[context_.sub];
            if(context_.pc>=static_cast<int>(program.size())||program[context_.pc].frame>context_.frame)break;
            if(--budget==0)throw std::logic_error("Flandre source instruction budget exhausted");
            Execute(program[context_.pc++]);
        }
        // 原生主循环在ECL执行后处理死亡；最终95写HP0、death96且死亡模式0，当帧已清活动位。
        if(health<=0&&death==96&&interactable) {
            if(deathMode==3) {health=1;deathMode=0;context_={.sub=96};death=-1;}
            else active=false;
        }
        if(active&&period_>0&&++elapsed_>=period_) {elapsed_-=period_;Emit();}
        if(interpolating_) {
            --left_;const float remaining=std::max(0.0F,static_cast<float>(left_)/duration_);
            const float fraction=power_<0?std::pow(1-remaining,-power_):1-std::pow(remaining,power_);
            const Vector3 next{start_.x+delta_.x*fraction,start_.y+delta_.y*fraction,0};
            velocity_={next.x-position.x,next.y-position.y,0};
            if(left_<=0) {position=next;velocity_={};speed_=0;interpolating_=false;}
        } else velocity_={std::cos(direction_)*speed_,std::sin(direction_)*speed_,0};
        const int pose=velocity_.x<0?-2:velocity_.x>0?2:0;
        if(posesEnabled_&&pose!=pose_) {animation=pose<0?poses_[3]:pose>0?poses_[4]:pose_==-2?poses_[1]:pose_==2?poses_[2]:poses_[0];pose_=pose;}
        if(active&&handoff<0&&visible&&(nativeCallback==6||
           (health>0&&(nativeCallback==10||nativeCallback==15||nativeCallback==17||nativeCallback==19))))AmbientStep();
        ++context_.frame;++phaseFrames;
    }
    Vector3 position{};int health=0,reserve=0,timeout=0,death=-1,timeoutCallback=-1,healthCallback=-1;
    int animation=-1,auxiliaryAnimation=-1,interrupt=-1,card=-1,nativeCallback=-1;float radius=28.0F/2;bool active=true;bool damageable=true,contact=true,interactable=true,visible=true,defeatActors=false;
    std::vector<Spawn> spawns;std::vector<Emission> emissions;std::vector<int> sounds,drops,items;
    std::vector<Beam> beams;
    std::vector<Effect> effects;
    std::vector<AmbientEffect> ambientEffects;
    int effectRequests=0,deathEffect=0,dropEffect=0,deathMode=0;bool specialActor=false; // 132跳过演员绘制，但ANM自身仍更新并供回收读取尺寸。
    bool createBeams=true; // 测试显式注入原生创建器返回空；不由此推断完整Bomb/池满策略等价。
    int handoff=-1; // 前奏直接调用后继符卡时停在82/92/107/113边界；分别验证后继，不伪造整场完成。
    bool survival=false; // 源135只记录独立超时结算位，不修改受伤或隐藏位。
    int phaseFrames=0; // 独立阶段总钟，不随子调用或3/2回跳重置；原生16在7200帧切换到计算用零HP。
private:
    void AmbientStep() {
        // 直接按已核验机器码的像素公式和60Hz顺序执行；局部随机输入是项目明确的视觉适配边界。
        ambientPhase_+=0.01745329238474369F;
        if(ambientPhase_>=.7853981852531433F)ambientPhase_-=1.5707963705062866F;
        const int period=ambientClock_<=30?8:ambientClock_<=60?4:ambientClock_<=120?2:1;
        const auto next=[&] {ambientRandom_^=ambientRandom_<<13;ambientRandom_^=ambientRandom_>>17;ambientRandom_^=ambientRandom_<<5;return ambientRandom_;};
        if(ambientClock_!=ambientPrevious_&&ambientClock_%period==0) {
            const unsigned m=(ambientClock_%16)/2;
            const auto n=m+(m?(next()&65535U)%m:0);
            const float radius=n*10.0F+32;
            const float angle=ambientPhase_-n*3.1415927410125732F/40;
            const float x=std::cos(angle)*radius,y=std::sin(angle)*radius;
            for(float sign:{1.0F,-1.0F}) {
                const float u=static_cast<float>(next()>>8)/16777216.0F;
                const Vector3 velocity{(u*40-20)/60,n*8.0F/60-.2666666805744171F,0};
                ambientEffects.push_back({{position.x+sign*x,position.y+y,position.z},velocity,
                    {-velocity.x*.008333333767950535F,-velocity.y*.008333333767950535F,0}});
            }
        }
        ambientPrevious_=ambientClock_++;
    }
    std::uint32_t ambientRandom_=1;
    int ambientClock_=0,ambientPrevious_=0; // 实际出生复制清零模板；不能沿用构造器的previous=-1。
    float ambientPhase_=0;
    struct Context {int sub=0,pc=0,frame=0,compare=0;std::array<float,12> variables{};};
    float& Variable(float id) {
        if(id==-10101)return storedAngle_;
        if(id==-10015)return position.x;if(id==-10016)return position.y;if(id==-10017)return position.z;
        return context_.variables.at(static_cast<int>(-id)-10001);
    }
    float Value(float id) {
        if(id<=-10001&&id>=-10012)return Variable(id);
        if(id==-10100)return Random(6.28318548F)-3.14159274F;
        if(id==-10101)return storedAngle_;
        if(id==-10021)return player_.x==position.x&&player_.y==position.y?1.57079637F:std::atan2(player_.y-position.y,player_.x-position.x);
        if(id==-10015)return position.x;if(id==-10016)return position.y;if(id==-10017)return position.z;if(id==-10018)return player_.x;if(id==-10019)return player_.y;if(id==-10024)return health;
        return id;
    }
    void Interpolate(int duration,Vector3 target,float power=2) {
        start_=position;delta_={target.x-position.x,target.y-position.y,0};duration_=left_=duration;power_=power;velocity_={};interpolating_=true;
    }
    void Emit() {
        const auto& a=shot_.args;
        for(int layer=0;layer<static_cast<int>(a[3]);++layer)for(int n=0;n<static_cast<int>(a[2]);++n) {
            const bool aimed=shot_.opcode==67||shot_.opcode==69;
            const float aim=aimed?std::atan2(player_.y-position.y-offset_.y,player_.x-position.x-offset_.x):0;
            float angle=a[6]+aim;
            if(shot_.opcode==67||shot_.opcode==68) {
                float spread=static_cast<int>(a[2])%2?(n+1)/2*a[7]:n/2*a[7]+a[7]/2;if(n%2)spread=-spread;angle+=spread;
            } else if(shot_.opcode==75)angle=Random(a[6]-a[7])+a[7];
            else angle+=n*6.28318548F/a[2]+layer*a[7];
            const float speed=shot_.opcode==75?Random(a[4]-a[5])+a[5]:a[4]-(a[4]-a[5])*layer/a[3];
            const float accelerationAngle=accelerationAngle_<=-999?angle:accelerationAngle_;
            emissions.push_back({{position.x+offset_.x,position.y+offset_.y,0},{std::cos(angle)*speed,std::sin(angle)*speed,0},
                static_cast<int>(a[0]),static_cast<int>(a[1]),static_cast<int>(a[8]),acceleration_,accelerationFrames_,
                {std::cos(accelerationAngle)*acceleration_,std::sin(accelerationAngle)*acceleration_,0},
                (static_cast<int>(a[8])&3072)?static_cast<int>(accelerationFrames_):0,
                (static_cast<int>(a[8])&32)?accelerationAngle_:0,(static_cast<int>(a[8])&32)?accelerationFrames_:-1});
        }
        if((static_cast<int>(a[8])&512)&&sound_>=0)sounds.push_back(sound_);
    }
    void Execute(const Instruction& ins) {
        const auto& a=ins.args;
        const auto v=[&](int i){return Value(a[i]);};
        const auto jump=[&]{context_.frame=static_cast<int>(a[0]);context_.pc=static_cast<int>(a[1]);};
        switch(ins.opcode) {
            case 0:break;
            case 1:active=false;break;
            case 2:jump();break;
            case 3:if(--Variable(a[2])>0)jump();break;
            case 4:case 5:Variable(a[0])=v(1);break;
            case 13:Variable(a[0])=static_cast<int>(v(1))+static_cast<int>(v(2));break;
            case 14:Variable(a[0])=static_cast<int>(v(1))-static_cast<int>(v(2));break;
            case 16:Variable(a[0])=static_cast<int>(v(1))/static_cast<int>(v(2));break;
            case 17:Variable(a[0])=static_cast<int>(v(1))%static_cast<int>(v(2));break;
            case 6: {
                const auto bound=static_cast<unsigned>(v(1));
                if(bound==0)Variable(a[0])=0;
                else {random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;Variable(a[0])=static_cast<float>(random_%bound);}
                break;
            }
            case 9:Variable(a[0])=Random(v(1))+v(2);break;
            case 8:Variable(a[0])=Random(v(1));break;
            case 18:++Variable(a[0]);break;
            case 20:Variable(a[0])=v(1)+v(2);break;
            case 21:Variable(a[0])=v(1)-v(2);break;
            case 22:Variable(a[0])=v(1)*v(2);break;
            case 23:Variable(a[0])=v(1)/v(2);break;
            case 25:Variable(a[0])=std::atan2(v(4)-v(2),v(3)-v(1));break;
            case 26:while(Variable(a[0])>3.14159274F)Variable(a[0])-=6.28318548F;while(Variable(a[0])<-3.14159274F)Variable(a[0])+=6.28318548F;break;
            case 27:case 28:context_.compare=v(0)<v(1)?-1:v(0)>v(1)?1:0;break;
            case 30:if(context_.compare<=0)jump();break;
            case 31:if(context_.compare==0)jump();break;
            case 33:if(context_.compare>=0)jump();break;
            case 34:if(context_.compare!=0)jump();break;
            case 35:
                if((context_.sub==81&&a[0]==82)||(context_.sub==91&&a[0]==92)||(context_.sub==106&&a[0]==107)||(context_.sub==112&&a[0]==113)) {handoff=static_cast<int>(a[0]);break;}
                stack_.push_back(context_);context_.sub=static_cast<int>(a[0]);context_.pc=context_.frame=0;Variable(-10001)=a[1];Variable(-10005)=a[2];break;
            case 36:if(stack_.empty())throw std::logic_error("Unexpected Flandre return");context_=stack_.back();stack_.pop_back();break;
            case 43:position={v(0),v(1),v(2)};break;
            case 45:direction_=v(0);speed_=v(1);interpolating_=false;break;
            case 47:speed_=v(0);interpolating_=false;break;
            case 50:
                direction_=Random(a[1]-a[0])+a[0];
                if(position.x<128&&std::abs(direction_)>3.14159274F/2)direction_=std::copysign(3.14159274F,direction_)-direction_;
                if(position.x>256&&std::abs(direction_)<3.14159274F/2)direction_=std::copysign(3.14159274F,direction_)-direction_;
                if(position.y<96&&direction_<0)direction_=-direction_;
                if(position.y>72&&direction_>0)direction_=-direction_;
                interpolating_=false;break;
            case 52:case 54:
                direction_=v(1);speed_=v(2);
                Interpolate(static_cast<int>(a[0]),{position.x+std::cos(direction_)*speed_*a[0]/2,position.y+std::sin(direction_)*speed_*a[0]/2,0},ins.opcode==54?-2:2);break;
            // 原生移动分派的模式2连续三次乘r，得到r^4；58并非三次缓出。
            case 56:case 57:case 58:Interpolate(static_cast<int>(a[0]),{v(1),v(2),v(3)},ins.opcode==56?1:ins.opcode==57?2:4);break;
            case 61:Interpolate(static_cast<int>(a[0]),{position.x+std::cos(direction_)*speed_*a[0]/2,position.y+std::sin(direction_)*speed_*a[0]/2,0});break;
            case 65:
                boundsMin_={a[0],a[1],0};boundsMax_={a[2],a[3],0};clamped_=true;break;
            case 67:case 68:case 69:case 70:case 75:
                shot_=ins;for(int n=0;n<9;++n)shot_.args[n]=v(n);
                shot_.args[4]=v(4)==0?0:std::max(0.3F,v(4)+(rankDisabled_?0:-0.5F+rank_/32.0F));
                shot_.args[5]=std::max(0.3F,v(5)+(rankDisabled_?0:-0.25F+rank_/64.0F));
                if(!suppressed_)Emit();break;
            case 76:period_=static_cast<int>(a[0]);period_+=period_/5-(period_/5*2*rank_)/32;elapsed_=0;break;
            case 77:
                period_=static_cast<int>(a[0]);period_+=period_/5-(period_/5*2*rank_)/32;
                if(period_>0) {random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;elapsed_=random_%period_;}
                break;
            case 78:suppressed_=true;break;
            case 79:suppressed_=false;break;
            case 81:offset_={v(0),v(1),v(2)};break;
            case 200:{const float angle=v(0),radius=v(1);offset_={std::cos(angle)*radius,std::sin(angle)*radius,0};break;}
            case 82:acceleration_=v(4);accelerationFrames_=v(0);accelerationAngle_=v(5);break;
            case 84:sound_=static_cast<int>(a[0]);break;
            case 85:
                if(!createBeams) {handles_.at(beamSlot_)=-1;break;}
                handles_.at(beamSlot_)=static_cast<int>(beams.size());
                beams.push_back({{position.x+offset_.x,position.y+offset_.y,0},v(2),v(4),v(6),v(7),beamSlot_,
                    static_cast<int>(a[8]),static_cast<int>(a[9]),static_cast<int>(a[10]),static_cast<int>(a[11]),static_cast<int>(a[12])});break;
            case 87:beamSlot_=static_cast<int>(a[0]);break;
            case 88:if(auto* b=CurrentBeam(static_cast<int>(a[0])))b->angle+=v(1);break;
            case 90:if(auto* b=CurrentBeam(static_cast<int>(a[0])))b->anchor={position.x+a[1],position.y+a[2],position.z+a[3]};break;
            case 92:if(auto* b=CurrentBeam(static_cast<int>(a[0]));b&&b->forcedFade<0)b->forcedFade=0;break;
            case 93:card=static_cast<int>(a[1])+1;break;
            case 95:spawns.push_back({static_cast<int>(a[0]),static_cast<int>(v(4)),static_cast<int>(a[5]),static_cast<int>(a[6]),{v(1),v(2),v(3)},context_.variables,storedAngle_});break;
            case 96:defeatActors=true;break;
            case 97:animation=static_cast<int>(a[0]);interrupt=-1;break;
            case 98:for(int n=0;n<5;++n)poses_[n]=static_cast<int>(a[n]);pose_=99;posesEnabled_=poses_[3]>=0;break;
            case 99:if(a[0]!=0||a[1]!=18)throw std::logic_error("Unexpected Flandre auxiliary");auxiliaryAnimation=18;break;
            case 100:deathEffect=static_cast<int>(a[0])&255;dropEffect=(static_cast<int>(a[0])>>8)&255;break;
            case 103:radius=std::min(a[0],a[1])/2;break;
            case 104:contact=a[0]!=0;break;
            case 105:damageable=a[0]!=0;break;
            case 106:sounds.push_back(static_cast<int>(a[0]));break;
            case 107:deathMode=static_cast<int>(a[0]);break;
            case 108:death=static_cast<int>(a[0]);break;
            case 111:health=static_cast<int>(a[0]);break;
            case 113:reserve=static_cast<int>(a[0]);break;
            case 114:healthCallback=static_cast<int>(a[0]);break;
            case 115:timeout=static_cast<int>(a[0]);break;
            case 116:timeoutCallback=static_cast<int>(a[0]);break;
            case 117:interactable=a[0]!=0;break;
            case 118:effectRequests+=static_cast<int>(a[1]);effects.push_back({static_cast<int>(a[0]),static_cast<int>(a[1]),ins.effectColor,position});break;
            case 119:drops.push_back(static_cast<int>(a[0]));break;
            case 124:items.push_back(static_cast<int>(a[0]));break;
            // Extra的102均查调色表第11项；不从生产RGBA或世界轴反推参考值。
            case 102:
                if(a[0]!=11)throw std::logic_error("Unsupported Flandre attached palette");
                effects.push_back({13,1,0xffe0e0ffU,position,{a[1],a[2],a[3]},a[4]});break;
            // 122的6/10/15/17/19由步末独立环境公式处理。
            // Bomb响应另按真实EXE与独立时钟验证，局部视觉输入不宣称原生全局随机流等价。
            case 121:
                if(a[0]==14) {
                    Variable(-10004)=0;const auto saved=position,oldOffset=offset_;offset_={};
                    for(int slot=0;slot<8;++slot)if(const auto* b=CurrentBeam(slot)) {
                        for(float distance=b->start;distance<b->end;distance+=48) {
                            position={b->anchor.x+std::cos(b->angle)*distance,b->anchor.y+std::sin(b->angle)*distance,0};Emit();
                        }
                        ++Variable(-10004);
                    }
                    position=saved;offset_=oldOffset;
                } else if(a[0]==16) {
                    const int hp=phaseFrames<7200?health:0;
                    if(a[1]==0) {Variable(-10008)=2-static_cast<float>(hp)/6000;Variable(-10010)=40+hp*240/6000;}
                    else {
                        const float width=320-static_cast<float>(hp)*160/6000,height=128-static_cast<float>(hp)*64/6000;
                        Variable(-10007)=192-width*.5F+Random(width);Variable(-10008)=96-height*.5F+Random(height);
                    }
                } else if(a[0]==18) {
                    const int hp=phaseFrames<7200?health:0;
                    Variable(-10005)=0.006F-static_cast<float>(hp)*0.001F/6000;
                    Variable(-10002)=22+hp*25/6000;Variable(-10003)=370+hp*350/6000;Variable(-10004)=400-hp*250/6000;
                } else nativeCallback=static_cast<int>(a[0]);break;
            case 122:if(a[0]!=-1&&a[0]!=6&&a[0]!=10&&a[0]!=15&&a[0]!=17&&a[0]!=19)throw std::logic_error("Unsupported Flandre extension");nativeCallback=static_cast<int>(a[0]);break;
            case 128:interrupt=static_cast<int>(a[0]);break;
            case 131:
                for(int n=0;n<6;++n)if(a[n]!=0)throw std::logic_error("Unsupported Flandre rank override");rankDisabled_=true;break;
            case 132:specialActor=a[0]!=0;visible=!specialActor;break;
            case 133:timeoutCallback=death;break;
            // 135的0x10位由原生超时路径消费，独立于105/117/132；此处保留状态供结算边界断言。
            case 135:survival=a[0]!=0;break;
            case 94:rankDisabled_=false;break;
            case 101:case 109:case 112:case 126:break;
            default:throw std::logic_error("Unsupported Flandre source opcode "+std::to_string(ins.opcode));
        }
    }
    Beam* CurrentBeam(int slot) {
        const int handle=handles_.at(slot);return handle>=0&&beams.at(handle).active?&beams[handle]:nullptr;
    }
    int rank_,period_=0,elapsed_=0,sound_=-1,pose_=99,duration_=0,left_=0;
    unsigned random_;Context context_;std::vector<Context> stack_;std::array<int,5> poses_{160,163,164,161,162};
    Vector3 velocity_{},start_{},delta_{},offset_{},player_{},boundsMin_{32,48,0},boundsMax_{352,120,0};float direction_=0,speed_=0,acceleration_=0,accelerationFrames_=-1,accelerationAngle_=0,power_=2,storedAngle_=0;
    std::array<int,8> handles_{};int beamSlot_=0;
    bool clamped_=false,suppressed_=false,interpolating_=false,posesEnabled_=false,boundaryEmitter_=false,rankDisabled_=false;Instruction shot_{};
};
}
