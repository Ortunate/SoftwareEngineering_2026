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
    if(flags&Accelerate)bullet.acceleration=Multiply(direction,state.projectile.acceleration);
    if(flags&LaunchBoost) {
        bullet.launchBoost=13.3928571F*factor;bullet.launchBoostSeconds=16.0F/60;
        bullet.baseVelocity=bullet.velocity;
    }
    if(flags&(DelayTurn|DelayAim)) {
        bullet.turnDelaySeconds=state.projectile.delayFrames/60;
        bullet.turnAngle=state.projectile.turnAngle;bullet.turnSpeed=state.projectile.turnSpeed*factor;
        bullet.turnAimed=(flags&DelayAim)!=0;bullet.baseVelocity=bullet.velocity;
    }
    // 零初速加速弹也须保存单位发射方向，不能在启动加速期退化为固定轴而破坏空间均匀性。
    if(state.spatial&&(flags&LaunchBoost))bullet.launchDirection=direction;
    // 显现脚本结束后才开始加速、减速和延迟转向，所有方向使用相同内容时长。
    bullet.spawnVisualSeconds=flags&SpawnLarge ? 32.0F/60 : flags&SpawnMedium ? 16.0F/60 : flags&SpawnSmall ? 10.0F/60 : 0;
    bullet.spawnMoveScale=flags&SpawnLarge ? 1.0F/3 : flags&SpawnMedium ? 0.4F : flags&SpawnSmall ? 0.5F : 1;
    if(bullet.spawnVisualSeconds>0)bullet.baseVelocity=bullet.velocity;
}
/// @brief 直接按等面积球带/球冠采样，不复制平面；速度层使用同一角分布以保留层间通道。
void FireSpatial(const State& state,const Shot& shot,Vector3 origin,Vector3 player,BulletSystem& bullets,
                 int count,int layers,float first,float last) {
    const bool aimed=shot.formation==Formation::AimedFan||shot.formation==Formation::AimedRing;
    const bool fan=shot.formation==Formation::AimedFan||shot.formation==Formation::FixedFan;
    const spatial::Frame frame=spatial::Frame::FromAxis(aimed ? Vector3{player.x-origin.x,player.y-origin.y,player.z-origin.z} : Vector3{0,0,-1});
    const float minimumCosine=fan ? std::cos(shot.coneHalfAngle) : -1;
    const int color=std::clamp(shot.color,0,15);
    const VisualId visual=static_cast<VisualId>(state.bulletBase+shot.type*16+color);
    for(int layer=0;layer<layers;++layer)for(int n=0;n<count;++n) {
        Vector3 direction;
        if(fan&&count==1) {
            if(shot.sampleIndex<0)direction=frame.forward;
            else {
                const auto index=static_cast<std::uint32_t>(shot.sampleIndex)+1;
                const float cosine=1-(1-minimumCosine)*spatial::RadicalInverse(index,2);
                direction=frame.Direction(cosine,2*kPi*spatial::RadicalInverse(index,3)+shot.angle);
            }
        } else direction=spatial::EqualAreaDirection(frame,n,count,minimumCosine,shot.angle+(fan ? 0 : layer*shot.spacing));
        const float speed=first-(first-last)*layer/layers;
        Bullet* bullet=bullets.Spawn(BulletOwner::Enemy,origin,Multiply(direction,speed),
            shot.type==2 ? 0.223214286F : 0.0892857143F,kColors[color],visual);
        // 各方向独立建立切向转轴，延迟 ±90° 转向沿球面切向展开，而非围绕某个全局轴聚集。
        if(bullet)ConfigureProjectile(*bullet,state,shot,direction,spatial::Frame::FromAxis(direction).right,1);
    }
}
}
void State::Fire(const Shot& shot,Vector3 position,Vector3 player,BulletSystem& bullets) const {
    if(!Valid(TimedShot{0,shot}))throw std::invalid_argument("Invalid shot configuration");
    const auto rankInt=[&](int n) { return static_cast<int>(rank*(rankInfluence[n+1]-rankInfluence[n])/32)+static_cast<int>(rankInfluence[n]); };
    const int count=std::max(1,(spatial&&shot.spatialCount>0 ? shot.spatialCount : shot.count)+rankInt(2)), layers=std::max(1,shot.layers+rankInt(4));
    const int color=std::clamp(shot.color,0,15), type=shot.type;
    const float influence=rankInfluence[0]+rank*(rankInfluence[1]-rankInfluence[0])/32;
    const float first=shot.firstSpeed==0 ? 0 : std::max(0.803571429F,shot.firstSpeed+influence);
    const float last=std::max(0.803571429F,shot.lastSpeed+influence/2);
    const Vector3 origin{position.x+offset.x,position.y+offset.y,position.z+offset.z};
    if(spatial) { FireSpatial(*this,shot,origin,player,bullets,count,layers,first,last);return; }
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
        } else angle+=n*2*kPi/count+layer*shot.spacing+(shot.formation==Formation::AimedRing ? aim : 0);
        const Vector3 direction{std::cos(angle),heightSlope,-std::sin(angle)};
        const float speed=first-(first-last)*layer/layers;
        const VisualId visual=type>=0 && type<10 ? static_cast<VisualId>(bulletBase+type*16+color) : fallback;
        Bullet* bullet=bullets.Spawn(BulletOwner::Enemy,origin,Multiply(direction,speed),
                type==2 ? 0.223214286F : 0.0892857143F,kColors[color],visual);
        if(!bullet) continue;
        ConfigureProjectile(*bullet,*this,shot,direction,{0,1,0},heightFactor);
    }
}
void State::Fire(const Laser& laser,Vector3 position,Vector3 player,BulletSystem& bullets) const {
    const Vector3 origin{position.x+offset.x,position.y+offset.y,position.z+offset.z};
    Bullet* beam=bullets.SpawnBeam(origin,BulletSystem::DirectionTo(origin,player),
        laser.length,laser.width/2,laser.grow/60,laser.sustain/60,kColors[laser.color%16]);
    if(beam) {
        beam->visual=static_cast<VisualId>(laserBase+laser.color%16);
        beam->warningSeconds=laser.warning/60;beam->dangerEndSeconds=(laser.grow+laser.sustain-laser.harmlessTail)/60;
        beam->maxAge=(laser.grow+laser.sustain+laser.fade)/60;beam->beamGrowSeconds=laser.grow/60;beam->beamFadeSeconds=laser.fade/60;
    }
}
bool Valid(const TimedShot& cue) {
    const auto& p=cue.shot;
    if(cue.frame<0 || static_cast<unsigned>(p.formation)>static_cast<unsigned>(Formation::FixedRing) ||
       p.type<0 || p.type>=10 || p.color<0 || p.color>=16 || p.count<1 || p.layers<1 || p.spatialCount<0 || p.sampleIndex<-1 || p.flags<0 || p.flags>255)return false;
    for(float v:{p.firstSpeed,p.lastSpeed,p.angle,p.spacing,p.coneHalfAngle})if(!std::isfinite(v))return false;
    return p.coneHalfAngle>0&&p.coneHalfAngle<=kPi;
}
} // namespace koumakyou::skill
