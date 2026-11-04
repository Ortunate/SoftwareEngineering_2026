// 本文件在源像素坐标执行三面新增符卡的指令与已核对原生星形函数，独立生成发射见证。
#include "stage3_spell_reference.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>
namespace koumakyou::stage3::reference {
namespace {
constexpr float pi=std::numbers::pi_v<float>;
float Normalize(float value) {while(value>pi)value-=2*pi;while(value<-pi)value+=2*pi;return value;}
Vector3 Add(Vector3 a,Vector3 b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vector3 Delta(Vector3 a,Vector3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vector3 Scale(Vector3 v,float factor) {return {v.x*factor,v.y*factor,v.z*factor};}
}
SpellReference::SpellReference(unsigned difficulty,int sub,Vector3 point,unsigned seed):position(point),difficulty_(difficulty),random_(seed?seed:1) {
    if(difficulty>3||!(sub==4||sub==7||sub==9||(sub>=19&&sub<=48)))throw std::invalid_argument("Unsupported Stage 3 combat reference entry");
    context_.sub=sub;entry_=sub;if(sub==4||sub==7||sub==9||sub==21||sub==29)clamped_=false;
}
float SpellReference::Random(float range) {
    random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
    return static_cast<float>(random_>>8)/16777216.0F*range;
}
float SpellReference::Value(float value,Vector3 player) const {
    const int index=static_cast<int>(-value)-10001;
    if(index>=0&&index<12&&value==std::floor(value))return context_.variables[index];
    if(value==-10021)return std::atan2(player.y-position.y,player.x-position.x);
    if(value==-10022)return static_cast<float>(phaseFrames);
    return value;
}
float& SpellReference::Variable(float id) {return context_.variables.at(static_cast<int>(-id)-10001);}
void SpellReference::Interpolate(int frames,Vector3 target,int power) {
    interpolationStart_=position;interpolationDelta_=Delta(target,position);
    interpolationDuration_=interpolationLeft_=frames;power_=power;interpolating_=true;velocity_={};
}
void SpellReference::Emit(const Instruction& instruction,Vector3 player,Vector3 origin,float base,float first) {
    const auto& a=instruction.args;
    const int count=std::max(1,static_cast<int>(a[2])+static_cast<int>(16*(rank_[3]-rank_[2])/32)+static_cast<int>(rank_[2]));
    const int layers=std::max(1,static_cast<int>(a[3])+static_cast<int>(16*(rank_[5]-rank_[4])/32)+static_cast<int>(rank_[4]));
    const float influence=rank_[0]+16*(rank_[1]-rank_[0])/32;
    const float high=first==0?0:std::max(0.3F,first+influence),low=std::max(0.3F,a[5]+influence/2);
    for(int layer=0;layer<layers;++layer)for(int n=0;n<count;++n) {
        float angle=base,speed=high-(high-low)*layer/layers;
        if(instruction.opcode==67||instruction.opcode==68) {
            float fan=count%2?(n+1)/2*a[7]:n/2*a[7]+a[7]/2;if(n%2)fan=-fan;
            angle+=fan;
            if(instruction.opcode==67)angle+=std::atan2(player.y-origin.y,player.x-origin.x);
        } else if(instruction.opcode==69||instruction.opcode==70) {
            angle+=n*2*pi/count+layer*a[7];
            if(instruction.opcode==69)angle+=std::atan2(player.y-origin.y,player.x-origin.x);
        } else if(instruction.opcode==75) {angle=Random(base-a[7])+a[7];speed=Random(high-low)+low;}
        else throw std::logic_error("Unknown Stage 3 reference formation");
        SpellEmission result;result.position=origin;result.velocity={std::cos(angle)*speed,std::sin(angle)*speed,0};
        result.type=static_cast<int>(a[0]);result.color=static_cast<int>(a[1]);result.flags=static_cast<int>(a[8]);
        if(result.flags&16)result.acceleration={std::cos(accelerationAngle_)*acceleration_,std::sin(accelerationAngle_)*acceleration_,0};
        if(result.flags&64) {result.turnAngle=turnAngle_;result.turnSpeed=turnSpeed_;result.turnDelay=turnDelay_;}
        emissions.push_back(result);
    }
}
void SpellReference::Star(Vector3 player) {
    int count=static_cast<int>(Variable(-10003));const int limit=static_cast<int>(Variable(-10004));
    if(count>=limit) {context_.star=false;return;}
    if(count==0) {
        starStart_=position;starPlayer_=player;angles_[0]=Random(2*pi)-pi;angles_[1]=Normalize(angles_[0]+2.5132741928100586F);
    }
    if(count%30==0) {
        angles_[0]=angles_[1];
        for(int i=1;i<6;++i)angles_[i]=Normalize(angles_[i-1]+2.5132741928100586F);
    }
    if(count%6==0) {
        const float progress=static_cast<float>(count)/limit;
        Vector3 center=Add(starStart_,Scale(Delta(starPlayer_,starStart_),progress*0.1F));center.z=0;
        const float factor=progress+0.5F;float angle=1.0471975803375244F*factor;
        for(int edge=0;edge<5;++edge) {
            const float radius=Variable(-10008),part=(count%30)/30.0F;
            const Vector3 from{std::cos(angles_[edge])*radius,std::sin(angles_[edge])*radius,0};
            const Vector3 to{std::cos(angles_[edge+1])*radius,std::sin(angles_[edge+1])*radius,0};
            const float influence=rank_[0]+16*(rank_[1]-rank_[0])/32;
            const float sourceRange=std::max(0.3F,shot_.args[5]+influence/2);
            Emit(shot_,player,Add(center,Add(from,Scale(Delta(to,from),part))),angle,shot_.args[4]+Random(sourceRange));
            angle-=0.5235987901687622F*factor;
        }
        sounds.push_back(22);
    }
    Variable(-10003)=static_cast<float>(count+1);
}
void SpellReference::Execute(const Instruction& instruction,Vector3 player) {
    const auto& a=instruction.args;const auto v=[&](int n){return Value(a[n],player);};
    const auto jump=[&]{context_.frame=static_cast<int>(a[0]);context_.pc=static_cast<int>(a[1]);};
    switch(instruction.opcode) {
        case 1:context_.pc=1000000;break;
        case 2:jump();break;
        case 3:if(--Variable(a[2])>0)jump();break;
        case 4:case 5:Variable(a[0])=v(1);break;
        case 9:Variable(a[0])=Random(v(1))+v(2);break;
        case 6:random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;Variable(a[0])=static_cast<float>(random_%static_cast<unsigned>(v(1)));break;
        case 8:Variable(a[0])=Random(v(1));break;
        case 13:Variable(a[0])=static_cast<int>(v(1))+static_cast<int>(v(2));break;
        case 14:Variable(a[0])=static_cast<int>(v(1))-static_cast<int>(v(2));break;
        case 16:Variable(a[0])=static_cast<int>(v(1))/static_cast<int>(v(2));break;
        case 17:Variable(a[0])=static_cast<int>(v(1))%static_cast<int>(v(2));break;
        case 18:++Variable(a[0]);break;
        case 20:Variable(a[0])=v(1)+v(2);break;
        case 21:Variable(a[0])=v(1)-v(2);break;
        case 26:Variable(a[0])=Normalize(v(0));break;
        case 27:case 28:context_.compare=v(0)<v(1)?-1:v(0)>v(1)?1:0;break;
        case 30:if(context_.compare<=0)jump();break;
        case 29:if(context_.compare<0)jump();break;
        case 33:if(context_.compare>=0)jump();break;
        case 34:if(context_.compare!=0)jump();break;
        case 35:case 39: {
            if(instruction.opcode==39&&v(3)!=v(4))break;
            if(stack_.size()>=32)throw std::logic_error("Stage 3 reference stack exhausted");
            stack_.push_back(context_);context_.sub=static_cast<int>(a[0]);context_.frame=context_.pc=0;
            Variable(-10001)=a[1];Variable(-10005)=a[2];break;
        }
        case 36:
            if(stack_.empty()) {context_.pc=1000000;break;}
            context_=stack_.back();stack_.pop_back();break;
        case 43:position={v(0),v(1),v(2)};break;
        case 47:speed_=v(0);interpolating_=false;break;
        case 45:direction_=v(0);speed_=v(1);interpolating_=false;break;
        case 48:movementAcceleration_=v(0);interpolating_=false;break;
        case 50: {
            direction_=Random(a[1]-a[0])+a[0];
            if(position.x<128&&std::abs(direction_)>pi/2)direction_=std::copysign(pi,direction_)-direction_;
            if(position.x>256&&std::abs(direction_)<pi/2)direction_=std::copysign(pi,direction_)-direction_;
            if(position.y<96&&direction_<0)direction_=-direction_;
            if(position.y>96&&direction_>0)direction_=-direction_;
            interpolating_=false;break;
        }
        case 56:case 57:case 59:Interpolate(static_cast<int>(a[0]),{v(1),v(2),v(3)},instruction.opcode==59?-2:instruction.opcode==57?2:1);break;
        case 61:Interpolate(static_cast<int>(a[0]),Add(position,{std::cos(direction_)*speed_*a[0]/2,std::sin(direction_)*speed_*a[0]/2,0}),2);break;
        case 63:Interpolate(static_cast<int>(a[0]),Add(position,{std::cos(direction_)*speed_*a[0]/2,std::sin(direction_)*speed_*a[0]/2,0}),2);break;
        case 65:clamped_=true;lower_={a[0],a[1]};upper_={a[2],a[3]};break;
        case 66:clamped_=false;break;
        case 67:case 68:case 69:case 70:case 75: {
            shot_=instruction;for(int n=0;n<9;++n)shot_.args[n]=v(n);
            if(!suppressed_)Emit(shot_,player,Add(position,offset_),shot_.args[6],shot_.args[4]);break;
        }
        case 76:case 77:if(a[0]!=0)throw std::logic_error("Unexpected automatic Stage 3 spell emission");break;
        case 78:suppressed_=true;break;
        case 79:suppressed_=false;break;
        case 81:offset_={v(0),v(1),v(2)};break;
        case 82:
            acceleration_=a[4];accelerationAngle_=v(5);
            turnDelay_=a[0];turnAngle_=v(4);turnSpeed_=v(5);break;
        case 93:rank_={-0.5F,0.5F,0,0,0,0};break;
        case 94:clearBullets=true;rank_={-0.5F,0.5F,0,0,0,0};break;
        case 95:if(a[0]!=29)throw std::logic_error("Unknown Stage 3 source summon");spawns.push_back({{a[1],a[2],a[3]},int(a[4]),int(a[5]),int(a[6])});break;
        case 99: {
            if(a[0]!=4||a[1]!=66)throw std::logic_error("Unknown Stage 3 auxiliary animation");
            auto& slot=auxiliary[4];slot.script=66;slot.frames=0;++slot.revision;break;
        }
        case 105:damageable=a[0]!=0;break;
        case 106:sounds.push_back(static_cast<int>(a[0]));break;
        case 102: {
            std::uint32_t color=0;
            switch(int(a[0])) {
                case 5:color=0xffffe0e0U;break;
                case 6:color=0xffffe0ffU;break;
                case 9:case 10:case 11:color=0xffe0e0ffU;break;
                default:throw std::logic_error("Unsupported Stage 3 attached palette");
            }
            effects.push_back({13,1,color,position,{v(1),v(2),v(3)},v(4)});break;
        }
        case 118:effects.push_back({int(v(0)),int(v(1)),instruction.effectColor,position});break;
        case 112:phaseFrames=static_cast<int>(a[0]);break;
        case 122:if(a[0]==-1)context_.star=false;else if(a[0]==2)context_.star=true;else throw std::logic_error("Unknown Stage 3 extension");break;
        case 131:for(int n=0;n<6;++n)rank_[n]=a[n];break;
        // 生命跳转由独立配置和图形测试覆盖；102/118只记录请求，不模拟粒子后续积分。
        case 96:case 97:case 98:case 100:case 101:case 103:case 104:case 107:case 108:case 109:case 111:case 113:case 114:case 115:case 116:case 117:case 119:case 120:case 124:case 125:case 126:break;
        default:throw std::logic_error("Unsupported Stage 3 spell reference opcode "+std::to_string(instruction.opcode));
    }
}
void SpellReference::Tick(Vector3 player) {
    if(entry_==9&&ticks_>=130)throw std::logic_error("Stage 3 midboss effect reference window exhausted");
    ++ticks_;emissions.clear();sounds.clear();spawns.clear();effects.clear();clearBullets=false;position=Add(position,velocity_);
    if(clamped_) {position.x=std::clamp(position.x,lower_.x,upper_.x);position.y=std::clamp(position.y,lower_.y,upper_.y);}
    int budget=4096;
    while(--budget) {
        const auto program=ProgramsForDifficulty(difficulty_)[context_.sub];
        if(context_.pc>=static_cast<int>(program.size())||program[context_.pc].frame>context_.frame)break;
        visited.at(context_.sub)=true;
        const auto instruction=program[context_.pc++];Execute(instruction,player);
    }
    if(!budget)throw std::logic_error("Stage 3 spell reference instruction budget exhausted");
    if(interpolating_) {
        --interpolationLeft_;const float remaining=std::max(0.0F,float(interpolationLeft_)/interpolationDuration_);
        const float part=power_<0?std::pow(1-remaining,float(-power_)):1-std::pow(remaining,float(power_));
        const auto next=Add(interpolationStart_,Scale(interpolationDelta_,part));velocity_=Delta(next,position);
        if(interpolationLeft_<=0) {position=next;velocity_={};speed_=0;interpolating_=false;}
    } else {speed_+=movementAcceleration_;velocity_={std::cos(direction_)*speed_,std::sin(direction_)*speed_,0};}
    if(context_.star)Star(player);
    for(auto& slot:auxiliary)if(slot.script>=0)++slot.frames;
    ++context_.frame;++phaseFrames;
}
}
