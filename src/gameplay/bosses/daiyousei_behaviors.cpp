// 本文件实现 DaiyouseiBoss 内部命名行为；移动、弹幕、演出和随机读取保持原内容时序。
#include "gameplay/bosses/daiyousei.hpp"
#include "stages/stage2/behaviors.hpp"
#include "stages/stage2/ids.hpp"
#include "gameplay/spatial.hpp"
#include <cmath>
#include <numbers>
namespace koumakyou::bosses {
using namespace stage2;
using behavior::Actor;using behavior::Routine;using skill::Formation;
namespace {

using behavior::Actor;
using behavior::Routine;
using skill::Formation;
constexpr float pi=std::numbers::pi_v<float>;
movement::State& Motion(Actor& a) { return a.liftedMotion ? a.liftedMotion->planar : a.motion; }
/// @brief Boss 移动保持原 X/Z 路程预算，仅以独立序号分配保守高度，不消耗攻击随机流。
void AimHeight(Actor& a) {
    if(a.liftedMotion)a.liftedMotion->Aim(a.motion,-1.25F+2.5F*spatial::RadicalInverse(a.spatialIndex++,2));
}
void MoveTo(Actor& a,int frames,Vector3 target,int power=2) {
    if(a.skills.spatial&&!a.liftedMotion)a.liftedMotion.emplace(a.motion);
    Motion(a).Interpolate(frames,target,power);AimHeight(a);
}

/// @brief 多向图案直接采样三倍方向预算；逐发单方向环转为连续球面扫描，避免所有弹停留在同一切面。
/// 中性对照不改变图案和随机读取；空间序号独立于演员的随机流。
void Fire(Actor& a,skill::Shot shot) {
    if(a.skills.spatial) {
        shot.spatialCount=shot.count>1 ? shot.count*3 : 1;
        if(shot.count==1&&(shot.formation==Formation::FixedRing||shot.formation==Formation::AimedRing)) {
            shot.formation=Formation::AimedFan;shot.sampleIndex=static_cast<int>(a.spatialIndex++);
            shot.coneHalfAngle=pi;
        }
    }
    a.Fire(shot);
}
/// @brief 琪露诺两组非符/符卡共用姿态槽；中断/退场不重新初始化整场演出的视觉年龄。

}

namespace {
/// @brief 两个旋转方向共用 48 次逐帧双速度层；浮点相位与速度按源顺序逐次相加。
Routine Spiral(Actor& a,bool red) {
    a.soundEvents.push_back(6);
    float speed=4.01785707F,tail=3.75F,angle=1.57079637F;
    for(int i=0;i<48;++i) {
        co_await a.At(40+i);Fire(a,{Formation::FixedRing,4,red?2:10,1,2,speed,tail,angle,0,5});
        a.soundEvents.push_back(9);speed+=0.133928567F;
        co_await a.At(41+i);tail-=0.0535714291F;angle+=red ? -0.130899698F : 0.130899698F;
    }
}
Routine AimedWaves(Actor& a) {
    a.soundEvents.push_back(6);
    for(int i=0;i<16;++i) {
        co_await a.At(40+i*10);
        a.skills.projectile={.delayFrames=40,.turnSpeed=10.7142859F};
        Fire(a,{Formation::AimedFan,5,15,3,2,10.7142859F,5.35714293F,0,0.52359879F,132});
        Fire(a,{Formation::AimedFan,5,6,3,1,6.69642878F,5.35714293F,0,0.52359879F,4});a.soundEvents.push_back(9);
    }
    co_await a.At(200);
}
Routine Teleport(Actor& a) {
    a.interactable=false;a.Animate(a.Position().x<0 ? 67 : 66);
    const float target=a.Random(14.2857141F)-7.14285707F;
    co_await a.At(40);a.Animate(target<0 ? 69 : 68);
    Motion(a).position={target,0,5.71428585F};
    a.motion.position={target,a.motion.position.y,5.71428585F};a.interactable=true;
    co_await a.At(80);
}
}
Routine DaiyouseiBoss::Behaviors::Midboss(Actor& a) {
    a.damageable=false;a.deathEffect=2;a.Animate(64);
    MoveTo(a,60,{0,0,5.71428585F});
    co_await a.At(60);a.damageable=true;
    co_await a.At(130);
    for(int cycle=0;cycle<8;++cycle) {
        if(cycle%3==2)co_await AimedWaves(a);
        else co_await Spiral(a,cycle%3==1);
        co_await Teleport(a);
        co_await a.At(210);a.Rewind(130);
    }
}
Routine DaiyouseiBoss::Behaviors::Defeated(Actor& a) {
    a.damageable=false;a.itemEvents.push_back(3);a.finished=true;co_return;
}
Routine DaiyouseiBoss::Behaviors::Timeout(Actor& a) {
    a.damageable=false;
    co_await a.At(10);MoveTo(a,60,{0,0,12.8571424F});
    co_await a.At(70);a.finished=true;
}

}
