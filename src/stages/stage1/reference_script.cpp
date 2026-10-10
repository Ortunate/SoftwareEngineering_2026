// 本文件实现新典一面的 ECL 时间轴，并在原轨迹上叠加 X/Y 走位与三维锥束、球壳、转向环和光束。
#include "stages/stage1/reference_script.hpp"
#include "stages/stage1/ids.hpp"
#include "gameplay/bullets/patterns.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace redmist::stage1 {
namespace {
constexpr float kPi = std::numbers::pi_v<float>;
constexpr std::array<Color,16> kColors{{{190,190,200,255},{140,35,60,255},{255,65,95,255},
 {120,50,150,255},{240,105,245,255},{40,75,160,255},{95,155,255,255},{35,130,155,255},
 {85,225,255,255},{40,125,55,255},{120,235,150,255},{150,245,80,255},
 {170,140,30,255},{255,220,85,255},{255,155,75,255},{250,245,235,255}}};
Vector3 Add(Vector3 a, Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vector3 Multiply(Vector3 v, float n) { return {v.x*n,v.y*n,v.z*n}; }
/// @brief 绕给定单位轴建立锥面方向；angle/roll 为弧度，返回单位向量，极端仰角仍使用正交基。
Vector3 ConeDirection(Vector3 axis, float angle, float roll) {
    axis=pattern::Normalize(axis);
    const Vector3 reference=std::abs(axis.y)<0.9F ? Vector3{0,1,0} : Vector3{1,0,0};
    const Vector3 right=pattern::Normalize(pattern::Cross(reference,axis));
    const Vector3 up=pattern::Cross(axis,right);
    return Add(Multiply(axis,std::cos(angle)),Add(Multiply(right,std::sin(angle)*std::cos(roll)),
                                                 Multiply(up,std::sin(angle)*std::sin(roll))));
}
/// @brief 同时倾斜环方向和其转向轴；两次旋转均为弧度，保持单位长度与旋转手性。
Vector3 TiltRing(Vector3 direction, float tilt, float bank) {
    return BulletSystem::RotateAroundAxis(BulletSystem::RotateAroundAxis(direction,{1,0,0},tilt),{0,0,1},bank);
}
}
void ReferenceScript::Reset(int sub, Vector3 source, float sourceHealth, std::uint32_t seed, bool mirrored,
                            ReferenceGeometry geometry) {
    *this = ReferenceScript{};
    if (sub < 0 || sub >= 38) throw std::invalid_argument("Invalid Stage 1 subprogram");
    context_.sub = sub; sourcePosition = source; health = maxHealth = sourceHealth;
    random_ = seed ? seed : 1; mirrored_ = mirrored;
    geometry_=geometry; initialSub_=sub;
    spatialPhase_=static_cast<float>((seed*2654435761U)>>8)/16777216.0F*2*kPi;
}
Vector3 ReferenceScript::Position() const {
    Vector3 world=ReferencePosition(sourcePosition);
    if(geometry_==ReferenceGeometry::Planar)return world;
    const float seconds=spatialFrames_/60.0F;
    const float fade=std::clamp(seconds/0.75F,0.0F,1.0F);
    const float envelope=fade*fade*(3-2*fade);
    const bool boss=initialSub_>=5;
    const bool dark=initialSub_==3 || initialSub_==4;
    const float phase=boss ? 0 : spatialPhase_;
    const float xOffset=(boss ? 1.4F : dark ? 1.2F : 0.85F)*std::sin(seconds*(boss ? 0.85F : 1.9F)+phase);
    const float yOffset=(boss ? 3.0F : dark ? 3.8F : 2.7F)*std::sin(seconds*(boss ? 0.68F : dark ? 0.95F : 1.4F)+phase)
                        +0.65F*std::sin(seconds*1.4F);
    // 仅约束新增的横向/高度偏移；原始 sourcePosition 和 Z 不参与变形，退场仍按原脚本边界判断。
    world.x=std::clamp(world.x+xOffset*envelope*(mirrored_ ? -1 : 1),-9.0F,9.0F);
    world.y=std::clamp(world.y+yOffset*envelope,-8.5F,8.5F);
    return world;
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
    if (health <= 0) health = 1; // 击破回调的退场演员保持 1 HP，避免再次触发旧死亡回调。
    timerCallback_ = deathCallback_;
    lifeThreshold_ = -1; timeoutFrames = -1; phaseFrames = 0;
}
void ReferenceScript::Tick(Vector3 player, BulletSystem& bullets) {
    ++spatialFrames_;
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
    ++context_.frame; ++phaseFrames;
}
void ReferenceScript::Execute(const reference::Instruction& i, Vector3 player, BulletSystem& bullets) {
    const auto& a = i.args;
    const auto v = [&](int n) { return Value(a[n]); };
    const auto jump = [&] { context_.frame = static_cast<int>(a[0]); context_.pc = static_cast<int>(a[1]); };
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
        const Vector3 origin = Add(Position(),{offset_.x*kReferenceScale,-offset_.z*kReferenceScale,-offset_.y*kReferenceScale});
        Vector3 direction=BulletSystem::DirectionTo(origin,player);
        if(geometry_==ReferenceGeometry::Spatial) {
            // 每组六道光束保留一条直接瞄准，其余五条围绕视线展开；方向在预警开始时锁定，允许玩家脱离扇面。
            const int index=beamIndex_++%6;
            if(index!=0)direction=ConeDirection(direction,0.18F,(index-1)*2*kPi/5+spatialFrames_/60.0F*0.4F);
        }
        Bullet* beam = bullets.SpawnBeam(origin,direction,
                a[6]*kReferenceScale,a[7]*kReferenceScale/2,a[8]/60,a[9]/60,kColors[static_cast<int>(a[1])%16]);
        if (beam) { beam->warningSeconds = a[11]/60; beam->dangerEndSeconds = (a[8]+a[9]-a[12])/60;
                    beam->maxAge=(a[8]+a[9]+a[10])/60; beam->beamGrowSeconds=a[8]/60;
                    beam->beamFadeSeconds=a[10]/60; }
        break;
    }
    case 93: case 94: bullets.ClearEnemyBullets(); rankInfluence_={-0.5F,0.5F,0,0,0,0}; break;
    case 106: soundEvents.push_back(static_cast<int>(a[0])); break;
    case 105: damageable = a[0]!=0; break;
    case 108: deathCallback_=static_cast<int>(a[0]); break;
    case 111: health=maxHealth=a[0]; break;
    case 112: phaseFrames=static_cast<int>(a[0]); break;
    case 113: lifeThreshold_=a[0]; break;
    case 114: lifeCallback_=static_cast<int>(a[0]); break;
    case 115: timeoutFrames=static_cast<int>(a[0]); break;
    case 116: timerCallback_=static_cast<int>(a[0]); break;
    case 119: dropEvents.push_back(static_cast<int>(a[0])); break;
    // 下列指令控制旧版 2D 动画、全局演出或掉落；其生命周期由 Game/renderer 管理。
    case 76: case 78: case 79: case 96: case 97: case 98: case 100: case 101: case 102:
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
    const Vector3 origin=Add(Position(),{offset_.x*kReferenceScale,-offset_.z*kReferenceScale,-offset_.y*kReferenceScale});
    // Planar 对照模式保留 X/Z 图案与发射时锁定的高度坡度；源正角度对应绕世界 +Y 旋转。
    // Spatial 在下方按原扇形/环/速度层生成三维单位方向，仍共用原弹速、弹数和时序。
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
        Vector3 direction{std::cos(angle),heightSlope,-std::sin(angle)};
        Vector3 rotationAxis{0,1,0};
        float speedFactor=heightFactor;
        if(geometry_==ReferenceGeometry::Spatial) {
            const float seconds=spatialFrames_/60.0F;
            if(i.opcode<=68) {
                const Vector3 axis=i.opcode==67 ? BulletSystem::DirectionTo(origin,player) :
                    Vector3{std::cos(Value(a[6])),0,-std::sin(Value(a[6]))};
                if(count>1) {
                    const bool hasCenter=count%2!=0;
                    const int ringIndex=n-(hasCenter ? 1 : 0), ringCount=count-(hasCenter ? 1 : 0);
                    const float aperture=std::clamp(std::abs(Value(a[7]))*count/2,0.18F,0.65F);
                    direction=hasCenter && n==0 ? axis : ConeDirection(axis,aperture,
                        ringIndex*2*kPi/ringCount+seconds*0.7F+layer*0.55F+Value(a[6]));
                } else {
                    // 多速度层展开为螺旋束，最快层保留瞄准中心；单颗连射用连续相位织出 Night Bird 的空间螺旋。
                    const float aperture=layers>1 ? 0.22F*std::sqrt(static_cast<float>(layer)/(layers-1)) :
                        std::min(0.45F,0.18F+std::abs(Value(a[6]))*0.4F);
                    direction=ConeDirection(axis,aperture,seconds*2.4F+layer*2.3999632F+Value(a[6])*3);
                }
            } else {
                const float tilt=0.55F*std::sin(seconds*0.7F+spatialPhase_);
                const float bank=0.4F*std::cos(seconds*0.53F+spatialPhase_);
                // 多层环变成不同纬度的球壳；单层环加入双波高度，避免所有弹仍落在同一倾斜平面。
                const float latitude=layers>1 ? -0.75F+1.5F*layer/(layers-1) : 0.28F*std::sin(2*angle+seconds);
                direction=TiltRing({std::cos(angle)*std::cos(latitude),std::sin(latitude),
                                    -std::sin(angle)*std::cos(latitude)},tilt,bank);
                rotationAxis=TiltRing({0,1,0},tilt,bank);
            }
            // 扩展使用单位方向，实际弹速沿用各原始速度层；不随高度或锥角增大而提速。
            speedFactor=1;
        }
        const float speed=first-(first-last)*layer/layers;
        const VisualId visual=color<=4 ? kRedBullet : color<=8 ? kBlueBullet : kGreenSphere;
        Bullet* bullet=bullets.Spawn(BulletOwner::Enemy,origin,Multiply(direction,speed*60*kReferenceScale),
                (type==2 ? 5.0F : 2.0F)*kReferenceScale,kColors[color],visual);
        if(!bullet) continue;
        bullet->rotationAxis=rotationAxis;
        const int flags=static_cast<int>(a[8]);
        if(flags&16)bullet->acceleration=Multiply(direction,effects_[4]*3600*kReferenceScale);
        if(flags&1) { bullet->launchBoost=5*60*kReferenceScale*speedFactor; bullet->launchBoostSeconds=16.0F/60;
                     bullet->baseVelocity=bullet->velocity; }
        if(flags&(64|128)) {
            bullet->turnDelaySeconds=effects_[0]/60;
            bullet->turnAngle=effects_[4]; bullet->turnSpeed=effects_[5]*60*kReferenceScale*speedFactor;
            bullet->turnAimed=(flags&128)!=0; bullet->baseVelocity=bullet->velocity;
        }
        // etama3.anm 的显现脚本结束于 10/16/32 帧；ECL 运动特效在显现结束后才起算。
        bullet->spawnVisualSeconds=flags&8 ? 32.0F/60 : flags&4 ? 16.0F/60 : flags&2 ? 10.0F/60 : 0;
        bullet->spawnMoveScale=flags&8 ? 1.0F/3 : flags&4 ? 0.4F : flags&2 ? 0.5F : 1;
        if(bullet->spawnVisualSeconds>0)bullet->baseVelocity=bullet->velocity;
    }
}
} // namespace redmist::stage1
