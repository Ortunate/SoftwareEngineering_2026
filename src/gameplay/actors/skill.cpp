// 本文件直接使用世界位置和每秒速率发射弹幕，保持速度层、瞄准、显现和延迟转向的计算顺序。
#include "gameplay/actors/skill.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace koumakyou::skill {
namespace {
constexpr float kPi=std::numbers::pi_v<float>;
constexpr std::array<Color,16> kColors{{{190,190,200,255},{140,35,60,255},{255,65,95,255},
 {120,50,150,255},{240,105,245,255},{40,75,160,255},{95,155,255,255},{35,130,155,255},
 {85,225,255,255},{40,125,55,255},{120,235,150,255},{150,245,80,255},
 {170,140,30,255},{255,220,85,255},{255,155,75,255},{250,245,235,255}}};
Vector3 Multiply(Vector3 v,float n) { return {v.x*n,v.y*n,v.z*n}; }
}
void State::Fire(const Shot& shot,Vector3 position,Vector3 player,BulletSystem& bullets) const {
    const auto rankInt=[&](int n) { return static_cast<int>(rank*(rankInfluence[n+1]-rankInfluence[n])/32)+static_cast<int>(rankInfluence[n]); };
    const int count=std::max(1,shot.count+rankInt(2)), layers=std::max(1,shot.layers+rankInt(4));
    const int color=std::clamp(shot.color,0,15), type=shot.type;
    const float influence=rankInfluence[0]+rank*(rankInfluence[1]-rankInfluence[0])/32;
    const float first=shot.firstSpeed==0 ? 0 : std::max(0.803571429F,shot.firstSpeed+influence);
    const float last=std::max(0.803571429F,shot.lastSpeed+influence/2);
    const Vector3 origin{position.x+offset.x,position.y+offset.y,position.z+offset.z};
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
        bullet->nativeType=type;bullet->nativeColor=color;
        bullet->rotationAxis={0,1,0};
        const int flags=shot.flags;
        if(flags&16)bullet->acceleration=Multiply(direction,projectile.acceleration);
        if(flags&1) { bullet->launchBoost=13.3928571F*heightFactor; bullet->launchBoostSeconds=16.0F/60;
                     bullet->baseVelocity=bullet->velocity; }
        if(flags&(64|128)) {
            bullet->turnDelaySeconds=projectile.delayFrames/60;
            bullet->turnAngle=projectile.turnAngle; bullet->turnSpeed=projectile.turnSpeed*heightFactor;
            bullet->turnAimed=(flags&128)!=0; bullet->baseVelocity=bullet->velocity;
        }
        // etama3.anm 的显现脚本结束于 10/16/32 帧；ECL 运动特效在显现结束后才起算。
        bullet->spawnVisualSeconds=flags&8 ? 32.0F/60 : flags&4 ? 16.0F/60 : flags&2 ? 10.0F/60 : 0;
        bullet->spawnMoveScale=flags&8 ? 1.0F/3 : flags&4 ? 0.4F : flags&2 ? 0.5F : 1;
        if(bullet->spawnVisualSeconds>0)bullet->baseVelocity=bullet->velocity;
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
       p.type<0 || p.type>=10 || p.color<0 || p.color>=16 || p.count<1 || p.layers<1 || p.flags<0 || p.flags>255)return false;
    for(float v:{p.firstSpeed,p.lastSpeed,p.angle,p.spacing})if(!std::isfinite(v))return false;
    return true;
}
} // namespace koumakyou::skill
