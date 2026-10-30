// 本文件实现第 4 面普通演员的命名行为与资源绑定；Boss 专属行为由角色模块拥有。
#include "stages/stage4/behaviors.hpp"
#include "stages/stage4/ids.hpp"
#include "gameplay/data/native_combat.hpp"
#include "gameplay/spatial.hpp"
#include <numbers>
#include <cmath>
namespace koumakyou::stage4 {
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
Routine Lifetime(Actor& a) {co_await a.At(std::max(0,10000-static_cast<int>(a.phaseFrames)));a.finished=true;}
Routine Spawner(Actor& a,bool laser=false) {
    co_await a.At(40);a.motion.Apply({.acceleration=-10.7142859F});
    co_await a.At(70);
    auto pattern=laser ? kHiddenLaserEmitter : kHiddenRandomEmitter;pattern.spatialShots=a.skills.spatial;
    a.spawnEvents.push_back({.position=a.Position(),.pattern=pattern,.sourceHealth=1000,.sourceScore=10,
        .seed=++a.spawnIndex,.sourceItem=0,.visual=kOrb,.hitRadius=0.625F,
        .bulletBase=kNativeBulletBase,.laserBase=kNativeLaserBase,.defeatBase=kNativeDefeat,.fallback=kBlueBullet,
        .retireOutside=false,.damageable=false});
    a.motion.Apply({.acceleration=0});a.motion.Polar(a.Random(pi/2)+pi/4,4.8214283F);co_await Lifetime(a);
}
/// @brief 自动射击的参数在配置时快照；首发按整数初相并在内容帧末递增，停止帧先关闭再判断发射。
Routine Periodic(Actor& a,int interval,int end,skill::Shot shot,movement::Path changes) {
    a.contactEnabled=false;
    interval+=interval/5-((interval/5)*2*a.skills.rank)/32;
    int next=interval-static_cast<int>(a.RandomIndex(interval))-1;int change=0;
    const auto context=a.skills;
    if(a.skills.spatial)shot.spatialCount=shot.count*3;
    for(int frame=0;frame<=end;++frame) {
        co_await a.At(frame);
        while(change<changes.count&&changes.keys[change].frame==frame)a.motion.Apply(changes.keys[change++]);
        if(frame==next&&frame<end) {a.FireAt(shot,a.Position(),&context);a.soundEvents.push_back(22);next+=interval;}
    }
    co_await Lifetime(a);
}
constexpr movement::Path kHookChanges{.keys={{{.frame=30,.angularVelocity=-3.92699099F},{.frame=70,.angularVelocity=0}}},.count=2};
constexpr movement::Path kSwerveChanges{.keys={{{.frame=30,.angularVelocity=3.92699099F},{.frame=70,.angularVelocity=-7.85398197F},
    {.frame=150,.angularVelocity=0}}},.count=3};
Routine Spiral(Actor& a,float first,float last,int count=3,int layers=2) {
    co_await a.At(40);a.motion.Apply({.acceleration=-10.7142859F});
    co_await a.At(70);a.motion.Apply({.acceleration=0});float angle=a.Random(2*pi)-pi;
    const float step=a.RandomIndex(2)==0 ? 0.241660967F : -0.241660967F;
    for(int i=0;i<64;++i) {
        co_await a.At(70+i*8);Fire(a,{Formation::FixedRing,2,6,count,layers,first,last,angle,0,4});a.soundEvents.push_back(7);angle+=step;
    }
    co_await a.At(582);a.motion.Polar(-pi/2,5.35714293F);co_await Lifetime(a);
}
Routine RingEmitter(Actor& a,int repeats,bool blue,bool offset) {
    a.interactable=false;co_await a.At(30);a.interactable=true;
    const int d=native_combat::DifficultyIndex(a.difficulty);
    for(int i=0;i<repeats;++i) {
        co_await a.At(30+i*50);
        if(blue) {
            constexpr int counts[]{8,16,24,32},layers[]{1,1,2,3};
            constexpr float speeds[]{3.75F,3.75F,6.4285717F,8.03571415F};
            Fire(a,{Formation::AimedRing,3,6,counts[d],layers[d],speeds[d],2.67857143F,0,0,5});
        }
        else if(offset) {
            Fire(a,{Formation::AimedOffsetRing,0,10,11,1,3.21428585F,2.67857143F,0,0,5});
            Fire(a,{Formation::AimedRing,3,10,11,1,3.21428585F,2.67857143F,0,0,5});
        } else {
            constexpr int counts[]{7,11,12,15},firstLayers[]{1,1,2,2},lastLayers[]{1,1,1,2};
            constexpr float speeds[]{3.21428585F,3.21428585F,4.28571415F,6.69642878F};
            const float angle=a.Random(2*pi)-pi,spacing=d==0?0.448798954F:0.285599321F;
            Fire(a,{Formation::AimedRing,0,10,counts[d],firstLayers[d],speeds[d],2.67857143F,angle,0,5});
            Fire(a,{Formation::AimedRing,3,10,counts[d],lastLayers[d],speeds[d],2.67857143F,angle+spacing,0,5});
        }
        a.soundEvents.push_back(8);
    }
    co_await a.At(30+repeats*50);a.interactable=false;a.InterruptAnimation(1);
    co_await a.At(60+repeats*50);a.visible=false;a.finished=true;
}
}
Routine DescendingSpawner(Actor& a) {co_await Spawner(a);}
Routine CrossingSpawner(Actor& a) {co_await Spawner(a);}
Routine LaserSpawner(Actor& a) {co_await Spawner(a,true);}
Routine HookFanShooter(Actor& a) {
    constexpr int counts[]{1,3,5,8};const int d=native_combat::DifficultyIndex(a.difficulty);
    co_await Periodic(a,50,70,{Formation::AimedFan,2,6,counts[d],2,4.8214283F,2.67857143F,0,d==3?0.261799395F:0.157079637F,4},kHookChanges);
}
Routine SwerveFanShooter(Actor& a) {
    constexpr int counts[]{1,3,5,9};const int d=native_combat::DifficultyIndex(a.difficulty);
    co_await Periodic(a,50,150,{Formation::AimedFan,2,6,counts[d],2,4.8214283F,2.67857143F,0,d==3?0.261799395F:0.157079637F,4},kSwerveChanges);
}
Routine HookLayerShooter(Actor& a) {
    constexpr int counts[]{1,1,3,5},layers[]{1,3,2,3};const int d=native_combat::DifficultyIndex(a.difficulty);
    co_await Periodic(a,128,70,{Formation::AimedFan,2,6,counts[d],layers[d],4.01785707F,1.875F,0,d==3?0.261799395F:0.17453292F,4},kHookChanges);
}
Routine SwerveLayerShooter(Actor& a) {
    constexpr int counts[]{1,1,2,5};const int d=native_combat::DifficultyIndex(a.difficulty);
    co_await Periodic(a,128,150,{Formation::AimedFan,2,6,counts[d],3,4.01785707F,1.875F,0,d==3?0.261799395F:0.17453292F,4},kSwerveChanges);
}
Routine DescendingFanShooter(Actor& a) {
    constexpr int counts[]{2,3,5,9};const int d=native_combat::DifficultyIndex(a.difficulty);
    co_await Periodic(a,50,100,{Formation::AimedFan,2,6,counts[d],2,d>=2?6.16071415F:5.35714293F,3.21428585F,0,0.17453292F,4},{});
}
Routine CrossingFanShooter(Actor& a) {co_await DescendingFanShooter(a);}
Routine PassingHookCrystal(Actor& a) {
    a.contactEnabled=false;co_await a.At(30);a.motion.Apply({.angularVelocity=-3.92699099F});
    co_await a.At(70);a.motion.Apply({.angularVelocity=0});co_await Lifetime(a);
}
Routine PassingRisingCrystal(Actor& a) {
    a.contactEnabled=false;co_await a.At(30);a.motion.Apply({.angularVelocity=3.92699099F});
    co_await a.At(110);a.motion.Apply({.angularVelocity=0});co_await Lifetime(a);
}
Routine StrongSpiralCrystal(Actor& a) {
    constexpr int counts[]{1,3,5,9},layers[]{1,2,3,3};
    constexpr float speeds[]{6.96428537F,6.96428537F,8.03571415F,8.57142925F};
    const int d=native_combat::DifficultyIndex(a.difficulty);co_await Spiral(a,speeds[d],4.8214283F,counts[d],layers[d]);
}
Routine GentleSpiralCrystal(Actor& a) {co_await Spiral(a,5.625F,3.75F);}
Routine CrystalDefeated(Actor& a) {a.ClearBullets();a.dropEvents.push_back(3);a.finished=true;co_return;}
Routine GreenRingEmitter(Actor& a) {co_await RingEmitter(a,60,false,false);}
Routine BlueRingEmitter(Actor& a) {co_await RingEmitter(a,10,true,false);}
Routine OffsetRingEmitter(Actor& a) {co_await RingEmitter(a,6,false,true);}
Routine EmitterDefeated(Actor& a) {a.interactable=false;a.InterruptAnimation(1);co_await a.At(30);a.visible=false;a.finished=true;}
Routine HiddenRandomEmitter(Actor& a) {
    a.damageable=false;a.interactable=false;
    for(int i=0;i<12;++i) {
        co_await a.At(30+i*6);a.skills.offset={a.Random(3.57142854F)-1.78571427F,0,-(a.Random(3.57142854F)-1.78571427F)};
        const float angle=a.Random(2*pi)-pi,speed=a.Random(2.67857143F)+2.67857143F;
        constexpr int counts[]{2,4,8,16};
        Fire(a,{Formation::FixedRing,4,6,counts[native_combat::DifficultyIndex(a.difficulty)],1,speed,0,angle,0.0981747732F,4});a.soundEvents.push_back(7);
    }
    co_await a.At(102);a.InterruptAnimation(1);co_await a.At(222);a.visible=false;a.finished=true;
}
Routine HiddenLaserEmitter(Actor& a) {
    a.damageable=false;a.interactable=false;
    co_await a.At(120);a.Fire(skill::Laser{.color=6,.length=22.3214283F,.width=1.42857146F,
        .grow=90,.sustain=120,.fade=16,.warning=70,.harmlessTail=14,.angle=pi/2,.aimed=false});a.soundEvents.push_back(16);
    const int d=native_combat::DifficultyIndex(a.difficulty);
    if(d>=2)Fire(a,{Formation::AimedRing,4,6,d==2?16:32,d==2?1:2,3.21428585F,0,0,0.0981747732F,4});
    co_await a.At(320);a.InterruptAnimation(1);co_await a.At(440);a.visible=false;a.finished=true;
}
void ConfigureBehavior(Actor& a,bool spatial) {
    a.skills.bulletBase=kNativeBulletBase;a.skills.laserBase=kNativeLaserBase;a.skills.fallback=kBlueBullet;a.skills.spatial=spatial;
    if(!spatial)a.liftedMotion.reset();
}

}
