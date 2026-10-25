// 本文件保留重构前 ECL 行为作为测试基准，覆盖调用/循环、镜像、瞄准和延迟转向，不进入生产引擎。
#include "reference_script.hpp"
#include "stages/stage1/ids.hpp"
#include "gameplay/bullets/patterns.hpp"
#include "gameplay/data/native_combat.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace koumakyou::stage1 {
namespace {
constexpr float kPi = std::numbers::pi_v<float>;
constexpr std::array<Color,16> kColors{{{190,190,200,255},{140,35,60,255},{255,65,95,255},
 {120,50,150,255},{240,105,245,255},{40,75,160,255},{95,155,255,255},{35,130,155,255},
 {85,225,255,255},{40,125,55,255},{120,235,150,255},{150,245,80,255},
 {170,140,30,255},{255,220,85,255},{255,155,75,255},{250,245,235,255}}};
Vector3 Add(Vector3 a, Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vector3 Multiply(Vector3 v, float n) { return {v.x*n,v.y*n,v.z*n}; }
}
void ReferenceScript::Reset(int sub, Vector3 source, float sourceHealth, std::uint32_t seed, bool mirrored) {
    *this = ReferenceScript{};
    if (sub < 0 || sub >= 38) throw std::invalid_argument("Invalid Stage 1 subprogram");
    context_.sub = sub; sourcePosition = source; health = maxHealth = sourceHealth;
    visited[sub]=true;
    random_ = seed ? seed : 1; mirrored_ = mirrored;
}
float ReferenceScript::Random(float range) {
    random_ ^= random_ << 13; random_ ^= random_ >> 17; random_ ^= random_ << 5;
    return static_cast<float>(random_ >> 8) / 16777216.0F * range;
}
float ReferenceScript::Value(float v) const {
    const int index = static_cast<int>(-v) - 10001;
    return index >= 0 && index < 12 && v == std::floor(v) ? context_.variables[index] : v;
}
float& ReferenceScript::Variable(float id) { return context_.variables.at(static_cast<int>(-id)-10001); }
void ReferenceScript::Call(int sub, float integer, float floating) {
    visited.at(sub)=true;
    if (stack_.size() >= 32) throw std::runtime_error("Stage 1 call stack exhausted");
    stack_.push_back(context_); context_ = {}; context_.sub = sub;
    context_.variables[0] = integer; context_.variables[4] = floating;
}
void ReferenceScript::Interpolate(int duration, Vector3 target, int power) {
    interpolationStart_ = sourcePosition;
    interpolationDelta_ = Add(target,Multiply(sourcePosition,-1));
    interpolationFrames_ = interpolationLeft_ = duration; interpolationPower_ = power;
    interpolating_ = true; velocity_ = {};
}
void ReferenceScript::Update(float seconds, Vector3 player, BulletSystem& bullets) {
    if(!std::isfinite(seconds) || seconds<0)throw std::invalid_argument("Invalid Stage 1 delta time");
    soundEvents.clear(); dropEvents.clear();
    remainingFrames_ += static_cast<double>(seconds) * 60;
    while (remainingFrames_ + 0.00001 >= 1 && !finished && pendingCallback < 0) {
        remainingFrames_ -= 1; Tick(player,bullets);
    }
}
void ReferenceScript::Transition() {
    if (pendingCallback < 0) return;
    context_ = {}; context_.sub = pendingCallback; pendingCallback = -1; stack_.clear();
    visited.at(context_.sub)=true;
    if (health <= 0) health = 1; // 击破回调的退场演员保持 1 HP，避免再次触发旧死亡回调。
    timerCallback_ = deathCallback_;
    lifeThreshold_ = -1; timeoutFrames = -1; phaseFrames = 0;
}
void ReferenceScript::SetAnimation(int script) {
    animationScript=script;animationAge=0;++animationRevision;
}
void ReferenceScript::Tick(Vector3 player, BulletSystem& bullets) {
    // 原作先采用上一帧算出的位移，再解释本帧指令；保留此顺序才能复现转折帧的位置。
    sourcePosition.x += velocity_.x * (mirrored_ ? -1 : 1);
    sourcePosition.y += velocity_.y; sourcePosition.z += velocity_.z;
    if (clamped_) { sourcePosition.x = std::clamp(sourcePosition.x,lower_.x,upper_.x);
                    sourcePosition.y = std::clamp(sourcePosition.y,lower_.y,upper_.y); }
    if (health < lifeThreshold_) { health = lifeThreshold_; pendingCallback = lifeCallback_; return; }
    if (health <= 0 && deathCallback_ >= 0) { pendingCallback = deathCallback_; return; }
    if (timeoutFrames >= 0 && phaseFrames >= timeoutFrames) {
        if (lifeThreshold_ > 0) health = lifeThreshold_;
        pendingCallback = timerCallback_; return;
    }
    // 循环可以在同一帧回到较早的脚本时间；预算防止损坏内容导致主线程永远无法返回。
    int budget = 2048;
    while (!finished && budget-- > 0) {
        const auto instructions = reference::kPrograms.at(context_.sub);
        if (context_.pc >= static_cast<int>(instructions.size())) break;
        const auto& instruction = instructions[context_.pc];
        if (instruction.frame > context_.frame) break;
        ++context_.pc; Execute(instruction,player,bullets);
    }
    if (budget <= 0) throw std::runtime_error("Stage 1 instruction budget exhausted");
    if (interpolating_) {
        --interpolationLeft_;
        const float remaining = std::max(0.0F,static_cast<float>(interpolationLeft_)/interpolationFrames_);
        const float fraction = interpolationPower_<0 ? std::pow(1-remaining,static_cast<float>(-interpolationPower_)) :
                               1-std::pow(remaining,static_cast<float>(interpolationPower_));
        const Vector3 next = Add(interpolationStart_,Multiply(interpolationDelta_,fraction));
        velocity_ = Add(next,Multiply(sourcePosition,-1));
        if (interpolationLeft_ <= 0) { sourcePosition = next; velocity_ = {}; interpolating_ = false; speed_ = 0; }
    } else {
        angle_ += angularVelocity_; speed_ += acceleration_;
        velocity_ = {std::cos(angle_)*speed_,std::sin(angle_)*speed_,0};
    }
    if(poses_[3]>=0) {
        const float horizontal=velocity_.x*(mirrored_ ? -1 : 1);
        const int next=horizontal<0 ? -2 : horizontal>0 ? 2 : 0;
        if(next!=pose_) {
            const int action=next<0 ? poses_[3] : next>0 ? poses_[4] :
                pose_==-2 ? poses_[1] : pose_==2 ? poses_[2] : poses_[0];
            SetAnimation(action);pose_=next;
        }
    }
    animationAge+=1.0/60;
    ++context_.frame; ++phaseFrames;
}
void ReferenceScript::Execute(const reference::Instruction& i, Vector3 player, BulletSystem& bullets) {
    const auto& a = i.args;
    const auto v = [&](int n) { return Value(a[n]); };
    const auto jump = [&] { context_.frame = static_cast<int>(a[0]); context_.pc = static_cast<int>(a[1]); };
    // 生命相关值只从统一目录读取；normalIndex 与当前解释器已核对的一面 570 条指令一一对应。
    // 缺失目录数据不能回退旧硬编码，否则新增关卡会在数值尚未接通时被误判为已实现。
    const auto healthValue=[&] {
        const auto* record=native_combat::Instruction(1,context_.sub,context_.pc-1);
        if(!record || record->opcode!=i.opcode || !record->value)throw std::logic_error("Missing native health instruction");
        return Value(static_cast<float>(*record->value));
    };
    switch (i.opcode) {
    case 0: break;
    case 1: finished = true; break;
    case 2: jump(); break;
    case 3: if (--Variable(a[2]) > 0) jump(); break;
    case 4: case 5: Variable(a[0]) = v(1); break;
    case 6: Variable(a[0]) = std::floor(Random(v(1))); break;
    case 8: Variable(a[0]) = Random(v(1)); break;
    case 20: Variable(a[0]) = v(1)+v(2); break;
    case 21: Variable(a[0]) = v(1)-v(2); break;
    case 23: Variable(a[0]) = v(1)/v(2); break;
    case 27: context_.compare = v(0)<v(1) ? -1 : v(0)>v(1) ? 1 : 0; break;
    case 31: if (context_.compare == 0) jump(); break;
    case 34: if (context_.compare != 0) jump(); break;
    case 35: Call(static_cast<int>(a[0]),a[1],a[2]); break;
    case 36: if (stack_.empty()) finished = true; else { context_ = stack_.back(); stack_.pop_back(); } break;
    case 39: if (v(3)==a[4]) Call(static_cast<int>(a[0]),a[1],a[2]); break;
    case 43: sourcePosition = {v(0),v(1),v(2)}; break;
    case 45: angle_ = v(0); speed_ = v(1); interpolating_ = false; break;
    case 46: angularVelocity_ = v(0); interpolating_ = false; break;
    case 47: speed_ = v(0); interpolating_ = false; break;
    case 48: acceleration_ = v(0); interpolating_ = false; break;
    case 49: case 50:
        angle_ = Random(a[1]-a[0])+a[0];
        if (i.opcode==50) {
            if (sourcePosition.x < lower_.x+96 && std::abs(angle_)>kPi/2) angle_ = std::copysign(kPi,angle_)-angle_;
            if (sourcePosition.x > upper_.x-96 && std::abs(angle_)<kPi/2) angle_ = std::copysign(kPi,angle_)-angle_;
            if (sourcePosition.y < lower_.y+48 && angle_<0) angle_ = -angle_;
            if (sourcePosition.y > upper_.y-48 && angle_>0) angle_ = -angle_;
        }
        break;
    case 56: case 57: case 59: Interpolate(static_cast<int>(a[0]),{v(1),v(2),v(3)},i.opcode==57 ? 2 : i.opcode==59 ? -2 : 1); break;
    case 61: Interpolate(static_cast<int>(a[0]),Add(sourcePosition,
                {std::cos(angle_)*speed_*a[0]/2,std::sin(angle_)*speed_*a[0]/2,0}),2); break;
    case 65: lower_ = {a[0],a[1]}; upper_ = {a[2],a[3]}; clamped_ = true; break;
    case 66: clamped_ = false; break;
    case 67: case 68: case 69: case 70: Emit(i,player,bullets); break;
    case 81: offset_ = {v(0),v(1),v(2)}; break;
    case 82: for (int n=0;n<8;++n) effects_[n]=v(n); break;
    case 86: {
        const Vector3 origin = ReferencePosition(Add(sourcePosition,offset_));
        Bullet* beam = bullets.SpawnBeam(origin,BulletSystem::DirectionTo(origin,player),
                a[6]*kReferenceScale,a[7]*kReferenceScale/2,a[8]/60,a[9]/60,kColors[static_cast<int>(a[1])%16]);
        if (beam) { beam->visual=static_cast<VisualId>(kNativeLaserBase+static_cast<int>(a[1])%16);
                    beam->warningSeconds = a[11]/60; beam->dangerEndSeconds = (a[8]+a[9]-a[12])/60;
                    beam->maxAge=(a[8]+a[9]+a[10])/60; beam->beamGrowSeconds=a[8]/60;
                    beam->beamFadeSeconds=a[10]/60; }
        break;
    }
    case 93: case 94: bullets.ClearEnemyBullets(); rankInfluence_={-0.5F,0.5F,0,0,0,0}; break;
    case 106: soundEvents.push_back(static_cast<int>(a[0])); break;
    case 105: damageable = healthValue()!=0; break;
    case 108: deathCallback_=static_cast<int>(healthValue()); break;
    case 111: health=maxHealth=healthValue(); break;
    case 112: phaseFrames=static_cast<int>(healthValue()); break;
    case 113: lifeThreshold_=healthValue(); break;
    case 114: lifeCallback_=static_cast<int>(healthValue()); break;
    case 115: timeoutFrames=static_cast<int>(healthValue()); break;
    case 116: timerCallback_=static_cast<int>(healthValue()); break;
    case 119: dropEvents.push_back(static_cast<int>(a[0])); break;
    case 97: SetAnimation(static_cast<int>(a[0])); break;
    case 100: deathEffect=static_cast<int>(a[0])&255;break;
    case 98: for(int n=0;n<5;++n)poses_[n]=static_cast<int>(a[n]);pose_=-1;break;
    // 下列指令控制额外槽位特效、全局演出或掉落；其生命周期由 Game/renderer 管理。
    case 76: case 78: case 79: case 96: case 101: case 102:
    case 103: case 104: case 107: case 117: case 118: case 126:
    break;
    case 131: for (int n=0;n<6;++n)rankInfluence_[n]=a[n]; break;
    default: throw std::runtime_error("Unsupported Stage 1 opcode: "+std::to_string(i.opcode));
    }
}
void ReferenceScript::Emit(const reference::Instruction& i, Vector3 player, BulletSystem& bullets) {
    const auto& a=i.args;
    const auto rankInt=[&](int n) { return static_cast<int>(rank_*(rankInfluence_[n+1]-rankInfluence_[n])/32)+static_cast<int>(rankInfluence_[n]); };
    const int count=std::max(1,static_cast<int>(Value(a[2]))+rankInt(2)), layers=std::max(1,static_cast<int>(Value(a[3]))+rankInt(4));
    const int color=std::clamp(static_cast<int>(Value(a[1])),0,15), type=static_cast<int>(a[0]);
    const float influence=rankInfluence_[0]+rank_*(rankInfluence_[1]-rankInfluence_[0])/32;
    const float first=Value(a[4])==0 ? 0 : std::max(0.3F,Value(a[4])+influence);
    const float last=std::max(0.3F,Value(a[5])+influence/2);
    const Vector3 origin=ReferencePosition(Add(sourcePosition,offset_));
    // 原作二维角度与位移完整保留在 X/Z 投影；源 Y 向下对应世界 -Z，源正角度因此绕世界 +Y。
    // 整轮在发射时锁定高度坡度，各速度层沿 Y 同步扩展，玩家升降不会改变原扇形或环的投影结构。
    const float planarDistance=std::hypot(player.x-origin.x,player.z-origin.z);
    const float heightSlope=std::clamp((player.y-origin.y)/std::max(planarDistance,4.0F),-3.0F,3.0F);
    const float heightFactor=std::sqrt(1+heightSlope*heightSlope);
    const float aim=std::atan2(origin.z-player.z,player.x-origin.x);
    for(int layer=0;layer<layers;++layer) for(int n=0;n<count;++n) {
        float angle=Value(a[6]);
        if(i.opcode<=68) {
            float fan=(count%2 ? (n+1)/2*Value(a[7]) : n/2*Value(a[7])+Value(a[7])/2);
            if(n%2)fan=-fan;
            angle+=fan+(i.opcode==67 ? aim : 0);
        } else angle+=n*2*kPi/count+layer*Value(a[7])+(i.opcode==69 ? aim : 0);
        const Vector3 direction{std::cos(angle),heightSlope,-std::sin(angle)};
        const float speed=first-(first-last)*layer/layers;
        const VisualId visual=NativeBulletVisual(type,color);
        Bullet* bullet=bullets.Spawn(BulletOwner::Enemy,origin,Multiply(direction,speed*60*kReferenceScale),
                (type==2 ? 5.0F : 2.0F)*kReferenceScale,kColors[color],visual);
        if(!bullet) continue;
        bullet->nativeType=type;bullet->nativeColor=color;
        bullet->rotationAxis={0,1,0};
        const int flags=static_cast<int>(a[8]);
        if(flags&16)bullet->acceleration=Multiply(direction,effects_[4]*3600*kReferenceScale);
        if(flags&1) { bullet->launchBoost=5*60*kReferenceScale*heightFactor; bullet->launchBoostSeconds=16.0F/60;
                     bullet->baseVelocity=bullet->velocity; }
        if(flags&(64|128)) {
            bullet->turnDelaySeconds=effects_[0]/60;
            bullet->turnAngle=effects_[4]; bullet->turnSpeed=effects_[5]*60*kReferenceScale*heightFactor;
            bullet->turnAimed=(flags&128)!=0; bullet->baseVelocity=bullet->velocity;
        }
        // etama3.anm 的显现脚本结束于 10/16/32 帧；ECL 运动特效在显现结束后才起算。
        bullet->spawnVisualSeconds=flags&8 ? 32.0F/60 : flags&4 ? 16.0F/60 : flags&2 ? 10.0F/60 : 0;
        bullet->spawnMoveScale=flags&8 ? 1.0F/3 : flags&4 ? 0.4F : flags&2 ? 0.5F : 1;
        if(bullet->spawnVisualSeconds>0)bullet->baseVelocity=bullet->velocity;
    }
}
} // namespace koumakyou::stage1
