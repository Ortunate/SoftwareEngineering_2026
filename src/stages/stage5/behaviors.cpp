// 本文件编排五面普通晶体的停顿、环形速度序列和跨场发射，不持有原始 ECL 或资源所有权。
#include "stages/stage5/behaviors.hpp"
#include "stages/stage5/ids.hpp"
#include <numbers>
namespace koumakyou::stage5 {
namespace {
using behavior::Actor;using behavior::Routine;using skill::Formation;
constexpr float pi=std::numbers::pi_v<float>;
void Configure(Actor& a,skill::Shot& shot) {
    a.skills.projectile.sampleAtStepStart=true;
    if(a.skills.spatial) {
        shot.spatialCount=shot.count*3;
        if(shot.formation==Formation::RandomAngleSpeed) {shot.sampleIndex=static_cast<int>(a.spatialIndex&0x7fffffffU);a.spatialIndex+=shot.spatialCount*shot.layers;}
    }
}
void Fire(Actor& a,skill::Shot shot) {Configure(a,shot);a.ConfigureEmitter(shot);a.Fire(shot);}
Routine Lifetime(Actor& a) {co_await a.At(std::max(0,10000-static_cast<int>(a.phaseFrames)));a.finished=true;}
Routine Stop(Actor& a) {
    co_await a.At(40);a.motion.Apply({.acceleration=-10.7142859F});
    co_await a.At(70);a.motion.Apply({.acceleration=0});a.SetEmitterSound(22);
}
Routine SpeedRings(Actor& a,bool shortWave) {
    co_await Stop(a);float speed=4.01785707F;
    for(int burst=0;burst<(shortWave ? 5 : 8);++burst) {
        co_await a.At(burst*(shortWave ? 4 : 5));Fire(a,{Formation::AimedRing,3,6,shortWave ? 24 : 30,1,speed,2.67857143F,0,0,516});
        speed+=shortWave ? 1.47321427F : 1.01785719F;
    }
    co_await a.At(shortWave ? 20 : 40);a.motion.Polar(pi/2,4.8214283F);co_await Lifetime(a);
}
Routine LayeredRings(Actor& a,int color) {
    co_await Stop(a);float speed=4.01785707F;
    for(int burst=0;burst<8;++burst) {
        co_await a.At(burst*5);
        Fire(a,{Formation::AimedRing,3,color,24,1,speed,2.67857143F,burst<4&&burst%2==0 ? 0.130899698F : 0,0,516});
        speed+=burst<4 ? 0.535714269F : 0.669642866F;
    }
    co_await a.At(40);a.motion.Polar(pi/2,4.8214283F);co_await Lifetime(a);
}
Routine Passing(Actor& a,int period,skill::Shot shot) {
    Configure(a,shot);a.ConfigureEmitter(shot);period+=period/5-(period/5*2*a.skills.rank)/32;
    a.SetEmitterPeriod(period,static_cast<int>(a.RandomIndex(period)));a.SetEmitterSound(22);co_await Lifetime(a);
}
}
Routine AcceleratingBlueRings(Actor& a) {co_await SpeedRings(a,false);}
Routine ShortBlueRings(Actor& a) {co_await SpeedRings(a,true);}
Routine BlueLayeredRings(Actor& a) {co_await LayeredRings(a,6);}
Routine RedLayeredRings(Actor& a) {co_await LayeredRings(a,2);}
Routine GreenLayeredRings(Actor& a) {co_await LayeredRings(a,10);}
Routine RandomBlueFans(Actor& a) {
    co_await Stop(a);
    for(int burst=0;burst<40;++burst) {
        co_await a.At(burst*3);const float angle=a.Random(3.92699075F)-0.392699093F,speed=a.Random(5.35714293F)+2.67857143F;
        Fire(a,{Formation::FixedFan,1,6,5,1,speed,2.67857143F,angle,0.0392699093F,516});
    }
    co_await a.At(120);a.motion.Polar(pi/2,4.8214283F);co_await Lifetime(a);
}
Routine PassingRandomCrystals(Actor& a) {co_await Passing(a,60,{Formation::RandomAngleSpeed,2,6,3,2,4.8214283F,2.14285707F,pi,0,516});}
Routine PassingBlueFans(Actor& a) {co_await Passing(a,40,{Formation::AimedFan,0,6,6,1,4.01785707F,2.14285707F,0,0.196349546F,516});}
Routine PassingRedFans(Actor& a) {co_await Passing(a,30,{Formation::AimedFan,4,2,4,1,4.01785707F,2.14285707F,0,0.261799395F,516});}
Routine PassingRandomRedRings(Actor& a) {co_await Passing(a,10,{Formation::RandomAngleSpeed,3,2,4,1,4.01785707F,2.14285707F,pi,-pi,516});}
Routine DropFour(Actor& a) {a.dropEvents.push_back(4);a.finished=true;co_return;}
Routine DropSix(Actor& a) {a.dropEvents.push_back(6);a.finished=true;co_return;}
void ConfigureBehavior(Actor& a,bool spatial) {
    a.skills.bulletBase=kNativeBulletBase;a.skills.laserBase=kNativeLaserBase;a.skills.fallback=kBlueBullet;a.skills.spatial=spatial;
    if(!spatial)a.liftedMotion.reset();
}
}
