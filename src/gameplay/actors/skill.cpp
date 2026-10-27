// 本文件直接使用世界位置和每秒速率发射弹幕，保持速度层、瞄准、显现和延迟转向的计算顺序。
#include "gameplay/actors/skill.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include "gameplay/spatial.hpp"
#include <stdexcept>
namespace koumakyou::skill {
namespace {
constexpr float kPi=std::numbers::pi_v<float>;
constexpr std::array<Color,16> kColors{{{190,190,200,255},{140,35,60,255},{255,65,95,255},
 {120,50,150,255},{240,105,245,255},{40,75,160,255},{95,155,255,255},{35,130,155,255},
 {85,225,255,255},{40,125,55,255},{120,235,150,255},{150,245,80,255},
 {170,140,30,255},{255,220,85,255},{255,155,75,255},{250,245,235,255}}};
Vector3 Multiply(Vector3 v,float n) { return {v.x*n,v.y*n,v.z*n}; }

/// @brief 初始化独立弹道快照；factor 仅供旧高度坡度补偿，三维单位方向恒为 1。
void ConfigureProjectile(Bullet& bullet,const State& state,const Shot& shot,Vector3 direction,Vector3 axis,float factor) {
    bullet.nativeType=shot.type;bullet.nativeColor=std::clamp(shot.color,0,15);bullet.rotationAxis=axis;
    const int flags=shot.flags;
    bullet.sampleMotionAtStart=state.projectile.sampleAtStepStart;
    if(flags&Accelerate) {
        bullet.acceleration=state.projectile.accelerationVector.value_or(Multiply(direction,state.projectile.acceleration));
        if(state.projectile.accelerationFrames>=0)bullet.accelerationSeconds=state.projectile.accelerationFrames/60;
    } else if(flags&Velocity) {
        bullet.polarMotion=true;bullet.angularVelocity=state.projectile.angularVelocity;bullet.angularSeconds=state.projectile.angularFrames/60;
        bullet.speedAcceleration=state.projectile.speedAcceleration;bullet.baseVelocity=bullet.velocity;
    }
    if(flags&LaunchBoost) {
        bullet.launchBoost=13.3928571F*factor;bullet.launchBoostSeconds=16.0F/60;
        bullet.baseVelocity=bullet.velocity;
    }
    if(flags&(DelayTurn|DelayAim|AbsoluteTurn)) {
        bullet.turnDelaySeconds=state.projectile.delayFrames/60;
        bullet.turnAngle=state.projectile.turnAngle;
        bullet.turnSpeed=state.projectile.preserveTurnSpeed ? BulletSystem::Length(bullet.velocity) : state.projectile.turnSpeed*factor;
        bullet.turnAimed=(flags&DelayAim)!=0;bullet.baseVelocity=bullet.velocity;
        bullet.turnAbsolute=(flags&AbsoluteTurn)!=0;bullet.turnCount=state.projectile.turnCount;
        const float horizontal=std::hypot(direction.x,direction.z);
        bullet.turnFixedDirection={horizontal*std::cos(state.projectile.turnAngle),direction.y,-horizontal*std::sin(state.projectile.turnAngle)};
    }
    // 零初速加速弹也须保存单位发射方向，不能在启动加速期退化为固定轴而破坏空间均匀性。
    if((state.spatial||state.projectile.sampleAtStepStart)&&(flags&LaunchBoost))bullet.launchDirection=direction;
    if(flags&(BounceAll|BounceExceptLowerZ)) {
        bullet.bouncesRemaining=state.projectile.bounceCount;bullet.reflectLowerZ=(flags&BounceAll)!=0;
        bullet.bounceSpeed=state.projectile.bounceSpeed<0 ? BulletSystem::Length(bullet.velocity) : state.projectile.bounceSpeed*factor;
        bullet.bounceLower=state.projectile.bounceLower;bullet.bounceUpper=state.projectile.bounceUpper;
        bullet.bounceHalfExtents=state.projectile.bounceHalfExtents;bullet.launchDirection=direction;
    }
    // 显现脚本结束后才开始加速、减速和延迟转向，所有方向使用相同内容时长。
    bullet.spawnVisualSeconds=flags&SpawnLarge ? 32.0F/60 : flags&SpawnMedium ? 16.0F/60 : flags&SpawnSmall ? 10.0F/60 : 0;
    bullet.spawnMoveScale=flags&SpawnLarge ? 1.0F/3 : flags&SpawnMedium ? 0.4F : flags&SpawnSmall ? 0.5F : 1;
    if(bullet.spawnVisualSeconds>0)bullet.baseVelocity=bullet.velocity;
}
/// @brief 直接按等面积球带/球冠采样，不复制平面；速度层使用同一角分布以保留层间通道。
void FireSpatial(const State& state,const Shot& shot,Vector3 origin,Vector3 player,BulletSystem& bullets,
                 int count,int layers,float first,float last,const std::function<float(float)>& random) {
    const bool aimed=shot.formation==Formation::AimedFan||shot.formation==Formation::AimedRing||shot.formation==Formation::AimedOffsetRing;
    const bool fan=shot.formation==Formation::AimedFan||shot.formation==Formation::FixedFan;
    const bool randomAngle=shot.formation==Formation::RandomAngle||shot.formation==Formation::RandomAngleSpeed;
    const bool randomSpeed=shot.formation==Formation::RandomSpeedRing||shot.formation==Formation::RandomAngleSpeed;
    const spatial::Frame frame=spatial::Frame::FromAxis(shot.spatialAxis.value_or(aimed ? BulletSystem::RotateAroundAxis(Vector3{player.x-origin.x,player.y-origin.y,player.z-origin.z},{0,1,0},shot.aimAxisOffset) :
        fan ? Vector3{std::cos(shot.angle),0,-std::sin(shot.angle)} : Vector3{0,0,-1}));
    const float minimumCosine=fan ? std::cos(shot.coneHalfAngle) : -1;
    const int color=std::clamp(shot.color,0,15);
    const VisualId visual=static_cast<VisualId>(state.bulletBase+shot.type*16+color);
    for(int layer=0;layer<layers;++layer)for(int n=0;n<count;++n) {
        Vector3 direction;
        float speed=first-(first-last)*layer/layers;
        if(randomAngle) {
            // 保留源图案每发一次角度读取；高度用独立低差异序列补齐，不消耗演员随机流。
            // atan2 的角度边界可跨过 ±π，不能先单独归一化而把窄区间变成整周。
            const float range=shot.angle-shot.spacing;
            const float sampled=random(range);
            const float center=(shot.angle+shot.spacing)/2;
            const auto cone=spatial::Frame::FromAxis(shot.spatialAxis.value_or(Vector3{std::cos(center),0,-std::sin(center)}));
            const float halfAngle=std::min(kPi,std::abs(range)/2);
            const auto sequence=static_cast<std::uint32_t>(layer*count+n+1)+(shot.sampleIndex<0 ? 0U : static_cast<std::uint32_t>(shot.sampleIndex));
            const float cosine=1-(1-std::cos(halfAngle))*spatial::RadicalInverse(sequence,2);
            direction=cone.Direction(cosine,range!=0 ? sampled/range*2*kPi : 0);
        } else if(fan&&count==1) {
            if(shot.sampleIndex<0)direction=frame.forward;
            else {
                const auto index=static_cast<std::uint32_t>(shot.sampleIndex)+1;
                const float cosine=1-(1-minimumCosine)*spatial::RadicalInverse(index,2);
                direction=frame.Direction(cosine,2*kPi*spatial::RadicalInverse(index,3)+shot.angle);
            }
        } else direction=spatial::EqualAreaDirection(frame,n,count,minimumCosine,
            (fan&&!aimed ? 0 : shot.angle)+(fan ? 0 : layer*shot.spacing));
        if(randomSpeed)speed=random(first-last)+last;
        Bullet* bullet=bullets.Spawn(BulletOwner::Enemy,origin,Multiply(direction,speed),
            shot.type==2 ? 0.223214286F : 0.0892857143F,kColors[color],visual);
        // 各方向独立建立切向转轴，延迟 ±90° 转向沿球面切向展开，而非围绕某个全局轴聚集。
        if(bullet)ConfigureProjectile(*bullet,state,shot,direction,spatial::Frame::FromAxis(direction).right,1);
    }
}
}
void State::Fire(const Shot& shot,Vector3 position,Vector3 player,BulletSystem& bullets,
                 const std::function<float(float)>& random) const {
    if(!Valid(TimedShot{0,shot}))throw std::invalid_argument("Invalid shot configuration");
    for(float value:{projectile.delayFrames,projectile.turnAngle,projectile.turnSpeed,projectile.acceleration,
        projectile.angularVelocity,projectile.angularFrames,projectile.speedAcceleration,projectile.accelerationFrames})
        if(!std::isfinite(value))throw std::invalid_argument("Non-finite projectile motion");
    if(projectile.delayFrames<0||projectile.angularFrames<0||projectile.accelerationFrames<-1||projectile.turnCount<1||
       ((shot.flags&(DelayTurn|DelayAim|AbsoluteTurn))&&projectile.delayFrames<=0))throw std::invalid_argument("Invalid projectile motion");
    if(projectile.accelerationVector)for(float v:{projectile.accelerationVector->x,projectile.accelerationVector->y,projectile.accelerationVector->z})
        if(!std::isfinite(v))throw std::invalid_argument("Non-finite projectile acceleration");
    if(shot.flags&(BounceAll|BounceExceptLowerZ)) {
        if(projectile.bounceCount<1||!std::isfinite(projectile.bounceSpeed))throw std::invalid_argument("Invalid bounce configuration");
        const auto lower=projectile.bounceLower,upper=projectile.bounceUpper,extent=projectile.bounceHalfExtents;
        for(float v:{lower.x,lower.y,lower.z,upper.x,upper.y,upper.z,extent.x,extent.y,extent.z})
            if(!std::isfinite(v))throw std::invalid_argument("Non-finite bounce boundary");
        if(lower.x>=upper.x||lower.y>=upper.y||lower.z>=upper.z||extent.x<0||extent.y<0||extent.z<0)
            throw std::invalid_argument("Invalid bounce boundary");
    }
    if(shot.formation>=Formation::RandomAngle&&!random)throw std::invalid_argument("Random formation requires random source");
    const auto rankInt=[&](int n) { return static_cast<int>(rank*(rankInfluence[n+1]-rankInfluence[n])/32)+static_cast<int>(rankInfluence[n]); };
    const int count=std::max(1,(spatial&&shot.spatialCount>0 ? shot.spatialCount : shot.count)+rankInt(2)), layers=std::max(1,shot.layers+rankInt(4));
    const int color=std::clamp(shot.color,0,15), type=shot.type;
    const float influence=rankInfluence[0]+rank*(rankInfluence[1]-rankInfluence[0])/32;
    const float first=shot.firstSpeed==0 ? 0 : std::max(0.803571429F,shot.firstSpeed+influence);
    const float last=std::max(0.803571429F,shot.lastSpeed+influence/2);
    const Vector3 origin{position.x+offset.x,position.y+offset.y,position.z+offset.z};
    if(spatial) { FireSpatial(*this,shot,origin,player,bullets,count,layers,first,last,random);return; }
    // 正角度绕世界 +Y，扇形与环形排列在世界 X/Z 平面。
    // 整轮在发射时锁定高度坡度，各速度层沿 Y 同步扩展，玩家升降不会改变原扇形或环的投影结构。
    const float planarDistance=std::hypot(player.x-origin.x,player.z-origin.z);
    const float heightSlope=std::clamp((player.y-origin.y)/std::max(planarDistance,4.0F),-3.0F,3.0F);
    const float heightFactor=std::sqrt(1+heightSlope*heightSlope);
    const float aim=std::atan2(origin.z-player.z,player.x-origin.x);
    for(int layer=0;layer<layers;++layer) for(int n=0;n<count;++n) {
        float angle=shot.angle;
        if(shot.formation==Formation::AimedFan || shot.formation==Formation::FixedFan) {
            float fan=(count%2 ? (n+1)/2*shot.spacing : n/2*shot.spacing+shot.spacing/2);
            if(n%2)fan=-fan;
            angle+=fan+(shot.formation==Formation::AimedFan ? aim : 0);
        } else if(shot.formation==Formation::RandomAngle||shot.formation==Formation::RandomAngleSpeed)
            angle=random(shot.angle-shot.spacing)+shot.spacing;
        else angle+=n*2*kPi/count+layer*shot.spacing+
            ((shot.formation==Formation::AimedRing||shot.formation==Formation::AimedOffsetRing) ? aim : 0)+
            ((shot.formation==Formation::AimedOffsetRing||shot.formation==Formation::FixedOffsetRing) ? kPi/count : 0);
        const Vector3 direction{std::cos(angle),heightSlope,-std::sin(angle)};
        const float speed=(shot.formation==Formation::RandomSpeedRing||shot.formation==Formation::RandomAngleSpeed)
            ? random(first-last)+last : first-(first-last)*layer/layers;
        const VisualId visual=type>=0 && type<10 ? static_cast<VisualId>(bulletBase+type*16+color) : fallback;
        Bullet* bullet=bullets.Spawn(BulletOwner::Enemy,origin,Multiply(direction,speed),
                type==2 ? 0.223214286F : 0.0892857143F,kColors[color],visual);
        if(!bullet) continue;
        ConfigureProjectile(*bullet,*this,shot,direction,{0,1,0},heightFactor);
    }
}
Bullet* State::Fire(const Laser& laser,Vector3 position,Vector3 player,BulletSystem& bullets) const {
    for(float value:{laser.length,laser.width,laser.grow,laser.sustain,laser.fade,laser.warning,laser.harmlessTail,laser.angle,laser.speed,laser.startOffset,laser.initialLength})
        if(!std::isfinite(value))throw std::invalid_argument("Non-finite laser configuration");
    if(laser.color<0||laser.color>=16||laser.length<=0||laser.width<=0||laser.grow<0||laser.sustain<=0||
       laser.fade<0||laser.warning<0||laser.harmlessTail<0||laser.startOffset<0||laser.animationScript<-1||laser.initialLength<-1||
       (laser.initialLength>=0&&(laser.initialLength>laser.length||laser.speed<=0)))
        throw std::invalid_argument("Invalid laser configuration");
    const Vector3 origin{position.x+offset.x,position.y+offset.y,position.z+offset.z};
    Vector3 direction=BulletSystem::RotateAroundAxis(laser.aimed ? BulletSystem::DirectionTo(origin,player) : Vector3{1,0,0},{0,1,0},laser.angle);
    if(laser.direction) {
        const auto vector=*laser.direction;const float length=BulletSystem::Length(vector);
        if(!std::isfinite(length)||length<=0)throw std::invalid_argument("Invalid laser world direction");
        direction=Multiply(vector,1/length);
    }
    const Vector3 start{origin.x+direction.x*laser.startOffset,origin.y+direction.y*laser.startOffset,origin.z+direction.z*laser.startOffset};
    Bullet* beam=bullets.SpawnBeam(start,direction,
        laser.length,laser.width/2,laser.grow/60,laser.sustain/60,kColors[laser.color%16]);
    if(beam) {
        beam->velocity=Multiply(direction,laser.speed);
        if(laser.initialLength>=0) {
            beam->velocity={};beam->beamLength=laser.initialLength;beam->beamLengthLimit=laser.length;
            beam->beamTravelSpeed=laser.speed;beam->beamEndDistance=laser.startOffset+laser.initialLength;
        }
        beam->visual=laser.visual==kInvalidVisualId ? static_cast<VisualId>(laserBase+laser.color%16) : laser.visual;
        beam->beamAnchor=origin;beam->beamStartOffset=laser.startOffset;beam->beamAnimationScript=laser.animationScript;
        beam->warningSeconds=laser.warning/60;beam->dangerEndSeconds=(laser.grow+laser.sustain-laser.harmlessTail)/60;
        beam->maxAge=(laser.grow+laser.sustain+laser.fade)/60;beam->beamGrowSeconds=laser.grow/60;beam->beamFadeSeconds=laser.fade/60;
    }
    return beam;
}
bool Valid(const TimedShot& cue) {
    const auto& p=cue.shot;
    if(cue.frame<0 || static_cast<unsigned>(p.formation)>static_cast<unsigned>(Formation::RandomAngleSpeed) ||
       p.type<0 || p.type>=10 || p.color<0 || p.color>=16 || p.count<1 || p.layers<1 || p.spatialCount<0 || p.sampleIndex<-1 || p.flags<0 || p.flags>4095)return false;
    for(float v:{p.firstSpeed,p.lastSpeed,p.angle,p.spacing,p.coneHalfAngle,p.aimAxisOffset})if(!std::isfinite(v))return false;
    if(p.spatialAxis) {
        const auto direction=*p.spatialAxis;
        const float length=BulletSystem::Length(direction);
        if(!std::isfinite(length)||length<=1e-6F)return false;
    }
    return p.coneHalfAngle>0&&p.coneHalfAngle<=kPi;
}
} // namespace koumakyou::skill
