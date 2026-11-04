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
    co_await Stop(a);float speed=4.01785707F;const int d=native_combat::DifficultyIndex(a.difficulty);
    for(int burst=0;burst<(shortWave ? 5 : 8);++burst) {
        co_await a.At(burst*(shortWave ? 4 : 5));Fire(a,{Formation::AimedRing,3,6,shortWave ? (d==0?12:d==1?24:32) : d==0?16:d==1?30:40,1,speed,2.67857143F,0,0,516});
        speed+=shortWave ? 1.47321427F : 1.01785719F;
        // Lunatic 在基础增量后另加一次速度；保留两次浮点加法的顺序。
        if(d==3)speed+=shortWave?0.535714269F:0.267857134F;
    }
    co_await a.At(shortWave ? 20 : 40);a.motion.Polar(pi/2,4.8214283F);co_await Lifetime(a);
}
Routine LayeredRings(Actor& a,int color) {
    co_await Stop(a);float speed=4.01785707F;const int d=native_combat::DifficultyIndex(a.difficulty);
    const int pairs=d==0?0:d==1?2:4,tail=d==0?5:d==1?4:3;
    // Easy 的源跳转跳过前段并推进局部时钟；实际在停顿结束当帧开始尾段，不能额外等十帧。
    for(int pair=0;pair<pairs;++pair)for(int half=0;half<2;++half) {
        co_await a.At(pair*10+half*5);
        Fire(a,{Formation::AimedRing,3,color,24,d==3&&half==0?2:1,speed,2.67857143F,half==0?0.130899698F:0,0,516});
        speed+=0.535714269F;
    }
    for(int burst=0;burst<tail;++burst) {
        co_await a.At(pairs*10+burst*5);
        Fire(a,{Formation::AimedRing,3,color,24,1,speed,2.67857143F,0,0,516});speed+=0.669642866F;
    }
    co_await a.At(pairs*10+tail*5);a.motion.Polar(pi/2,4.8214283F);co_await Lifetime(a);
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
    co_await Stop(a);const int d=native_combat::DifficultyIndex(a.difficulty),bursts=d==0?20:40;
    for(int burst=0;burst<bursts;++burst) {
        co_await a.At(burst*3);const float angle=a.Random(3.92699075F)-0.392699093F,speed=a.Random(5.35714293F)+2.67857143F;
        Fire(a,{Formation::FixedFan,1,6,d==3?7:5,d>=2?2:1,speed,2.67857143F,angle,0.0392699093F,516});
    }
    co_await a.At(bursts*3);a.motion.Polar(pi/2,4.8214283F);co_await Lifetime(a);
}
Routine PassingRandomCrystals(Actor& a) {
    const int d=native_combat::DifficultyIndex(a.difficulty);
    co_await Passing(a,d<2?60:d==2?40:30,{Formation::RandomAngleSpeed,2,6,d<2?3:d==2?4:5,d==0?1:2,
        4.8214283F,2.14285707F,pi,0,516});
}
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
