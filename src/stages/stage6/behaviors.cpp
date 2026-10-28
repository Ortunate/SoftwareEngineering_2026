// 本文件编排六面晶体的弯曲横穿、一次蓝环与隐藏环境位置；不包含 ECL 分派或原始坐标换算。
#include "stages/stage6/behaviors.hpp"
#include "stages/stage6/ids.hpp"
#include <numbers>
namespace koumakyou::stage6 {
namespace {
using behavior::Actor;using behavior::Routine;using skill::Formation;
constexpr float pi=std::numbers::pi_v<float>;
Routine Lifetime(Actor& a) {co_await a.At(std::max(0,10000-static_cast<int>(a.phaseFrames)));a.finished=true;}
Routine Hook(Actor& a,bool red) {
    skill::Shot shot{Formation::RandomAngleSpeed,2,red ? 2 : 6,red ? 9 : 6,red ? 1 : 2,red ? 5.35714293F : 4.28571415F,2.67857143F,pi,-pi,4};
    if(a.skills.spatial)shot.spatialCount=shot.count*3;a.skills.projectile.sampleAtStepStart=true;a.ConfigureEmitter(shot);
    const int period=80+16-32*a.skills.rank/32;a.SetEmitterPeriod(period,static_cast<int>(a.RandomIndex(period)));
    co_await a.At(30);a.motion.Apply({.angularVelocity=red ? 2.09439516F : -3.92699099F});
    co_await a.At(red ? 90 : 115);a.motion.Apply({.angularVelocity=0});a.SetEmitterPeriod(0);co_await Lifetime(a);
}
Routine Ring(Actor& a) {
    co_await a.At(40);a.motion.Apply({.acceleration=-10.7142859F});a.Random(2*pi);a.soundEvents.push_back(7);
    skill::Shot shot{Formation::AimedRing,2,6,60,1,4.28571415F,2.67857143F,0,0,4};
    if(a.skills.spatial)shot.spatialCount=shot.count*3;a.skills.projectile.sampleAtStepStart=true;a.Fire(shot);
    a.motion.Apply({.acceleration=0});a.motion.Polar(a.Random(pi/2)+pi/4,4.8214283F);co_await Lifetime(a);
}
}
Routine AmbientPositions(Actor& a) {
    a.visible=false;a.interactable=false;
    for(int pulse=0;pulse<54;++pulse) {
        co_await a.At((pulse+1)*40);a.motion.position.x=a.Random(17.1428571F)-8.57142857F;a.motion.position.z=-a.Random(10);
    }
    a.finished=true;
}
Routine BlueHook(Actor& a) {co_await Hook(a,false);}
Routine RedHook(Actor& a) {co_await Hook(a,true);}
Routine CrossingRing(Actor& a) {co_await Ring(a);}
Routine DescendingRing(Actor& a) {co_await Ring(a);}
Routine DropEight(Actor& a) {a.dropEvents.push_back(8);a.finished=true;co_return;}
void ConfigureBehavior(Actor& a,bool spatial) {a.skills.bulletBase=kNativeBulletBase;a.skills.laserBase=kNativeLaserBase;a.skills.fallback=kBlueBullet;a.skills.spatial=spatial;if(!spatial)a.liftedMotion.reset();}
}
