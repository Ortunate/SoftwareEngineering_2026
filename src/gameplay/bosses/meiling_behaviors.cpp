// 本文件实现 MeilingBoss 内部命名行为；移动、弹幕、演出和随机读取保持原内容时序。
#include "gameplay/bosses/meiling.hpp"
#include "stages/stage3/behaviors.hpp"
#include "stages/stage3/ids.hpp"
#include "gameplay/spatial.hpp"
#include <cmath>
#include <numbers>
namespace koumakyou::bosses {
using namespace stage3;
using behavior::Actor;using behavior::Routine;using skill::Formation;
namespace {

using behavior::Actor;using behavior::Routine;using skill::Formation;
constexpr float pi=std::numbers::pi_v<float>;
movement::State& Motion(Actor& a) {return a.liftedMotion ? a.liftedMotion->planar : a.motion;}
void Height(Actor& a) {
    if(a.liftedMotion)a.liftedMotion->Aim(a.motion,-1.25F+2.5F*spatial::RadicalInverse(a.spatialIndex++,2));
}
void MoveTo(Actor& a,int frames,Vector3 point,int power=2) {
    if(a.skills.spatial&&!a.liftedMotion)a.liftedMotion.emplace(a.motion);
    Motion(a).Interpolate(frames,point,power);Height(a);
}
void Wander(Actor& a,float speed,int frames) {
    if(a.skills.spatial&&!a.liftedMotion)a.liftedMotion.emplace(a.motion);
    a.RandomDirection(-pi,pi);Motion(a).Apply({.speed=speed});Motion(a).Decelerate(frames);Height(a);
}
void Fire(Actor& a,skill::Shot shot) {
    if(a.skills.spatial) {
        shot.spatialCount=shot.count>1||shot.formation==Formation::FixedFan||shot.formation==Formation::AimedFan ? shot.count*3 : 1;
        if(shot.formation==Formation::RandomAngle||shot.formation==Formation::RandomAngleSpeed) {
            // 随机单发也须跨轮推进高度序列；每轮从零采样会使所有雨弹集中在同一个切面。
            shot.sampleIndex=static_cast<int>(a.spatialIndex&0x7fffffffU);a.spatialIndex+=shot.spatialCount*shot.layers;
        }
        if(shot.count==1&&(shot.formation==Formation::FixedRing||shot.formation==Formation::AimedRing)) {
            shot.formation=Formation::AimedFan;shot.sampleIndex=static_cast<int>(a.spatialIndex++&0x7fffffffU);shot.coneHalfAngle=pi;
        }
    }
    a.Fire(shot);
}
/// @brief 停顿前沿用上一帧速度；加速度从 40 帧开启，70 帧关闭，不能提前把负速度钳成零。

/// @brief 普通妖精自动发射以配置时 rank 快照计算周期；读取一次整数初相，帧末先增计数。

void Initialize(Actor& a) {
    a.SetPoses({64,69,70,67,68});a.deathEffect=2;
    Motion(a).bounds={{-7.14285714F,3.57142857F},{7.14285714F,7.85714286F}};Motion(a).clamped=true;
}

}

namespace {
/// @brief 原准备演出按十帧采样；等待消耗本子行为时钟，不推进父行为的局部时间。
Routine Prepare(Actor& a,int frames) {
    a.soundEvents.push_back(25);co_await a.At(frames);a.soundEvents.push_back(7);
}
Routine OffsetRain(Actor& a) {
    float x=a.Random(1.42857146F)-0.714285731F,z=a.Random(1.42857146F)-0.714285731F;
    for(int shot=0;shot<8;++shot) {
        co_await a.At(20+shot*8);a.skills.offset={x,0,-z};
        Fire(a,{Formation::RandomSpeedRing,3,6,16,1,8.03571415F,4.5535717F,0,0,4});
        x-=a.Random(1.42857146F)-0.714285731F;z-=a.Random(1.07142854F)-0.714285731F;a.soundEvents.push_back(8);
    }
    co_await a.At(124);
}
Routine DenseRing(Actor& a) {
    a.skills.offset={};Fire(a,{Formation::FixedRing,1,2,76,1,2.94642878F,1.60714281F,0,0,4});
    a.soundEvents.push_back(26);co_await a.At(10);
}
Routine MidbossRetreat(Actor& a,bool defeated) {
    a.damageable=false;Motion(a).clamped=false;Motion(a).Apply({.speed=0});a.SetPoses({64,69,70,67,68});a.skills.offset={};
    if(defeated) {a.dropEvents.push_back(5);a.soundEvents.push_back(18);}
    co_await a.At(130);a.Animate(68);MoveTo(a,60,{0,0,12.8571424F});
    co_await a.At(180);a.finished=true;
}
Routine FirstCycle(Actor& a,int cycle) {
    float angle=a.Random(2*pi)-pi,speed=5.35714293F;const int count=std::min(cycle+5,10);
    for(int i=0;i<30;++i) {
        co_await a.At(i*4);Fire(a,{Formation::AimedRing,5,2,count,1,speed,1.60714281F,angle,0,4});a.soundEvents.push_back(7);
        angle+=cycle%2==0 ? -0.0981747732F : 0.0981747732F;speed+=0.133928567F;
    }
    co_await a.At(120);Wander(a,13.3928576F,40);a.soundEvents.push_back(25);float spacing=0.0392699093F;
    for(int i=0;i<16;++i) {
        co_await a.At(120+i*2);if((16-i)%5==0)a.soundEvents.push_back(8);
        Fire(a,{Formation::AimedRing,5,2,count,1,speed,1.60714281F,angle,0,4});
        angle+=cycle%2==0 ? -spacing : spacing;spacing+=0.0654498488F;speed+=0.160714284F;
    }
    co_await a.At(152);a.soundEvents.push_back(7);
}
void Summon(Actor& a,float x,float health) {
    auto pattern=kSummonedStream;pattern.spatialShots=a.skills.spatial;
    a.spawnEvents.push_back({.position={x,a.Position().y,11.4285717F},.pattern=pattern,
        .sourceHealth=health,.sourceScore=2000,.seed=++a.spawnIndex,.sourceItem=1,
        .visual=kOrb,.hitRadius=0.625F,.bulletBase=kNativeBulletBase,.laserBase=kNativeLaserBase,
        .defeatBase=kNativeDefeat,.fallback=kBlueBullet,.retirementHeights=Vector2{-10.5F,10.5F}});
}
Routine SummonWave(Actor& a) {
    Summon(a,-7.14285707F,2000);Summon(a,7.14285707F,1000);
    co_await a.At(30);Summon(a,-4.28571415F,500);Summon(a,4.28571415F,2000);
    co_await a.At(60);Summon(a,-1.42857146F,1000);Summon(a,1.42857146F,500);
}
Routine AlternatingRings(Actor& a) {
    for(int i=0;i<3;++i) {
        co_await a.At(i*70);const float angle=a.Random(2*pi)-pi;
        Fire(a,{Formation::FixedRing,3,i%2 ? 6 : 5,16,2,8.03571415F,5.35714293F,angle,0.261799395F,4});a.soundEvents.push_back(8);
    }
}
Routine FallingRings(Actor& a) {
    a.skills.projectile.accelerationVector=Vector3{0,0,-4.01785707F};
    for(int i=0;i<3;++i) {
        co_await a.At(i*70);Fire(a,{Formation::RandomAngleSpeed,3,6,i==1 ? 16 : 14,1,5.35714293F,0.803571463F,0,-pi,25});
        a.soundEvents.push_back(7);
    }
}
Routine FastRandomRings(Actor& a) {
    for(int i=0;i<3;++i) {
        co_await a.At(i*70);const float unused=a.Random(2*pi)-pi;(void)unused;
        Fire(a,{Formation::RandomAngleSpeed,3,5,i==1 ? 16 : 24,1,10.7142859F,3.48214269F,pi,-pi,2});a.soundEvents.push_back(9);
    }
}
Routine BlueRain(Actor& a) {
    a.skills.projectile.accelerationVector=Vector3{0,0,-4.33928585F};a.skills.rankInfluence={};
    for(int i=0;i<40;++i) {
        co_await a.At(i*2);Fire(a,{Formation::RandomAngleSpeed,5,6,1,1,2.67857143F,0.803571463F,0,-pi,19});a.soundEvents.push_back(22);
    }
    co_await a.At(160);
}
Routine TurnedRings(Actor& a) {
    a.skills.rankInfluence={-1.33928573F,2.14285707F,0,0,0,0};
    for(int side=0;side<2;++side) {
        a.skills.projectile={.delayFrames=60,.turnAngle=side ? 2.8972466F : -2.8972466F,.turnSpeed=8.03571415F};
        float speed=5.35714293F;
        for(int i=0;i<6;++i) {
            co_await a.At(side*76+i);Fire(a,{Formation::FixedRing,4,side ? 2 : 6,24,1,speed,5.35714293F,0,0,66});speed+=1.60714281F;
        }
        co_await a.At(side*76+26);a.soundEvents.push_back(9);
    }
}
}
Routine MeilingBoss::Behaviors::Midboss(Actor& a) {
    a.damageable=false;a.contactEnabled=false;Initialize(a);
    Motion(a).clamped=false;MoveTo(a,100,{0,0,3.30357146F},-2);co_await Prepare(a,100);
    Motion(a).clamped=true;a.damageable=true;a.contactEnabled=true;
    for(int i=0;i<160;++i) {
        co_await a.At(30);if(i%2==0)co_await OffsetRain(a);else co_await DenseRing(a);
        Wander(a,13.3928576F,50);co_await Prepare(a,50);co_await a.At(90);if(i<159)a.Rewind(30);
    }
    a.finished=true;
}
Routine MeilingBoss::Behaviors::GorgeousSweetFlower(Actor& a) {
    a.damageable=false;Initialize(a);a.skills.offset={};a.phaseFrames=0;
    MoveTo(a,120,{0,0,3.57142854F});a.SetPoses({71,71,71,67,68});co_await Prepare(a,120);a.damageable=true;
    float left=a.Random(2*pi)-pi,right=a.Random(2*pi)-pi;
    a.skills.rankInfluence={-1.33928573F,0,0,2,0,0};
    for(;;) {
        Fire(a,{Formation::FixedRing,5,13,4,1,5.35714293F,1.60714281F,left,0,4});
        Fire(a,{Formation::FixedRing,5,13,4,1,5.35714293F,1.60714281F,right,0,4});a.soundEvents.push_back(27);
        left=std::remainder(left+0.1134464F,2*pi);right=std::remainder(right-0.130899698F,2*pi);
        for(int i=0;i<6;++i) {
            if(static_cast<int>(a.phaseFrames)%80==0)Fire(a,{Formation::AimedRing,5,2,42,1,3.21428585F,1.60714281F,0,0,4});
            co_await a.At(i+1);
        }
        a.Rewind(0);
    }
}
Routine MeilingBoss::Behaviors::MidbossDefeated(Actor& a) {co_await MidbossRetreat(a,true);}
Routine MeilingBoss::Behaviors::MidbossTimeout(Actor& a) {co_await MidbossRetreat(a,false);}
Routine MeilingBoss::Behaviors::Entrance(Actor& a) {
    Initialize(a);a.damageable=false;a.contactEnabled=false;a.skills.offset={0,0,0.535714269F};
    // 战斗边界须在场外入场完成后启用；对白等待期的下界对应源 176，开战阶段再改为 144。
    Motion(a).clamped=false;MoveTo(a,100,{0,0,3.30357146F},-2);co_await Prepare(a,100);
    Motion(a).bounds.lower.y=2.14285714F;Motion(a).clamped=true;
    co_await a.At(1000000);
}
Routine MeilingBoss::Behaviors::FirstNonspell(Actor& a) {
    a.interactable=true;a.contactEnabled=true;
    Initialize(a);
    a.phaseFrames=0;a.damageable=true;
    for(int cycle=0;;++cycle) {co_await a.At(50);co_await FirstCycle(a,cycle);co_await a.At(170);a.Rewind(50);}
}
Routine MeilingBoss::Behaviors::RainbowWindChime(Actor& a) {
    a.defeatActors=true;a.damageable=false;a.skills.offset={};a.phaseFrames=0;
    MoveTo(a,120,{0,0,7.14285707F});a.SetPoses({71,71,71,67,68});co_await Prepare(a,120);a.damageable=true;
    MoveTo(a,2000,{0,0,2.85714293F},1);float base=0.87266463F;
    constexpr std::array<int,7> colors{2,14,13,11,8,6,4},counts{2,3,4,2,4,3,2};
    for(int cycle=0;;++cycle) {
        float angle=base;
        for(int i=0;i<7;++i) {Fire(a,{Formation::FixedRing,5,colors[i],counts[i],1,6.96428537F,1.60714281F,angle,0,2});angle+=0.1134464F;}
        base+=cycle%96<48 ? 0.130899698F : -0.130899698F;if(base<-pi)base+=2*pi;
        a.soundEvents.push_back(24);co_await a.At(7);a.Rewind(0);
    }
}
Routine MeilingBoss::Behaviors::SummoningNonspell(Actor& a) {
    a.ClearBullets();Initialize(a);a.phaseFrames=0;
    a.dropEvents.push_back(5);a.damageable=true;
    a.skills.rankInfluence={-2.67857143F,2.67857143F,-2,6,0,0};
    for(;;) {
        co_await a.At(160);co_await SummonWave(a);
        for(int i=0;i<4;++i) {
            const auto choice=a.RandomIndex(3);
            if(choice==0)co_await AlternatingRings(a);else if(choice==1)co_await FallingRings(a);else co_await FastRandomRings(a);
            Wander(a,5.35714293F,80);co_await Prepare(a,80);
        }
        co_await a.At(280);a.Rewind(160);
    }
}
Routine MeilingBoss::Behaviors::RainNonspell(Actor& a) {
    a.ClearBullets();a.defeatActors=true;Initialize(a);a.phaseFrames=0;
    a.dropEvents.push_back(5);a.damageable=true;
    for(;;) {
        co_await a.At(160);co_await TurnedRings(a);co_await BlueRain(a);
        Wander(a,10.7142859F,80);co_await Prepare(a,80);co_await a.At(280);a.Rewind(160);
    }
}
Routine MeilingBoss::Behaviors::ColorfulRain(Actor& a) {
    a.ClearBullets();a.defeatActors=true;a.damageable=false;a.skills.offset={};a.phaseFrames=0;
    MoveTo(a,120,{0,0,7.14285707F});a.SetPoses({71,71,71,67,68});co_await Prepare(a,120);a.damageable=true;
    for(;;) {
        a.skills.projectile.accelerationVector=Vector3{0,0,-4.33928585F};
        for(int i=0;i<20;++i) {
            co_await a.At(i*4);for(int color:{6,10,11,8})Fire(a,{Formation::RandomAngleSpeed,5,color,1,1,2.67857143F,0.803571463F,pi,-pi,19});
            a.soundEvents.push_back(22);
        }
        co_await a.At(80);Wander(a,8.03571415F,80);co_await Prepare(a,80);
        for(int i=0;i<20;++i) {
            co_await a.At(100+i*3);
            for(int j=0;j<4;++j) {
                const float angle=std::array{pi,0.0F,2.3561945F,0.785398185F}[j];
                a.skills.projectile.accelerationVector=Vector3{3.85714269F*std::cos(angle),0,-3.85714269F*std::sin(angle)};
                Fire(a,{Formation::RandomAngleSpeed,5,std::array{2,4,13,14}[j],1,1,2.67857143F,0.803571463F,pi,-pi,19});
            }
            a.soundEvents.push_back(22);
        }
        co_await a.At(160);a.Rewind(0);
    }
}
Routine MeilingBoss::Behaviors::DazzlingColorTyphoon(Actor& a) {
    a.ClearBullets();a.defeatActors=true;a.damageable=false;a.phaseFrames=0;
    a.dropEvents.push_back(5);
    co_await a.At(60);a.skills.offset={};MoveTo(a,120,{0,0,4.28571415F});a.SetPoses({71,71,71,67,68});
    co_await Prepare(a,120);a.damageable=true;
    float reverse=a.Random(2*pi)-pi,forward=pi/2;int count=2;
    for(;;) {
        const auto rain=[&](int color,float acceleration,float angle,int directions) {
            a.skills.projectile.accelerationVector=Vector3{acceleration*std::cos(angle),0,-acceleration*std::sin(angle)};
            Fire(a,{Formation::RandomAngleSpeed,5,color,directions,1,2.67857143F,0.803571463F,pi,-pi,19});
        };
        rain(6,2.57142878F,forward,count);co_await a.At(61);
        rain(10,2.89285707F,forward+pi/2,1);co_await a.At(62);
        rain(8,2.89285707F,forward-pi/2,1);co_await a.At(63);
        rain(2,2.57142878F,reverse,count);co_await a.At(64);
        rain(13,2.89285707F,reverse+pi,1);co_await a.At(65);
        rain(14,2.57142878F,reverse-pi/2,1);
        forward+=0.104719758F;if(forward>pi)forward-=2*pi;
        reverse-=0.104719758F;if(reverse<-pi)reverse+=2*pi;
        if(a.phaseFrames>1200)count=3;else if(a.phaseFrames>600)count=2;
        a.soundEvents.push_back(22);co_await a.At(66);a.Rewind(60);
    }
}
Routine MeilingBoss::Behaviors::Defeated(Actor& a) {
    a.damageable=false;a.lifeThreshold=-1;a.timeoutFrames=-1;a.deathCallback=End;
    a.RandomDirection(-pi,pi);Motion(a).Apply({.speed=1.60714281F});Motion(a).Decelerate(60);
    for(int i=0;i<6;++i) {co_await a.At(i*10+10);a.soundEvents.push_back(7);}
    a.soundEvents.push_back(18);a.pendingCallback=End;
}
Routine MeilingBoss::Behaviors::End(Actor& a) {a.finished=true;co_return;}

}
