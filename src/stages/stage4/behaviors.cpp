// 本文件编排四面普通晶体、小恶魔和帕秋莉命名行为；时间轴只传出生配置，不包含攻击循环或原子程序分派。
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
Routine Spiral(Actor& a,float first,float last) {
    co_await a.At(40);a.motion.Apply({.acceleration=-10.7142859F});
    co_await a.At(70);a.motion.Apply({.acceleration=0});float angle=a.Random(2*pi)-pi;
    const float step=a.RandomIndex(2)==0 ? 0.241660967F : -0.241660967F;
    for(int i=0;i<64;++i) {
        co_await a.At(70+i*8);Fire(a,{Formation::FixedRing,2,6,3,2,first,last,angle,0,4});a.soundEvents.push_back(7);angle+=step;
    }
    co_await a.At(582);a.motion.Polar(-pi/2,5.35714293F);co_await Lifetime(a);
}
Routine RingEmitter(Actor& a,int repeats,bool blue,bool offset) {
    a.interactable=false;co_await a.At(30);a.interactable=true;
    for(int i=0;i<repeats;++i) {
        co_await a.At(30+i*50);
        if(blue)Fire(a,{Formation::AimedRing,3,6,16,1,3.75F,2.67857143F,0,0,5});
        else if(offset) {
            Fire(a,{Formation::AimedOffsetRing,0,10,11,1,3.21428585F,2.67857143F,0,0,5});
            Fire(a,{Formation::AimedRing,3,10,11,1,3.21428585F,2.67857143F,0,0,5});
        } else {
            const float angle=a.Random(2*pi)-pi;
            Fire(a,{Formation::AimedRing,0,10,11,1,3.21428585F,2.67857143F,angle,0,5});
            Fire(a,{Formation::AimedRing,3,10,11,1,3.21428585F,2.67857143F,angle+0.285599321F,0,5});
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
Routine HookFanShooter(Actor& a) {co_await Periodic(a,50,70,{Formation::AimedFan,2,6,3,2,4.8214283F,2.67857143F,0,0.157079637F,4},kHookChanges);}
Routine SwerveFanShooter(Actor& a) {co_await Periodic(a,50,150,{Formation::AimedFan,2,6,3,2,4.8214283F,2.67857143F,0,0.157079637F,4},kSwerveChanges);}
Routine HookLayerShooter(Actor& a) {co_await Periodic(a,128,70,{Formation::AimedFan,2,6,1,3,4.01785707F,1.875F,0,0.17453292F,4},kHookChanges);}
Routine SwerveLayerShooter(Actor& a) {co_await Periodic(a,128,150,{Formation::AimedFan,2,6,1,3,4.01785707F,1.875F,0,0.17453292F,4},kSwerveChanges);}
Routine DescendingFanShooter(Actor& a) {co_await Periodic(a,50,100,{Formation::AimedFan,2,6,3,2,5.35714293F,3.21428585F,0,0.17453292F,4},{});}
Routine CrossingFanShooter(Actor& a) {co_await Periodic(a,50,100,{Formation::AimedFan,2,6,3,2,5.35714293F,3.21428585F,0,0.17453292F,4},{});}
Routine PassingHookCrystal(Actor& a) {
    a.contactEnabled=false;co_await a.At(30);a.motion.Apply({.angularVelocity=-3.92699099F});
    co_await a.At(70);a.motion.Apply({.angularVelocity=0});co_await Lifetime(a);
}
Routine PassingRisingCrystal(Actor& a) {
    a.contactEnabled=false;co_await a.At(30);a.motion.Apply({.angularVelocity=3.92699099F});
    co_await a.At(110);a.motion.Apply({.angularVelocity=0});co_await Lifetime(a);
}
Routine StrongSpiralCrystal(Actor& a) {co_await Spiral(a,6.96428537F,4.8214283F);}
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
        Fire(a,{Formation::FixedRing,4,6,4,1,speed,0,angle,0.0981747732F,4});a.soundEvents.push_back(7);
    }
    co_await a.At(102);a.InterruptAnimation(1);co_await a.At(222);a.visible=false;a.finished=true;
}
Routine HiddenLaserEmitter(Actor& a) {
    a.damageable=false;a.interactable=false;
    co_await a.At(120);a.Fire(skill::Laser{.color=6,.length=22.3214283F,.width=1.42857146F,
        .grow=90,.sustain=120,.fade=16,.warning=70,.harmlessTail=14,.angle=pi/2,.aimed=false});a.soundEvents.push_back(16);
    co_await a.At(320);a.InterruptAnimation(1);co_await a.At(440);a.visible=false;a.finished=true;
}
void ConfigureBehavior(Actor& a,bool spatial) {
    a.skills.bulletBase=kNativeBulletBase;a.skills.laserBase=kNativeLaserBase;a.skills.fallback=kBlueBullet;a.skills.spatial=spatial;
    if(!spatial)a.liftedMotion.reset();
}
namespace {
movement::State& Motion(Actor& a) {
    if(a.skills.spatial&&!a.liftedMotion)a.liftedMotion.emplace(a.motion);
    return a.liftedMotion ? a.liftedMotion->planar : a.motion;
}
void Height(Actor& a) {if(a.liftedMotion)a.liftedMotion->Aim(a.motion,-1.25F+2.5F*spatial::RadicalInverse(a.spatialIndex++,2));}
void MoveTo(Actor& a,int frames,Vector3 target) {Motion(a).Interpolate(frames,target,2);Height(a);}
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
int Period(const Actor& a,int frames) {return frames+frames/5-(frames/5*2*a.skills.rank)/32;}
void ConfigureEmitter(Actor& a,skill::Shot shot) {
    a.skills.projectile.sampleAtStepStart=true;
    if(a.skills.spatial)shot.spatialCount=shot.count*3;
    a.ConfigureEmitter(shot);
}
}
namespace koakuma {
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
Routine Encounter(Actor& a) {
    a.SetEmitterPeriod(0);a.ClearBullets();a.defeatActors=true;a.interactable=false;a.Animate(64);a.deathEffect=2;
    a.motion.position={0,a.motion.position.y,4.28571415F};
    Motion(a).bounds={{-7.14285714F,2.14285714F},{7.14285714F,7.85714286F}};Motion(a).clamped=true;
    a.timeoutFrames=native_combat::RequireLiteral(native_combat::Setting(4,21,115));a.timeoutCallback=Timeout;a.deathCallback=Defeated;
    co_await a.At(30);a.interactable=true;
    for(;;) {
        co_await SpiralWave(a,0.196349546F);Wander(a,4.01785707F,90);co_await SummonPair(a);
        co_await a.At(90);co_await SpiralWave(a,-0.196349546F);co_await SummonPair(a);
        co_await a.At(91);a.Rewind(30);
    }
}
Routine Defeated(Actor& a) {
    a.SetEmitterPeriod(0);a.ClearBullets();a.interactable=false;a.itemEvents.push_back(3);a.dropEvents.push_back(8);a.finished=true;co_return;
}
Routine Timeout(Actor& a) {a.SetEmitterPeriod(0);a.ClearBullets();a.interactable=false;a.finished=true;co_return;}
}
namespace patchouli {
namespace {
void Initialize(Actor& a) {
    a.SetPoses({65,69,70,67,68});a.deathEffect=2;
    Motion(a).bounds={{-7.14285714F,3.57142857F},{7.14285714F,7.85714286F}};Motion(a).clamped=true;
}
Vector3 LaserDirection(const Actor& a,int slot,float angle) {
    if(!a.skills.spatial)return {std::cos(angle),0,-std::sin(angle)};
    // 四条光束直接分配四个等面积球带；第二组独立反向旋转，不复制某个平面形成厚片。
    return spatial::Frame::FromAxis({0,0,-1}).Direction(-0.75F+0.5F*(slot%4),angle);
}
void BeginLaser(Actor& a,int slot,float angle,bool second,bool red) {
    a.FireBeam(slot,skill::Laser{.color=red ? 2 : 6,.length=19.4642849F,.width=1.07142854F,
        .grow=30,.sustain=second ? 90.0F : 70.0F,.fade=20,.warning=30,.harmlessTail=14,
        .aimed=false,.startOffset=2.85714293F,.direction=LaserDirection(a,slot,angle),.animationScript=1});
}
void RotateLaser(Actor& a,int slot,float angle) {
    if(auto* beam=a.Beam(slot)) {
        beam->beamDirection=LaserDirection(a,slot,angle);
        beam->position={beam->beamAnchor.x+beam->beamDirection.x*beam->beamStartOffset,
            beam->beamAnchor.y+beam->beamDirection.y*beam->beamStartOffset,
            beam->beamAnchor.z+beam->beamDirection.z*beam->beamStartOffset};
    }
}
/// @brief 新典 121(12) 原位旋转的第二分量读取已改写的第一分量，必须保留此求值顺序。
/// 中性 X/Z 不把它“修正”为标准旋转；空间适配仅将该异常半径沿对应光束方向分配。
void EmitFromLasers(Actor& a,const std::array<float,8>& angles) {
    for(int slot=0;slot<8;++slot)if(const auto* beam=a.Beam(slot)) {
        const float x=2.85714293F*std::cos(angles[slot]),z=x*std::sin(angles[slot]);
        Vector3 offset{x,0,z};
        if(a.skills.spatial) {const float length=std::hypot(x,z);offset={beam->beamDirection.x*length,beam->beamDirection.y*length,beam->beamDirection.z*length};}
        const auto p=a.Position();a.FireEmitterAt({p.x+offset.x,p.y+offset.y,p.z+offset.z});
    }
}
Routine LaserCycle(Actor& a,bool reverse,bool red,int cycle) {
    ConfigureEmitter(a,{Formation::AimedRing,3,2,reverse ? 1 : 10,reverse ? 2 : 1,5.35714293F,3.21428585F,0,0,4});
    a.ClearBeamSlots();std::array<float,8> angles{pi/4,3*pi/4,-3*pi/4,-pi/4,pi/4,3*pi/4,-3*pi/4,-pi/4};
    for(int slot=0;slot<4;++slot)BeginLaser(a,slot,angles[slot],false,red);a.soundEvents.push_back(16);
    const float step=reverse ? -0.00682954909F : 0.00682954909F;
    for(int frame=0;frame<180;++frame) {
        co_await a.At(frame);
        if(frame<120)for(int slot=0;slot<4;++slot) {angles[slot]+=step;RotateLaser(a,slot,angles[slot]);}
        if(frame==60) {for(int slot=4;slot<8;++slot)BeginLaser(a,slot,angles[slot],true,red);a.soundEvents.push_back(16);}
        if(frame>=60)for(int slot=4;slot<8;++slot) {angles[slot]-=step;RotateLaser(a,slot,angles[slot]);}
        if(cycle!=0&&frame%50==0)EmitFromLasers(a,angles);
    }
    co_await a.At(180);
}
Routine Maneuver(Actor& a,bool second,int cycle) {
    Wander(a,6.69642878F,90);
    ConfigureEmitter(a,{Formation::AimedRing,3,second ? 6 : 2,cycle+(second ? 10 : 16),second ? 3 : 4,
        second ? 10.7142859F : 9.375F,second ? 4.01785707F : 3.21428585F,0,0,4});
    a.SetEmitterSound(7);a.SetEmitterPeriod(Period(a,second ? 15 : 40));
    co_await a.At(90);MoveTo(a,90,{0,0,4.28571415F});
    co_await a.At(180);
    a.SetEmitterPeriod(second ? Period(a,60) : 0);
    if(second)ConfigureEmitter(a,{Formation::AimedRing,3,6,12,1,4.01785707F,2.67857143F,0,0,4});
}
Routine Nonspell(Actor& a,bool second) {
    Initialize(a);a.SetEmitterPeriod(0);a.damageable=true;a.interactable=true;a.contactEnabled=true;a.skills.offset={0,0,0.535714269F};
    if(second) {a.ClearBullets();a.defeatActors=true;a.dropEvents.push_back(8);}
    for(int cycle=0;;++cycle) {
        co_await a.At(second ? 120 : 40);a.Animate(66);
        co_await a.At(second ? 180 : 100);co_await LaserCycle(a,cycle%2==0,second,cycle);
        co_await Maneuver(a,second,cycle);a.Rewind(second ? 120 : 40);
    }
}
}
Routine Entrance(Actor& a) {
    Initialize(a);a.damageable=false;a.contactEnabled=false;a.skills.offset={0,0,0.535714269F};
    // 入场从战斗边界外开始，源边界在插值完成后才启用，不能提前夹断左上方入场路线。
    Motion(a).clamped=false;MoveTo(a,60,{0,0,4.28571415F});co_await a.At(60);Motion(a).clamped=true;
    co_await a.At(1000000);
}
Routine FirstNonspell(Actor& a) {co_await Nonspell(a,false);}
Routine SecondNonspell(Actor& a) {co_await Nonspell(a,true);}
namespace {
Routine PrepareSpell(Actor& a) {
    a.SetEmitterPeriod(0);a.ClearBullets();a.defeatActors=true;a.SetEmitterSound(-1);
    a.damageable=false;a.skills.offset={};MoveTo(a,120,{0,0,6.42857143F});
    co_await a.At(120);a.damageable=true;a.Animate(66);a.SetPoses({66,66,66,67,68});
}
Routine FireRings(Actor& a,bool advanced) {
    for(int cycle=0;;++cycle) {
        const int count=cycle/3+(advanced ? 8 : 11);
        for(int burst=0;burst<5;++burst) {
            co_await a.At(burst*8);
            for(int ring=0;ring<(advanced ? 2 : 3);++ring) {
                a.skills.projectile={.angularVelocity=ring==1 ? -1.47262156F : 1.47262156F,.angularFrames=128};
                const float speed=advanced ? 5.35714293F : ring==0 ? 5.89285707F : ring==1 ? 4.01785707F : 1.875F;
                Fire(a,{Formation::FixedRing,7,0,count,advanced ? 2 : 1,speed,advanced ? 3.21428585F : 1.875F,a.Random(2*pi)-pi,0,32});
            }
            a.soundEvents.push_back(9);
        }
        co_await a.At(160);Wander(a,4.01785707F,90);co_await a.At(170);a.Rewind(0);
    }
}
/// @brief 左右边缘交替读随机位置；奇数迭代的源条件跳转先将局部时钟推进两帧，因而两发同刻到期。
Routine LeafRain(Actor& a,bool advanced,bool forest) {
    const int halfPeriod=forest ? 11 : 5;
    const Vector3 acceleration=advanced ? Vector3{1.36370504F,0,-1.36370504F} : Vector3{-1.36370504F,0,-1.36370504F};
    for(int iteration=0;;++iteration) {
        const int top=iteration/2*(halfPeriod*2)+(iteration%2 ? halfPeriod+1 : 0);
        co_await a.At(top);
        if(iteration%(forest ? 4 : 6)==0) {
            a.skills.offset={};a.skills.projectile={.accelerationVector=acceleration};
            Fire(a,{Formation::RandomAngleSpeed,forest ? 7 : 5,forest ? 0 : advanced ? 13 : 11,
                forest ? 22 : advanced ? 15 : 10,1,5.35714293F,0.803571429F,pi,-pi,19});a.soundEvents.push_back(26);
        }
        a.skills.projectile={.accelerationVector=acceleration,.accelerationFrames=(advanced||forest) ? 60.0F : -1.0F};
        auto p=a.Position();a.skills.offset={a.Random(17.1428571F)-8.57142857F-p.x,0,8.57142857F-p.z};
        const float first=advanced ? 1.06465089F : 2.63544726F,last=advanced ? 0.506145477F : 2.07694173F;
        Fire(a,{Formation::RandomAngleSpeed,5,forest ? 2 : 10,1,1,3.21428585F,0.803571429F,first,last,19});
        co_await a.At(top+(iteration%2==0 ? 2 : 0));p=a.Position();
        const float z=10-a.Random(8.57142857F)-(iteration%2 ? 8.57142857F : 0);
        a.skills.offset={(forest ? 8.39285714F : advanced ? -9.10714286F : 9.10714286F)-p.x,0,z-p.z};
        Fire(a,{Formation::RandomAngleSpeed,5,forest ? 2 : 11,1,1,3.21428585F,0.803571429F,first,last,19});
    }
}
Routine TurningOrbs(Actor& a,bool advanced) {
    for(;;) {
        a.skills.projectile={.delayFrames=advanced ? 50.0F : 180.0F,.turnAngle=a.Random(pi)-pi/2,
            .turnSpeed=advanced ? 5.89285707F : 3.75F,.turnCount=advanced ? 2 : 1};
        Fire(a,{Formation::RandomAngleSpeed,1,advanced ? 12 : 13,10,1,advanced ? 8.83928585F : 6.4285717F,
            advanced ? 6.16071415F : 2.67857143F,pi,0,67});a.soundEvents.push_back(22);
        if(a.PlayerPosition().z>a.Position().z)Fire(a,{Formation::AimedFan,3,6,7,2,7.5F,3.21428585F,0,0.34906584F,4});
        co_await a.At(advanced ? 9 : 12);a.Rewind(0);
    }
}
}
Routine AgniShine(Actor& a) {co_await PrepareSpell(a);co_await FireRings(a,false);}
Routine AgniShineAdvanced(Actor& a) {co_await PrepareSpell(a);co_await FireRings(a,true);}
Routine SylphyHorn(Actor& a) {co_await PrepareSpell(a);co_await LeafRain(a,false,false);}
Routine SylphyHornAdvanced(Actor& a) {co_await PrepareSpell(a);co_await LeafRain(a,true,false);}
Routine ForestBlaze(Actor& a) {co_await PrepareSpell(a);co_await LeafRain(a,false,true);}
Routine LazyTrilithon(Actor& a) {co_await PrepareSpell(a);co_await TurningOrbs(a,false);}
Routine LazyTrilithonAdvanced(Actor& a) {co_await PrepareSpell(a);co_await TurningOrbs(a,true);}
Routine PrincessUndine(Actor& a) {
    co_await PrepareSpell(a);a.ElapseLocalFrames(120);
    for(int cycle=0;;++cycle) {
        ConfigureEmitter(a,{.formation=Formation::AimedFan,.type=3,.color=6,.count=16,.layers=2,.firstSpeed=9.375F,
            .lastSpeed=3.21428585F,.angle=pi,.spacing=0.34906584F,.flags=4,.aimAxisOffset=pi});
        a.SetEmitterSound(7);a.SetEmitterPeriod(Period(a,22));float spread=pi/8;
        for(int burst=0;burst<12;++burst) {
            co_await a.At(120+burst*10);
            const auto origin=a.Position(),player=a.PlayerPosition();
            const auto frame=spatial::Frame::FromAxis({player.x-origin.x,player.y-origin.y,player.z-origin.z});
            for(int ray=0;ray<3;++ray) {
                skill::Laser laser{.color=6,.length=4.28571415F,.width=0.267857134F,
                    .grow=0,.sustain=9999,.fade=30,.warning=0,.harmlessTail=30,.angle=ray==0 ? 0 : ray==1 ? spread : -spread,
                    .speed=10.7142859F,.initialLength=0};
                // 中心束始终瞄准；侧束直接采样球冠，跨轮改变方位而不消耗额外源随机数。
                if(a.skills.spatial)laser.direction=ray==0 ? frame.forward : frame.Direction(std::cos(spread),
                    2*pi*spatial::RadicalInverse(static_cast<unsigned>(cycle*12+burst+1),2)+(ray==2 ? pi : 0));
                a.Fire(laser);
            }
            spread-=0.0261799395F;a.soundEvents.push_back(17);
        }
        co_await a.At(240);a.SetEmitterPeriod(0);Wander(a,4.01785707F,90);float angle=0;
        for(int burst=0;burst<8;++burst) {
            co_await a.At(240+burst*10);
            Fire(a,{Formation::AimedFan,6,3,10,1,6.69642878F,1.875F,angle,0.224399477F,0});
            Fire(a,{Formation::AimedFan,3,6,cycle+1,1,1.875F,1.875F,angle,0.34906584F,0});
            angle-=a.Random(0.224399477F)-0.112199739F;a.soundEvents.push_back(9);
        }
        co_await a.At(320);Wander(a,4.01785707F,90);co_await a.At(370);a.Rewind(120);
    }
}
Routine MetalFatigue(Actor& a) {
    co_await PrepareSpell(a);a.ElapseLocalFrames(120);
    for(;;) {
        const float angle=a.Random(2*pi)-pi;float turn=a.Random(2*pi)-pi;
        for(int burst=0;burst<8;++burst) {
            a.skills.projectile={.delayFrames=60,.turnAngle=turn,.turnSpeed=5.35714293F};
            Fire(a,{Formation::FixedRing,6,6,8,1,10.7142859F,2.67857143F,angle,0,257});turn+=pi/4;
        }
        a.soundEvents.push_back(8);co_await a.At(160);a.Rewind(120);
    }
}
Routine LavaCromlech(Actor& a) {
    co_await PrepareSpell(a);a.ElapseLocalFrames(120);
    for(int cycle=0;;) {
        for(int burst=0;burst<8;++burst) {
            co_await a.At(120+burst*8);Fire(a,{Formation::FixedRing,7,0,12,2,6.69642878F,1.875F,a.Random(2*pi)-pi,0.130899698F,0});a.soundEvents.push_back(9);
        }
        co_await a.At(244);Wander(a,4.01785707F,90);++cycle;
        for(int burst=0;burst<3;++burst) {
            co_await a.At(244+burst*10);a.skills.projectile={.delayFrames=130,.turnAngle=a.Random(pi)-pi/2,.turnSpeed=3.75F};
            Fire(a,{Formation::RandomAngleSpeed,1,13,cycle*2+10,1,6.4285717F,2.67857143F,pi,0,67});a.soundEvents.push_back(22);
        }
        co_await a.At(274);a.Rewind(120);
    }
}
Routine WaterElf(Actor& a) {
    co_await PrepareSpell(a);a.ElapseLocalFrames(120);a.SetEmitterSound(7);
    // 准备行为先开启周期，再由首轮两次显式发射覆盖配方；不能在每次直接发射时重置周期计数。
    ConfigureEmitter(a,{Formation::AimedRing,3,8,24,2,7.5F,3.21428585F,pi,0.34906584F,4});a.SetEmitterPeriod(Period(a,20));
    float angle=0;
    for(int cycle=0;;++cycle) {
        Fire(a,{Formation::AimedRing,3,8,24,2,7.5F,3.21428585F,pi,0.34906584F,4});
        Fire(a,{Formation::AimedFan,6,3,10,1,4.8214283F,1.875F,angle,0.34906584F,0});angle-=a.Random(0.224399477F)-0.112199739F;
        co_await a.At(140);ConfigureEmitter(a,{Formation::AimedRing,3,10,20,2,7.5F,3.21428585F,pi,0.34906584F,4});
        a.SetEmitterSound(7);a.SetEmitterPeriod(Period(a,20));Wander(a,4.01785707F,90);if((cycle+1)%8==0)angle=0;
        co_await a.At(190);a.Rewind(120);
    }
}
Routine MercuryPoison(Actor& a) {
    co_await PrepareSpell(a);a.ElapseLocalFrames(120);
    for(int cycle=0;;++cycle) {
        for(int burst=0;burst<16;++burst) {
            co_await a.At(120+burst*20);a.skills.projectile={.angularVelocity=burst%2 ? -0.7363108F : 0.7363108F,
                .angularFrames=90,.speedAcceleration=0.48214286F};
            Fire(a,{Formation::AimedRing,3,burst%2 ? 8 : 14,cycle+15,2,4.01785707F,2.14285707F,a.Random(2*pi)-pi,
                burst%2 ? -0.34906584F : 0.34906584F,37});a.soundEvents.push_back(7);
        }
        co_await a.At(440);Wander(a,4.01785707F,90);co_await a.At(490);a.Rewind(120);
    }
}
Routine EmeraldMegalith(Actor& a) {
    co_await PrepareSpell(a);a.ElapseLocalFrames(120);
    for(int cycle=0;;++cycle) {
        a.skills.offset={a.Random(12.8571429F)-6.42857143F,0,-(a.Random(5.71428571F)-2.85714286F)};
        Fire(a,{Formation::FixedRing,9,2,1,1,a.Random(5.35714293F)+4.28571415F,2.14285707F,pi/2,0,4});a.soundEvents.push_back(7);
        if(cycle%18==7)Wander(a,4.01785707F,90);
        co_await a.At(121);a.skills.projectile={.delayFrames=180,.turnAngle=a.Random(pi)-pi/2,.turnSpeed=4.5535717F};
        Fire(a,{Formation::RandomAngleSpeed,1,11,12,1,4.8214283F,2.67857143F,pi,-pi,2});a.soundEvents.push_back(22);
        co_await a.At(140);a.Rewind(120);
    }
}
Routine CombinationEntrance(Actor& a) {
    Initialize(a);a.SetEmitterPeriod(0);a.ClearBullets();a.defeatActors=true;a.dropEvents.push_back(8);a.SetEmitterSound(-1);
    a.damageable=false;a.skills.offset={0,0,0.535714269F};co_await a.At(120);
    // 组合符卡包装层在等待后重置阶段总时钟；自身的 120 帧准备期另计，不能合并两段等待。
    a.phaseFrames=0;
}
Routine Combination(Actor& a,behavior::Entry entry,int timeout) {
    co_await CombinationEntrance(a);a.timeoutFrames=timeout;co_await entry(a);
}
Routine PhaseComplete(Actor&) {co_return;}
Routine Defeated(Actor& a) {
    a.SetEmitterPeriod(0);a.damageable=false;a.contactEnabled=false;a.interactable=false;
    a.ClearBullets();Wander(a,1.60714293F,60);co_await a.At(60);a.soundEvents.push_back(18);a.finished=true;
}
}
}
