// 本文件直接执行四面独立 ECL 夹具的道中/非符/元素及组合符卡语义，不调用生产移动、弹幕或行为配方。
#include "stage4_combat_reference.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace koumakyou::stage4::reference {
namespace {
constexpr float pi=std::numbers::pi_v<float>;
Vector3 Add(Vector3 a,Vector3 b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vector3 Difference(Vector3 a,Vector3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vector3 Scale(Vector3 a,float b) {return {a.x*b,a.y*b,a.z*b};}
}
CombatReference::CombatReference(unsigned difficulty,unsigned shot,int sub,Vector3 point,unsigned seed,int rank)
    :position(point),difficulty_(difficulty),shot_(shot),random_(seed?seed:1),rank_(rank) {
    if(difficulty>3||shot>3||rank<0||rank>32||(sub!=21&&sub!=27&&sub!=37&&sub!=42&&sub!=45&&sub!=48&&sub!=57&&sub!=60&&sub!=63&&sub!=66&&sub!=69&&sub!=72&&sub!=75&&sub!=78&&sub!=51&&sub!=54&&sub!=81&&sub!=84&&sub!=87&&sub!=90&&sub!=93&&sub!=39&&sub!=40&&sub!=41&&sub!=96))
        throw std::invalid_argument("Unsupported Stage 4 combat reference entry");
    context_.sub=sub;handles_.fill(-1);
    // 后继组合借用 Sub39 已写入的三列选择码；独立启动时显式恢复，不从生产配置推导。
    if(sub==40||sub==41)SelectCombinations();
    // Sub37 由上一技能进入，继承 Sub26/27 设定的边界，不能以新演员的空边界执行随机反射。
    if(sub>=37) {boundsMin_={32,48,0};boundsMax_={352,144,0};clamped_=true;}
}
void CombatReference::SelectCombinations() {
    // 独立抄录已核对 EXE RVA 0x428f50 的四机体三列表，值为源组合选择码，不是生产 Behavior。
    constexpr std::array<int,12> selection{0,3,1,2,3,4,1,4,0,4,2,3};
    for(int n=0;n<3;++n)Variable(float(-10002-n))=float(selection[shot_*3+n]);
}
float CombatReference::Random(float range) {
    random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
    return static_cast<float>(random_>>8)/16777216.0F*range;
}
float CombatReference::Value(float id,Vector3 player) const {
    if(id<=-10001&&id>=-10012&&id==std::floor(id))return context_.variables.at(static_cast<int>(-id)-10001);
    if(id==-10013)return static_cast<float>(difficulty_);
    if(id==-10015)return position.x;if(id==-10016)return position.y;if(id==-10017)return position.z;
    if(id==-10019)return player.y;
    if(id==-10021)return std::atan2(player.y-position.y,player.x-position.x);
    if(id==-10025)return static_cast<float>(shot_);
    return id;
}
float& CombatReference::Variable(float id) {return context_.variables.at(static_cast<int>(-id)-10001);}
void CombatReference::Interpolate(int frames,Vector3 target,int power) {
    start_=position;delta_=Difference(target,position);duration_=left_=frames;power_=power;
    velocity_={};interpolating_=true;
}
void CombatReference::Emit(Vector3 origin,Vector3 player) {
    const auto& a=shotInstruction_.args;const int count=static_cast<int>(a[2]),layers=static_cast<int>(a[3]);
    const float high=a[4],low=a[5];
    for(int layer=0;layer<layers;++layer)for(int n=0;n<count;++n) {
        float angle=a[6],speed=high-(high-low)*layer/layers;
        if(shotInstruction_.opcode==69||shotInstruction_.opcode==70)angle+=n*2*pi/count+layer*a[7];
        else if(shotInstruction_.opcode==67||shotInstruction_.opcode==68) {
            float spread=count%2?(n+1)/2*a[7]:n/2*a[7]+a[7]/2;if(n%2)spread=-spread;angle+=spread;
        } else if(shotInstruction_.opcode==75) {angle=Random(a[6]-a[7])+a[7];speed=Random(high-low)+low;}
        else throw std::logic_error("Unsupported Stage 4 combat formation");
        if(shotInstruction_.opcode==67||shotInstruction_.opcode==69)angle+=std::atan2(player.y-origin.y,player.x-origin.x);
        emissions.push_back({origin,{std::cos(angle)*speed,std::sin(angle)*speed,0},int(a[0]),int(a[1]),
            (int(a[8])&16)?acceleration_:0,(int(a[8])&32)?angularVelocity_:0,(int(a[8])&32)?angularFrames_:0});
        auto& emitted=emissions.back();emitted.flags=int(a[8]);
        if(emitted.flags&32)emitted.speedAcceleration=acceleration_;
        if(emitted.flags&(64|128|256)) {
            emitted.turnDelay=angularFrames_;emitted.turnCount=turnCount_;emitted.turnAngle=turnAngle_;emitted.turnSpeed=turnSpeed_;
        }
        if(int(a[8])&16) {
            const float accelerationAngle=angularVelocity_==-999?angle:angularVelocity_;
            emissions.back().accelerationVector={std::cos(accelerationAngle)*acceleration_,std::sin(accelerationAngle)*acceleration_,0};
            emissions.back().accelerationFrames=angularFrames_;
        }
    }
    if((int(a[8])&512)&&sound_>=0)sounds.push_back(sound_);
}
void CombatReference::Execute(const Instruction& ins,Vector3 player) {
    const auto& a=ins.args;const auto v=[&](int n){return Value(a[n],player);};
    const auto jump=[&] {context_.frame=int(a[0]);context_.pc=int(a[1]);};
    switch(ins.opcode) {
        case 1:context_.pc=1000000;break;
        case 2:jump();break;
        case 3:if(--Variable(a[2])>0)jump();break;
        case 4:case 5:Variable(a[0])=v(1);break;
        case 8:Variable(a[0])=Random(v(1));break;
        case 9:Variable(a[0])=Random(v(1))+v(2);break;
        case 13:Variable(a[0])=int(v(1))+int(v(2));break;
        case 15:Variable(a[0])=int(v(1))*int(v(2));break;
        case 16:Variable(a[0])=int(v(1))/int(v(2));break;
        case 17:Variable(a[0])=int(v(1))%int(v(2));break;
        case 18:++Variable(a[0]);break;
        case 20:Variable(a[0])=v(1)+v(2);break;
        case 21:Variable(a[0])=v(1)-v(2);break;
        case 23:Variable(a[0])=v(1)/v(2);break;
        case 27:case 28:context_.compare=v(0)<v(1)?-1:v(0)>v(1)?1:0;break;
        case 29:if(context_.compare<0)jump();break;
        case 30:if(context_.compare<=0)jump();break;
        case 31:if(context_.compare==0)jump();break;
        case 32:if(context_.compare>0)jump();break;
        case 33:if(context_.compare>=0)jump();break;
        case 34:if(context_.compare!=0)jump();break;
        case 35:case 39:
            if(ins.opcode==39&&v(3)!=v(4))break;
            stack_.push_back(context_);context_.sub=int(a[0]);context_.pc=context_.frame=0;
            Variable(-10001)=a[1];Variable(-10005)=a[2];break;
        case 36:
            if(stack_.empty())context_.pc=1000000;else {context_=stack_.back();stack_.pop_back();}break;
        case 43:position={v(0),v(1),v(2)};break;
        case 47:speed_=v(0);interpolating_=false;break;
        case 50: {
            direction_=Random(a[1]-a[0])+a[0];
            if(position.x<boundsMin_.x+96&&std::abs(direction_)>pi/2)direction_=std::copysign(pi,direction_)-direction_;
            if(position.x>boundsMax_.x-96&&std::abs(direction_)<pi/2)direction_=std::copysign(pi,direction_)-direction_;
            if(position.y<boundsMin_.y+48&&direction_<0)direction_=-direction_;
            if(position.y>boundsMax_.y-48&&direction_>0)direction_=-direction_;
            interpolating_=false;break;
        }
        case 57:Interpolate(int(a[0]),{v(1),v(2),v(3)},2);break;
        case 61:Interpolate(int(a[0]),Add(position,{std::cos(direction_)*speed_*a[0]/2,std::sin(direction_)*speed_*a[0]/2,0}),2);break;
        case 65:boundsMin_={a[0],a[1],0};boundsMax_={a[2],a[3],0};clamped_=true;break;
        case 67:case 68:case 69:case 70:case 75: {
            shotInstruction_=ins;for(int n=0;n<9;++n)shotInstruction_.args[n]=v(n);
            const float influence=-0.5F+rank_/32.0F;
            shotInstruction_.args[4]=a[4]==0?0:std::max(0.3F,v(4)+influence);
            shotInstruction_.args[5]=std::max(0.3F,v(5)+influence/2);
            if(!suppressed_)Emit(Add(position,offset_),player);break;
        }
        case 76:period_=int(a[0]);period_+=period_/5-(period_/5*2*rank_)/32;elapsed_=0;break;
        case 78:suppressed_=true;break;
        case 79:suppressed_=false;break;
        case 81:offset_={v(0),v(1),v(2)};break;
        case 82:
            acceleration_=v(4);angularVelocity_=v(5);angularFrames_=v(0);
            turnCount_=int(v(1));turnAngle_=v(4);turnSpeed_=v(5);break;
        case 84:
            if(a[0]>=0) {sound_=int(a[0]);shotInstruction_.args[8]=float(int(shotInstruction_.args[8])|512);}
            else shotInstruction_.args[8]=float(int(shotInstruction_.args[8])&~512);break;
        case 85:case 86:
            handles_.at(slot_)=int(beams.size());
            beams.push_back({true,slot_,int(a[1]),0,int(a[8]),int(a[9]),int(a[10]),int(a[11]),int(a[12]),
                v(2),v(4),v(6),v(7),Add(position,offset_)});
            if(ins.opcode==86) {
                auto& beam=beams.back();beam.angle+=std::atan2(player.y-beam.anchor.y,player.x-beam.anchor.x);
                beam.speed=v(3);beam.lengthLimit=v(6);beam.end=v(5);
            }
            break;
        case 87:slot_=int(a[0]);break;
        case 88: {
            const int handle=handles_.at(int(a[0]));
            if(handle>=0&&beams.at(handle).active)beams[handle].angle+=v(1);break;
        }
        case 95:spawns.push_back({int(a[0]),int(a[4]),int(a[5]),int(a[6]),{v(1),v(2),v(3)}});break;
        case 94:clearBullets=true;break;
        case 102: {
            // 独立抄录真实EXE颜色表对应五行；不从生产RGBA请求反推源期望。
            std::uint32_t color=0;
            switch(int(a[0])) {
                case 10:color=0xffe0e0ffU;break;
                case 4:case 25:color=0xffffe0e0U;break;
                case 16:color=0xffe0ffe0U;break;
                case 22:color=0xffffffe0U;break;
                default:throw std::logic_error("Unsupported Stage 4 attached palette");
            }
            effects.push_back({13,1,color,position,{v(1),v(2),v(3)},v(4)});break;
        }
        case 118:effects.push_back({int(v(0)),int(v(1)),ins.effectColor,position});break;
        case 105:damageable=a[0]!=0;break;
        case 106:sounds.push_back(int(a[0]));break;
        case 93:card=int(a[1]);break;
        case 111:health=int(v(0));break;
        case 108:deathCallback=int(a[0]);break;
        case 114:healthCallback=int(a[0]);break;
        case 116:timeoutCallback=int(a[0]);break;
        case 113:threshold=int(v(0));break;
        case 115:timeout=int(v(0));break;
        case 117:interactable=a[0]!=0;break;
        case 121:
            if(a[0]==3) {SelectCombinations();break;}
            if(a[0]!=12)throw std::logic_error("Unsupported Stage 4 native extension");
            for(int handle:handles_)if(handle>=0&&beams[handle].active) {
                // 原生原位旋转第二项使用已改写的 X；保留其求值边界。
                const float x=64*std::cos(beams[handle].angle),y=x*std::sin(beams[handle].angle);
                Emit(Add(position,{x,-y,0}),player);
            }
            break;
        case 134:handles_.fill(-1);break;
        // 本版 EXE 的 127 分派到 RVA 0x25c2d 公共下一指令入口，确认为空操作。
        case 127:break;
        // 生命跳转和ANM由生命周期/图形测试另验，不在此伪造执行效果。
        case 83:case 96:case 97:case 98:case 99:case 100:case 101:case 103:case 104:
        case 107:case 112:case 119:case 126:case 130:case 133:break;
        default:throw std::logic_error("Unsupported Stage 4 combat opcode "+std::to_string(ins.opcode));
    }
}
void CombatReference::Tick(Vector3 player) {
    emissions.clear();sounds.clear();spawns.clear();effects.clear();clearBullets=false;
    // 原生 RVA 0x11f5f/0x11fbb/0x12055 的三段计时在各段末帧交接，
    // ECL 在寿命末帧仍能读取该引用；消退结束的更新之后才清 active。
    for(auto& b:beams)if(b.active) {
        if(++b.age>b.grow+b.sustain+b.fade)b.active=false;
        if(b.speed>0) {b.end+=b.speed;b.start=std::max(b.start,b.end-b.lengthLimit);}
    }
    position=Add(position,velocity_);
    if(clamped_) {position.x=std::clamp(position.x,boundsMin_.x,boundsMax_.x);position.y=std::clamp(position.y,boundsMin_.y,boundsMax_.y);}
    int budget=4096;
    while(--budget) {
        const auto program=ProgramsForDifficulty(difficulty_)[context_.sub];
        if(context_.pc>=int(program.size())||program[context_.pc].frame>context_.frame)break;
        visited.at(context_.sub)=true;const auto ins=program[context_.pc++];Execute(ins,player);
    }
    if(!budget)throw std::logic_error("Stage 4 reference instruction budget exhausted");
    if(period_>0&&++elapsed_>=period_) {elapsed_-=period_;Emit(Add(position,offset_),player);}
    if(interpolating_) {
        --left_;const float fraction=1-std::pow(std::max(0.0F,float(left_)/duration_),float(power_));
        const auto next=Add(start_,Scale(delta_,fraction));velocity_=Difference(next,position);
        if(left_<=0) {position=next;velocity_={};speed_=0;interpolating_=false;}
    } else velocity_={std::cos(direction_)*speed_,std::sin(direction_)*speed_,0};
    ++context_.frame;
}
}
