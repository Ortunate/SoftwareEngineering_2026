// 本文件直接执行五面已审阅战斗ECL夹具，独立计算源像素运动和逐发快照，不调用生产行为或技能。
#include "stage5_combat_reference.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace koumakyou::stage5::reference {
namespace {constexpr float pi=std::numbers::pi_v<float>;}
CombatReference::CombatReference(unsigned difficulty,int sub,Vector3 point,unsigned seed,int rank)
    :position(point),difficulty_(difficulty),random_(seed?seed:1),rank_(rank) {
    const bool familiar=(sub>=27&&sub<=35)||(sub>=38&&sub<=41);
    const bool spell=sub==46||sub==49||sub==52||sub==55||sub==60;
    if(difficulty>3||(!familiar&&!spell&&sub!=13&&sub!=18&&sub!=24&&sub!=36&&sub!=43&&sub!=63)||rank<0||rank>32)
        throw std::invalid_argument("Unsupported Stage 5 combat reference entry");
    if(spell&&sub!=60&&((sub==49||sub==55)!=(difficulty>=2)))throw std::invalid_argument("Stage 5 source spell absent at difficulty");
    context_.sub=sub; // 道中符卡独立启动仍继承Sub13的战斗边界。
    if(familiar) {clamped_=movingPoses_=false;animation=-1;}
    if(spell||sub==63)upper_.y=132;
}
float CombatReference::Random(float range) {
    random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
    return static_cast<float>(random_>>8)/16777216.0F*range;
}
void CombatReference::DefeatFamiliar() {
    if(movingPoses_||!active)throw std::logic_error("Only an active familiar can enter Sub35");
    context_={};context_.sub=35;stack_.clear();damageable=false;
}
float& CombatReference::Variable(float id) {return context_.variables.at(int(-id)-10001);}
float CombatReference::Value(float id,Vector3 player) const {
    if(id<=-10001&&id>=-10012)return context_.variables.at(int(-id)-10001);
    if(id==-10013)return float(difficulty_);if(id==-10015)return position.x;if(id==-10016)return position.y;if(id==-10017)return position.z;
    if(id==-10021)return std::atan2(player.y-position.y,player.x-position.x);
    return id;
}
void CombatReference::Interpolate(int frames,Vector3 target) {
    start_=position;delta_={target.x-position.x,target.y-position.y,target.z-position.z};
    duration_=left_=frames;velocity_={};interpolating_=true;
}
void CombatReference::Emit(Vector3 player) {
    const auto& a=shot_.args;const int count=int(a[2]),layers=int(a[3]);
    const Vector3 origin{position.x+offset_.x,position.y+offset_.y,position.z+offset_.z};
    for(int layer=0;layer<layers;++layer)for(int n=0;n<count;++n) {
        float angle=a[6],speed=a[4]-(a[4]-a[5])*layer/layers;
        if(shot_.opcode==69||shot_.opcode==70)angle+=n*2*pi/count+layer*a[7];
        else if(shot_.opcode==67||shot_.opcode==68) {
            float spread=count%2?(n+1)/2*a[7]:n/2*a[7]+a[7]/2;if(n%2)spread=-spread;angle+=spread;
        } else if(shot_.opcode==75) {angle=Random(a[6]-a[7])+a[7];speed=Random(a[4]-a[5])+a[5];}
        else throw std::logic_error("Unsupported Stage 5 combat formation");
        if(shot_.opcode==67||shot_.opcode==69)angle+=std::atan2(player.y-origin.y,player.x-origin.x);
        emissions.push_back({origin,{std::cos(angle)*speed,std::sin(angle)*speed,0},int(a[0]),int(a[1]),
            (int(a[8])&(1024|2048))?bounces_:0,!(int(a[8])&2048)});
    }
    if((int(a[8])&512)&&sound_>=0)sounds.push_back(sound_);
}
void CombatReference::EmitClockArc(Vector3 player,int pulse) {
    const float dx=player.x-position.x,dy=player.y-position.y,length=std::hypot(dx,dy);
    if(length==0)throw std::logic_error("Source clock arc requires a distinct player position");
    const float radius=pulse%2?-256.0F:256.0F;
    const float baseX=dx*(0.5F-pulse*0.5F/9)+dx/length*radius,baseY=dy*(0.5F-pulse*0.5F/9)+dy/length*radius;
    for(int n=0;n<9;++n) {
        const float phi=pi/4-(n+1)*pi/18,armX=-dx/length*radius,armY=-dy/length*radius;
        const Vector3 point{position.x+baseX+armX*std::cos(phi)+armY*std::sin(phi),
            position.y+baseY-armX*std::sin(phi)+armY*std::cos(phi),position.z};
        const float angle=std::atan2(player.y-point.y,player.x-point.x)+(pulse%2?-pi/4+n*pi/18:0);
        emissions.push_back({point,{std::cos(angle)*2,std::sin(angle)*2,0},8,3,0,true});
    }
    sounds.push_back(7);
}
void CombatReference::Execute(const Instruction& ins,Vector3 player) {
    const auto& a=ins.args;const auto v=[&](int n){return Value(a[n],player);};
    const auto jump=[&]{context_.frame=int(a[0]);context_.pc=int(a[1]);};
    switch(ins.opcode) {
        case 1:active=false;context_.pc=1000000;break;
        case 2:jump();break;
        case 3:if(--Variable(a[2])>0)jump();break;
        case 4:case 5:Variable(a[0])=v(1);break;
        case 6:
            random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;Variable(a[0])=float(random_%unsigned(v(1)));break;
        case 9:Variable(a[0])=Random(v(1))+v(2);break;
        case 14:Variable(a[0])=int(v(1))-int(v(2));break;
        case 16:Variable(a[0])=int(v(1))/int(v(2));break;
        case 17:Variable(a[0])=int(v(1))%int(v(2));break;
        case 18:++Variable(a[0]);break;
        case 20:Variable(a[0])=v(1)+v(2);break;
        case 21:Variable(a[0])=v(1)-v(2);break;
        case 27:case 28:context_.compare=v(0)<v(1)?-1:v(0)>v(1)?1:0;break;
        case 33:if(context_.compare>=0)jump();break;
        case 34:if(context_.compare!=0)jump();break;
        case 35:
            stack_.push_back(context_);context_.sub=int(a[0]);context_.pc=context_.frame=0;
            Variable(-10001)=a[1];Variable(-10005)=a[2];break;
        case 36:
            if(stack_.empty())context_.pc=1000000;else {context_=stack_.back();stack_.pop_back();}break;
        case 43:position={v(0),v(1),v(2)};velocity_={};speed_=0;interpolating_=false;break;
        case 47:speed_=v(0);interpolating_=false;break;
        case 50:
            direction_=Random(a[1]-a[0])+a[0];
            if(position.x<lower_.x+96&&std::abs(direction_)>pi/2)direction_=std::copysign(pi,direction_)-direction_;
            if(position.x>upper_.x-96&&std::abs(direction_)<pi/2)direction_=std::copysign(pi,direction_)-direction_;
            if(position.y<lower_.y+48&&direction_<0)direction_=-direction_;
            if(position.y>upper_.y-48&&direction_>0)direction_=-direction_;
            interpolating_=false;break;
        case 52:
            direction_=v(1);speed_=v(2);
            Interpolate(int(a[0]),{position.x+std::cos(direction_)*speed_*a[0]/2,position.y+std::sin(direction_)*speed_*a[0]/2,position.z});break;
        case 57:Interpolate(int(a[0]),{v(1),v(2),v(3)});break;
        case 61:Interpolate(int(a[0]),{position.x+std::cos(direction_)*speed_*a[0]/2,position.y+std::sin(direction_)*speed_*a[0]/2,position.z});break;
        case 65:lower_={a[0],a[1],0};upper_={a[2],a[3],0};clamped_=true;break;
        case 67:case 68:case 69:case 70:case 75:
            shot_=ins;for(int n=0;n<9;++n)shot_.args[n]=v(n);
            shot_.args[4]=std::max(0.3F,v(4)-0.5F+rank_/32.0F);shot_.args[5]=std::max(0.3F,v(5)-0.25F+rank_/64.0F);
            if(!suppressed_)Emit(player);break;
        case 76:case 77:
            period_=int(a[0]);period_+=period_/5-(period_/5*2*rank_)/32;elapsed_=0;
            if(ins.opcode==77&&period_>0) {random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;elapsed_=int(random_%unsigned(period_));}break;
        case 78:suppressed_=true;break;
        case 79:suppressed_=false;break;
        case 81:offset_={v(0),v(1),v(2)};break;
        case 82:bounces_=int(a[0]);break;
        case 84:
            if(a[0]>=0) {sound_=int(a[0]);shot_.args[8]=float(int(shot_.args[8])|512);}
            else shot_.args[8]=float(int(shot_.args[8])&~512);break;
        case 93:card=int(a[1]);break;
        case 94:clearBullets=true;break;
        case 95:spawns.push_back({int(a[0]),int(a[4]),int(a[5]),int(a[6]),{v(1),v(2),v(3)}});break;
        case 102:
            // 本关五条附着均来自源颜色表第6行，不能由生产RGBA反推参考值。
            if(int(a[0])!=6)throw std::logic_error("Unsupported Stage 5 attached palette");
            effects.push_back({13,1,0xffffe0ffU,position,{v(1),v(2),v(3)},v(4)});break;
        case 118:effects.push_back({int(v(0)),int(v(1)),ins.effectColor,position});break;
        case 97:animation=int(a[0]);break;
        case 98:movingPoses_=true;pose_=-1;break; // 本关所有被测98均为同一组咲夜移动姿态。
        case 103:radius=std::min(a[0],a[1])/2;break;
        case 105:damageable=a[0]!=0;break;
        case 106:sounds.push_back(int(a[0]));break;
        case 111:health=int(v(0));break;
        case 113:threshold=int(v(0));break;
        case 115:timeout=int(v(0));break;
        case 117:interactable=a[0]!=0;break;
        case 119:drops.push_back(int(a[0]));break;
        case 121:
            if(a[0]!=4)throw std::logic_error("Unsupported Stage 5 native combat extension");
            if(a[1]==0||a[1]==1)timeStopped=a[1]!=0;
            else if(a[1]==2) {
                // 测试每步删除上一步弹体，不用生产弹池反推期望；同帧若有候选则拒绝而非假装无改向。
                if(!emissions.empty())throw std::logic_error("Source redirect snapshot unexpectedly contains current-frame candidates");
                ++redirects;
            } else throw std::logic_error("Unsupported Stage 5 world clock mode");break;
        case 122:
            if(a[0]==5)nativeClock_=0;else if(a[0]==-1)nativeClock_=-1;
            else throw std::logic_error("Unsupported Stage 5 continuous native callback");break;
        case 128:interrupt=int(a[0]);break;
        // 生命回调和清敌由角色/图形生命周期另验；独立记录清弹与粒子请求，不积分其后续运动。
        case 83:case 96:case 99:case 100:case 101:case 104:case 107:case 108:case 112:case 114:case 116:case 126:break;
        default:throw std::logic_error("Unsupported Stage 5 midboss opcode "+std::to_string(ins.opcode));
    }
}
void CombatReference::Tick(Vector3 player) {
    emissions.clear();sounds.clear();drops.clear();spawns.clear();effects.clear();clearBullets=false;interrupt=redirects=0;if(!active)return;
    position.x+=velocity_.x;position.y+=velocity_.y;position.z+=velocity_.z;
    if(clamped_) {position.x=std::clamp(position.x,lower_.x,upper_.x);position.y=std::clamp(position.y,lower_.y,upper_.y);}
    int budget=4096;
    while(active&&--budget) {
        const auto program=ProgramsForDifficulty(difficulty_)[context_.sub];
        if(context_.pc>=int(program.size())||program[context_.pc].frame>context_.frame)break;
        Execute(program[context_.pc++],player);
    }
    if(!budget)throw std::logic_error("Stage 5 midboss instruction budget exhausted");
    if(active&&period_>0&&++elapsed_>=period_) {elapsed_-=period_;Emit(player);}
    if(nativeClock_>=0) {if(nativeClock_%9==0)EmitClockArc(player,nativeClock_/9);++nativeClock_;}
    if(interpolating_) {
        --left_;const float ratio=1-std::pow(std::max(0.0F,float(left_)/duration_),2.0F);
        const Vector3 next{start_.x+delta_.x*ratio,start_.y+delta_.y*ratio,start_.z+delta_.z*ratio};
        velocity_={next.x-position.x,next.y-position.y,next.z-position.z};
        if(left_<=0) {position=next;velocity_={};speed_=0;interpolating_=false;}
    } else velocity_={std::cos(direction_)*speed_,std::sin(direction_)*speed_,0};
    // 独立入口继承Sub12的98动画表；按下一帧横向速度交接移动/回身，显式97仅替换当前动画。
    const int pose=velocity_.x<0?-2:velocity_.x>0?2:0;
    if(movingPoses_&&pose!=pose_) {animation=pose<0?129:pose>0?130:pose_==-2?131:pose_==2?132:128;pose_=pose;}
    if(!timeStopped)++phaseFrames;
    ++context_.frame;
}
}
