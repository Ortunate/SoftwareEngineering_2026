// 本文件实现 KoakumaBoss 内部命名行为；移动、弹幕、演出和随机读取保持原内容时序。
#include "gameplay/bosses/koakuma.hpp"
#include "stages/stage4/behaviors.hpp"
#include "stages/stage4/ids.hpp"
#include "gameplay/spatial.hpp"
#include <cmath>
#include <numbers>
namespace koumakyou::bosses {
using namespace stage4;
using behavior::Actor;using behavior::Routine;using skill::Formation;
namespace {

using behavior::Actor;using behavior::Routine;using skill::Formation;
constexpr float pi=std::numbers::pi_v<float>;
void Fire(Actor& a,skill::Shot shot) {
    a.skills.projectile.sampleAtStepStart=true;
    if(a.skills.spatial) {
        shot.spatialCount=shot.count*3;
        if(shot.formation==Formation::RandomAngle||shot.formation==Formation::RandomAngleSpeed) {
            shot.sampleIndex=static_cast<int>(a.spatialIndex&0x7fffffffU);a.spatialIndex+=shot.spatialCount*shot.layers;
        }
    }
    a.ConfigureEmitter(shot);a.Fire(shot);
}

/// @brief 自动射击的参数在配置时快照；首发按整数初相并在内容帧末递增，停止帧先关闭再判断发射。

movement::State& Motion(Actor& a) {
    if(a.skills.spatial&&!a.liftedMotion)a.liftedMotion.emplace(a.motion);
    return a.liftedMotion ? a.liftedMotion->planar : a.motion;
}
void Height(Actor& a) {if(a.liftedMotion)a.liftedMotion->Aim(a.motion,-1.25F+2.5F*spatial::RadicalInverse(a.spatialIndex++,2));}

void Wander(Actor& a,float speed,int frames) {
    a.RandomDirection(-pi,pi);Motion(a).Apply({.speed=speed});Motion(a).Decelerate(frames);Height(a);
}
void SpawnEmitter(Actor& a) {
    auto pattern=kHiddenRandomEmitter;pattern.spatialShots=a.skills.spatial;
    a.spawnEvents.push_back({.position=a.Position(),.pattern=pattern,.sourceHealth=1000,.sourceScore=10,
        .seed=++a.spawnIndex,.sourceItem=0,.visual=kOrb,.hitRadius=0.625F,
        .bulletBase=kNativeBulletBase,.laserBase=kNativeLaserBase,.defeatBase=kNativeDefeat,.fallback=kBlueBullet,
        .retireOutside=false,.damageable=false});
}
Routine SummonPair(Actor& a) {SpawnEmitter(a);co_await a.At(30);SpawnEmitter(a);}

}

namespace {
Routine SpiralWave(Actor& a,float step) {
    float angle=a.Random(2*pi)-pi;a.soundEvents.push_back(5);
    a.skills.projectile={.acceleration=3.85714293F};
    for(int i=0;i<8;++i) {
        co_await a.At(i*20);Fire(a,{Formation::FixedRing,9,1,8,1,3.21428585F,2.67857143F,angle,0,20});
        angle-=step;a.soundEvents.push_back(7);
    }
    co_await a.At(160);
}
}
Routine KoakumaBoss::Behaviors::Encounter(Actor& a) {
    a.SetEmitterPeriod(0);a.ClearBullets();a.defeatActors=true;a.interactable=false;a.Animate(64);a.deathEffect=2;
    a.motion.position={0,a.motion.position.y,4.28571415F};
    Motion(a).bounds={{-7.14285714F,2.14285714F},{7.14285714F,7.85714286F}};Motion(a).clamped=true;
    co_await a.At(30);a.interactable=true;
    for(;;) {
        co_await SpiralWave(a,0.196349546F);Wander(a,4.01785707F,90);co_await SummonPair(a);
        co_await a.At(90);co_await SpiralWave(a,-0.196349546F);co_await SummonPair(a);
        co_await a.At(91);a.Rewind(30);
    }
}
Routine KoakumaBoss::Behaviors::Defeated(Actor& a) {
    a.SetEmitterPeriod(0);a.ClearBullets();a.interactable=false;a.itemEvents.push_back(3);a.dropEvents.push_back(8);a.finished=true;co_return;
}
Routine KoakumaBoss::Behaviors::Timeout(Actor& a) {a.SetEmitterPeriod(0);a.ClearBullets();a.interactable=false;a.finished=true;co_return;}

}
