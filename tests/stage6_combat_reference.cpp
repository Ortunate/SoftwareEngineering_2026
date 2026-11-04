// 本文件直接执行六面已审阅战斗ECL夹具，独立计算源像素运动和逐发快照，不调用生产行为或技能。
#include "stage6_combat_reference.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>
namespace koumakyou::stage6::reference {
namespace {constexpr float pi=std::numbers::pi_v<float>;}
CombatReference::CombatReference(unsigned difficulty,int sub,Vector3 point,unsigned seed,int rank,int shot,int chargingPulses)
    :position(point),difficulty_(difficulty),random_(seed?seed:1),rank_(rank),shotType_(shot),ambient_(seed) {
    const bool remilia=sub==19||sub==21||sub==25||sub==28||sub==31||sub==32||sub==35||sub==38||sub==41||sub==44||sub==47||sub==52||sub==55||sub==61||sub==64||sub==67;
    if(difficulty>3||(sub!=9&&sub!=14&&!remilia)||(remilia&&difficulty==0)||rank<10||rank>32||shot<0||shot>3||
       chargingPulses<0||chargingPulses>16||(sub!=21&&chargingPulses!=0))
        throw std::invalid_argument("Unsupported Stage 6 combat reference entry");
    if(((sub==32||sub==38||sub==44||sub==52||sub==61)&&difficulty!=1)||((sub==35||sub==41||sub==47||sub==55||sub==64)&&difficulty<2))throw std::invalid_argument("Stage 6 source spell absent at difficulty");
    context_.sub=sub;
    context_.variables[8]=float(chargingPulses);
    if(remilia) {poses_={160,163,164,161,162};animation=160;upper_.y=120;}
    if(sub==31) {clamped_=movingPoses_=false;animation=-1;}
    if(remilia&&sub!=31)nativeCallback_=6;
}
float CombatReference::Random(float range) {
    random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
    return static_cast<float>(random_>>8)/16777216.0F*range;
}
float& CombatReference::Variable(float id) {return context_.variables.at(int(-id)-10001);}
float CombatReference::Value(float id,Vector3 player) const {
    if(id<=-10001&&id>=-10012)return context_.variables.at(int(-id)-10001);
    // 源运行变量保存 1/2/4/8，而 UI/夹具选择使用 0…3；高难额外弹组直接与 4/8 比较。
    if(id==-10013)return float(1U<<difficulty_);if(id==-10015)return position.x;if(id==-10016)return position.y;if(id==-10017)return position.z;
    if(id==-10025)return float(shotType_);
    if(id==-10021)return std::atan2(player.y-position.y,player.x-position.x);
    return id;
}
void CombatReference::Interpolate(int frames,Vector3 target) {
    start_=position;delta_={target.x-position.x,target.y-position.y,target.z-position.z};
    duration_=left_=frames;velocity_={};interpolating_=true;
}
void CombatReference::Emit(Vector3 player,const Vector3* point) {
    const auto& a=shot_.args;const int count=int(a[2]),layers=int(a[3]);
    const Vector3 origin=point?*point:Vector3{position.x+offset_.x,position.y+offset_.y,position.z+offset_.z};
    for(int layer=0;layer<layers;++layer)for(int n=0;n<count;++n) {
        float angle=a[6],speed=a[4]-(a[4]-a[5])*layer/layers;
        if(shot_.opcode==69||shot_.opcode==70)angle+=n*2*pi/count+layer*a[7];
        else if(shot_.opcode==67||shot_.opcode==68) {
            float spread=count%2?(n+1)/2*a[7]:n/2*a[7]+a[7]/2;if(n%2)spread=-spread;angle+=spread;
        } else if(shot_.opcode==75||shot_.opcode==201) {angle=Random(a[6]-a[7])+a[7];speed=Random(a[4]-a[5])+a[5];}
        else throw std::logic_error("Unsupported Stage 6 combat formation");
        if(shot_.opcode==67||shot_.opcode==69)angle+=std::atan2(player.y-origin.y,player.x-origin.x);
        const Vector3 acceleration=radialAcceleration_ ? Vector3{std::cos(angle)*acceleration_.x,std::sin(angle)*acceleration_.x,0} : acceleration_;
        emissions.push_back({origin,{std::cos(angle)*speed,std::sin(angle)*speed,0},int(a[0]),int(a[1]),
            (int(a[8])&64)?delay_:0,(int(a[8])&64)?turn_:0,(int(a[8])&64)?turnCount_:0,
            (int(a[8])&16)?acceleration:Vector3{},(int(a[8])&16)?accelerationFrames_:-1,
            (int(a[8])&32)?projectileAngular_:0,(int(a[8])&32)?delay_:0,(int(a[8])&32)?polarAcceleration_:0,
            (int(a[8])&(1024|2048))?int(delay_):0,!(int(a[8])&2048),int(a[8])});
        projectiles.push_back(emissions.back());
    }
    if((int(a[8])&512)&&sound_>=0)sounds.push_back(sound_);
}
void CombatReference::SeedMines() {
    std::vector<Vector3> candidates;
    for(const auto& b:projectiles)if(b.type>=6&&b.type<=9)candidates.push_back(b.position);
    for(auto point:candidates) {
        Random(2*pi); // 原生8即使生成零初速小弹，仍消费一次未使用的方向。
        CombatEmission mine{.position=point,.type=3,.color=1,.flags=8};
        emissions.push_back(mine);projectiles.push_back(mine);
    }
    Variable(-10004)=float(candidates.size());
}
void CombatReference::ReleaseMines(bool randomDirections) {
    const float phase=Random(2*pi)-pi;sounds.push_back(12);
    for(auto& b:projectiles) {
        if(b.type<0||b.type>5||b.velocity.x!=0||b.velocity.y!=0||b.velocity.z!=0||
           b.acceleration.x!=0||b.acceleration.y!=0||b.acceleration.z!=0)continue;
        const float dx=position.x-b.position.x,dy=position.y-b.position.y,squared=dx*dx+dy*dy;
        const float distance=squared>0.10000000149F?std::sqrt(squared):0;
        const float angle=randomDirections?Random(2*pi)-pi:distance*pi*0.00390625F+phase;
        b.color=2;b.acceleration={std::cos(angle)*0.009999999776F,std::sin(angle)*0.009999999776F,0};
        b.accelerationFrames=120;
    }
}
void CombatReference::StarLattice(bool projectiles,Vector3 player) {
    const float seed=Random(2*pi);
    // 独立展开原生两组三层八节点；先交付本节点，再累计至下一层，不从生产世界几何回推位置。
    for(int pass=0;pass<2;++pass) {
        const float base=pass ? -7*pi/8+seed : -pi+seed,step=pass ? -pi/4 : pi/4;
        std::array<Vector3,8> nodes{};
        for(int i=0;i<8;++i)nodes[i]={position.x+32*std::cos(base+i*pi/4),position.y+32*std::sin(base+i*pi/4),position.z};
        for(int ring=0;ring<3;++ring)for(int i=0;i<8;++i) {
            const float angle=base+ring*step+i*pi/4,length=ring<2 ? 112 : 480;
            const Vector3 direction{std::cos(angle),std::sin(angle),0};
            if(projectiles)Emit(player,&nodes[i]);else beams.push_back({nodes[i],direction,length,60+ring*16});
            nodes[i].x+=direction.x*length;nodes[i].y+=direction.y*length;
        }
    }
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
        case 8:Variable(a[0])=Random(v(1));break;
        case 9:Variable(a[0])=Random(v(1))+v(2);break;
        case 13:Variable(a[0])=int(v(1))+int(v(2));break;
        case 14:Variable(a[0])=int(v(1))-int(v(2));break;
        case 16:Variable(a[0])=int(v(1))/int(v(2));break;
        case 17:Variable(a[0])=int(v(1))%int(v(2));break;
        case 18:++Variable(a[0]);break;
        case 20:Variable(a[0])=v(1)+v(2);break;
        case 21:Variable(a[0])=v(1)-v(2);break;
        case 23:Variable(a[0])=v(1)/v(2);break;
        case 26: {auto& angle=Variable(a[0]);while(angle>pi)angle-=2*pi;while(angle<-pi)angle+=2*pi;break;}
        case 27:case 28:context_.compare=v(0)<v(1)?-1:v(0)>v(1)?1:0;break;
        case 30:if(context_.compare<=0)jump();break;
        case 33:if(context_.compare>=0)jump();break;
        case 34:if(context_.compare!=0)jump();break;
        case 35:
            stack_.push_back(context_);context_.sub=int(a[0]);context_.pc=context_.frame=0;
            Variable(-10001)=a[1];Variable(-10005)=a[2];break;
        case 36:
            if(stack_.empty())context_.pc=1000000;else {context_=stack_.back();stack_.pop_back();}break;
        case 43:position={v(0),v(1),v(2)};velocity_={};speed_=0;interpolating_=false;break;
        case 45:direction_=v(0);speed_=v(1);interpolating_=false;break;
        case 46:angular_=v(0);break;
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
        case 67:case 68:case 69:case 70:case 75:case 201: {
            shot_=ins;for(int n=0;n<9;++n)shot_.args[n]=v(n);
            const float influence=rankInfluence_[0]+(rankInfluence_[1]-rankInfluence_[0])*rank_/32;
            shot_.args[4]=std::max(0.3F,v(4)+influence);shot_.args[5]=std::max(0.3F,v(5)+influence/2);
            if(!suppressed_)Emit(player);break;
        }
        case 76:case 77:
            period_=int(a[0]);period_+=period_/5-(period_/5*2*rank_)/32;elapsed_=0;
            if(ins.opcode==77&&period_>0) {random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;elapsed_=int(random_%unsigned(period_));}break;
        case 78:suppressed_=true;break;
        case 79:suppressed_=false;break;
        case 81:offset_={v(0),v(1),v(2)};break;
        case 82:
            delay_=v(0);turnCount_=int(v(1));turn_=v(4);
            projectileAngular_=v(5);polarAcceleration_=v(4);
            radialAcceleration_=v(5)==-999;accelerationFrames_=v(0);
            acceleration_=radialAcceleration_ ? Vector3{v(4),0,0} : Vector3{std::cos(v(5))*v(4),std::sin(v(5))*v(4),0};break;
        case 83:delay_=turn_=projectileAngular_=polarAcceleration_=0;turnCount_=0;acceleration_={};radialAcceleration_=false;accelerationFrames_=-1;break;
        case 84:
            if(a[0]>=0) {sound_=int(a[0]);shot_.args[8]=float(int(shot_.args[8])|512);}
            else shot_.args[8]=float(int(shot_.args[8])&~512);break;
        case 93:card=int(a[1]);break;
        case 94:rankInfluence_={-0.5F,0.5F,0,0,0,0};break;
        case 95:spawns.push_back({int(a[0]),int(a[4]),int(a[6]),int(a[5]),{v(1),v(2),v(3)}});break;
        case 96:projectiles.clear();break;
        case 97:animation=int(a[0]);break;
        case 98:
            // 源关闭姿态表时最后一槽写0xffff；运行时与前四槽的-1采用同一禁用约定。
            for(int n=0;n<5;++n)poses_[n]=a[n]==65535 ? -1 : int(a[n]);movingPoses_=poses_[3]>=0;pose_=-1;break;
        case 103:radius=std::min(a[0],a[1])/2;break;
        case 102:
            if(a[0]!=11)throw std::logic_error("Unsupported Stage 6 attachment palette");
            effects.push_back({13,1,0xffe0e0ffU,position,{a[1],a[2],a[3]},a[4]});break;
        case 105:damageable=a[0]!=0;break;
        case 106:sounds.push_back(int(a[0]));break;
        case 111:health=int(v(0));break;
        case 113:threshold=int(v(0));break;
        case 115:timeout=int(v(0));break;
        case 117:interactable=a[0]!=0;break;
        case 119:drops.push_back(int(a[0]));break;
        case 118:effects.push_back({int(a[0]),int(a[1]),ins.effectColor,position});break;
        case 121:
            if(a[0]==7&&(a[1]==0||a[1]==1))StarLattice(a[1]!=0,player);
            else if(a[0]==8&&a[1]==0)SeedMines();
            else if((a[0]==9||a[0]==11)&&a[1]==0)ReleaseMines(a[0]==11);
            else if(a[0]!=4||a[1]!=0)throw std::logic_error("Unsupported Stage 6 native extension");
            break; // 4(0)仅恢复世界时钟，7独立执行星阵节点和随机读取。
        case 131:for(int n=0;n<6;++n)rankInfluence_[n]=a[n];break;
        case 122:
            if(a[0]!=-1&&a[0]!=6&&a[0]!=10)throw std::logic_error("Unsupported Stage 6 callback");
            nativeCallback_=int(a[0]);break; // 10的环境生命保护在此执行，Bomb化蝠仍由角色生命周期另验。
        // 生命回调、清场及辅助动画由角色/图形生命周期另验，此处记录移动、发射与独立效果请求。
        case 99:case 100:case 101:case 104:case 107:case 108:case 112:case 114:case 116:case 126:break;
        default:throw std::logic_error("Unsupported Stage 6 midboss opcode "+std::to_string(ins.opcode));
    }
}
void CombatReference::Tick(Vector3 player) {
    emissions.clear();sounds.clear();drops.clear();spawns.clear();beams.clear();effects.clear();ambientEffects.clear();projectiles=environment;if(!active)return;
    position.x+=velocity_.x;position.y+=velocity_.y;position.z+=velocity_.z;
    if(clamped_) {position.x=std::clamp(position.x,lower_.x,upper_.x);position.y=std::clamp(position.y,lower_.y,upper_.y);}
    int budget=4096;
    while(active&&--budget) {
        const auto program=ProgramsForDifficulty(difficulty_)[context_.sub];
        if(context_.pc>=int(program.size())||program[context_.pc].frame>context_.frame)break;
        Execute(program[context_.pc++],player);
    }
    if(!budget)throw std::logic_error("Stage 6 midboss instruction budget exhausted");
    if(active&&period_>0&&++elapsed_>=period_) {elapsed_-=period_;Emit(player);}
    if(interpolating_) {
        --left_;const float ratio=1-std::pow(std::max(0.0F,float(left_)/duration_),2.0F);
        const Vector3 next{start_.x+delta_.x*ratio,start_.y+delta_.y*ratio,start_.z+delta_.z*ratio};
        velocity_={next.x-position.x,next.y-position.y,next.z-position.z};
        if(left_<=0) {position=next;velocity_={};speed_=0;interpolating_=false;}
    } else {direction_+=angular_;velocity_={std::cos(direction_)*speed_,std::sin(direction_)*speed_,0};}
    // 按下一帧横向速度交接移动/回身，显式97仅替换当前动画；角色姿态表在入场/98时建立。
    const int pose=velocity_.x<0?-2:velocity_.x>0?2:0;
    if(movingPoses_&&pose!=pose_) {animation=pose<0?poses_[3]:pose>0?poses_[4]:pose_==-2?poses_[1]:pose_==2?poses_[2]:poses_[0];pose_=pose;}
    if(active)ambientEffects=ambient_.Step(nativeCallback_,health,true,position);
    ++phaseFrames;
    ++context_.frame;
}
}
