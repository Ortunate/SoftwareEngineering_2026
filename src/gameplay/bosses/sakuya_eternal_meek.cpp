// 本文件实现 SakuyaBoss 内部命名行为；移动、弹幕、演出和随机读取保持原内容时序。
#include "gameplay/bosses/sakuya.hpp"
#include "stages/stage6/behaviors.hpp"
#include "stages/stage6/ids.hpp"
#include "gameplay/spatial.hpp"
#include <cmath>
#include <numbers>
namespace koumakyou::bosses {
using namespace stage6;
using behavior::Actor;using behavior::Routine;using skill::Formation;
namespace {

using behavior::Actor;using behavior::Routine;using skill::Formation;
constexpr float pi=std::numbers::pi_v<float>;
movement::State& Motion(Actor& a) {if(a.skills.spatial&&!a.liftedMotion)a.liftedMotion.emplace(a.motion);return a.liftedMotion ? a.liftedMotion->planar : a.motion;}
void Height(Actor& a) {if(a.liftedMotion)a.liftedMotion->Aim(a.motion,-1.25F+2.5F*spatial::RadicalInverse(a.spatialIndex++,2));}
void MoveTo(Actor& a,int frames,Vector3 target) {
    auto& motion=Motion(a);if(a.liftedMotion)a.liftedMotion->Rebase(a.motion);
    motion.Interpolate(frames,target,2);Height(a);
}
void Teleport(Actor& a,Vector3 target) {
    const bool lifted=a.liftedMotion.has_value();auto state=Motion(a);state.position=target;state.Polar(0,0);state.velocity={};
    a.liftedMotion.reset();a.motion=state;if(lifted)a.liftedMotion.emplace(state);
}
void Wander(Actor& a,float speed,int frames) {
    auto& motion=Motion(a);if(a.liftedMotion)a.liftedMotion->Rebase(a.motion);
    a.RandomDirection(-pi,pi);motion.Apply({.speed=speed});motion.Decelerate(frames);Height(a);
}
void Configure(Actor& a,skill::Shot& shot) {
    a.skills.projectile.sampleAtStepStart=true;
    if(a.skills.spatial) {
        shot.spatialCount=shot.count*3;
        if(shot.formation==Formation::RandomAngle||shot.formation==Formation::RandomAngleSpeed) {
            shot.sampleIndex=static_cast<int>(a.spatialIndex&0x7fffffffU);a.spatialIndex+=shot.spatialCount*shot.layers;
        }
    }
    a.ConfigureEmitter(shot);
}
void Fire(Actor& a,skill::Shot shot) {Configure(a,shot);a.Fire(shot);}
void Initialize(Actor& a) {
    Motion(a).bounds={{-7.14285714F,4.64285714F},{7.14285714F,7.85714286F}};Motion(a).clamped=true;
    a.skills.offset={0,0,0.535714269F};a.SetEmitterPeriod(0);a.skills.projectile={};a.contactEnabled=true;a.interactable=true;a.damageable=true;a.deathEffect=2;
}

Routine MidFans(Actor& a,float angle,float step) {
    a.soundEvents.push_back(5);
    for(int i=0;i<12;++i) {co_await a.At(i*4);Fire(a,{Formation::FixedFan,8,3,8,1,8.5714283F,2.67857143F,angle,0.261799395F,0});angle-=step;a.soundEvents.push_back(7);}
    co_await a.At(48);
}
Routine MidDelayedFans(Actor& a) {
    for(int i=0;i<16;++i) {
        co_await a.At(i);const float angle=a.Random(2*pi)-pi,turn=a.Random(2*pi)-pi;
        a.skills.projectile={.delayFrames=60,.turnAngle=turn,.preserveTurnSpeed=true};a.soundEvents.push_back(22);
        co_await a.At(i+1);Fire(a,{Formation::FixedFan,4,2,5,1,6.69642878F,2.67857143F,angle,0.044879895F,66});
    }
}

}

Routine SakuyaBoss::Behaviors::EternalMeekEntrance(Actor& a) {
    a.damageable=false;a.contactEnabled=false;a.SetPoses({128,131,132,129,130});a.skills.offset={0,0,0.535714269F};
    Motion(a).clamped=false;MoveTo(a,60,{0,0,4.28571415F});co_await a.At(60);
    Motion(a).bounds={{-7.14285714F,4.28571415F},{7.14285714F,7.85714286F}};Motion(a).clamped=true;co_await a.At(1000000);
}
Routine SakuyaBoss::Behaviors::EternalMeekNonspell(Actor& a) {
    Initialize(a);Motion(a).bounds.lower.y=4.28571415F;a.skills.offset={};a.Animate(128);a.ClearBullets();a.defeatActors=true;
    a.interactable=false;Teleport(a,{0,0,4.28571415F});a.soundEvents.push_back(5);co_await a.At(20);a.Animate(139);co_await a.At(30);a.interactable=true;
    for(;;) {
        co_await MidFans(a,0,pi/12);Wander(a,4.01785707F,90);co_await MidDelayedFans(a);
        co_await a.At(31);co_await MidFans(a,pi,-pi/12);co_await MidDelayedFans(a);
        co_await a.At(32);a.Rewind(30);
    }
}
Routine SakuyaBoss::Behaviors::EternalMeek(Actor& a) {
    a.SetEmitterPeriod(0);a.damageable=false;a.skills.offset={};a.phaseFrames=0;MoveTo(a,120,{0,0,3.57142854F});
    co_await a.At(120);a.damageable=true;a.Animate(139);
    for(;;) {
        Fire(a,{Formation::RandomAngleSpeed,3,6,4,1,16.0714283F,8.03571415F,pi,0,4});
        Fire(a,{Formation::RandomAngleSpeed,3,6,12,1,13.3928576F,8.03571415F,0,-pi,4});a.soundEvents.push_back(7);
        co_await a.At(124);a.Rewind(120);
    }
}
Routine SakuyaBoss::Behaviors::EternalMeekRetreat(Actor& a) {a.SetEmitterPeriod(0);a.ClearBullets();a.damageable=false;a.contactEnabled=false;a.Animate(128);a.finished=true;co_return;}
Routine SakuyaBoss::Behaviors::EternalMeekDefeated(Actor& a) {a.soundEvents.push_back(18);a.dropEvents.push_back(20);a.itemEvents.push_back(3);co_await EternalMeekRetreat(a);}

}
