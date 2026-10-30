// 本文件实现 CirnoBoss 内部命名行为；移动、弹幕、演出和随机读取保持原内容时序。
#include "gameplay/bosses/cirno.hpp"
#include "stages/stage2/behaviors.hpp"
#include "stages/stage2/ids.hpp"
#include "gameplay/spatial.hpp"
#include <array>
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
void Wander(Actor& a,float speed,int frames) {
    if(a.skills.spatial&&!a.liftedMotion)a.liftedMotion.emplace(a.motion);
    a.RandomDirection(-pi,pi);Motion(a).Apply({.speed=speed});Motion(a).Decelerate(frames);AimHeight(a);
}
float AimAngle(const Actor& a) {
    const auto p=a.PlayerPosition(),origin=a.Position();return std::atan2(origin.z-p.z,p.x-origin.x);
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
void InitializeCirno(Actor& a) {
    a.poses={128,128,128,129,130};a.deathEffect=2;
    a.skills.offset={0,0,0.535714269F};
    Motion(a).bounds={{-7.14285714F,4.01785714F},{7.14285714F,7.85714286F}};
    Motion(a).clamped=true;
}

}

namespace {
Routine FirstVolley(Actor& a) {
    MoveTo(a,40,{0,0,5.71428585F});
    const auto difficulty=static_cast<std::size_t>(native_combat::DifficultyIndex(a.difficulty));
    float spacing=std::array{0.041336745F,0.0560998693F,0.0654498488F,0.0654498488F}.at(difficulty);
    for(int wave=0;wave<3;++wave) {
        co_await a.At(41+wave*70);a.skills.rankInfluence={-1.33928573F,2.67857146F,0,2,0,0};
        const float angle=AimAngle(a);a.soundEvents.push_back(8);
        for(int i=0;i<6;++i) {
            co_await a.At(41+wave*70+i*2);
            Fire(a,{Formation::FixedFan,5,6,i+1,1,13.3928576F-i*1.33928573F,2.67857146F,angle,spacing,4});
            if(i==5&&difficulty>=2)Fire(a,{Formation::FixedRing,3,6,difficulty==2 ? 16 : 32,
                difficulty==2 ? 1 : 2,8.03571415F,2.67857146F,angle,spacing,4});
        }
        spacing+=std::array{0.0392699093F,0.0490873866F,0.0490873866F,0.0654498488F}.at(difficulty);
    }
    co_await a.At(251);
}
Routine FirstRings(Actor& a,int cycle) {
    const auto difficulty=static_cast<std::size_t>(native_combat::DifficultyIndex(a.difficulty));
    Motion(a).Apply({.speed=8.03571415F});
    a.skills.rankInfluence={-1.33928573F,0.803571463F,0,4,0,0};
    for(int repeat=0;repeat<3;++repeat) {
        co_await a.At(21);a.RandomDirection(-pi,pi);Motion(a).Decelerate(60);AimHeight(a);
        a.skills.projectile={.delayFrames=60,.turnSpeed=10.7142859F};
        Fire(a,{Formation::AimedRing,1,6,std::array{8,16,24,24}.at(difficulty),difficulty==3 ? 2 : 1,5.35714293F,2.67857146F,0,0,4});a.soundEvents.push_back(8);
        a.ElapseLocalFrames(cycle*3);
        co_await a.At(41);Fire(a,{Formation::AimedRing,5,15,std::array{16,32,32,32}.at(difficulty),1,
            std::array{5.35714293F,8.03571415F,10.7142859F,13.3928576F}.at(difficulty),2.67857146F,0,0,132});
        a.soundEvents.push_back(8);a.ElapseLocalFrames(cycle*3);
        co_await a.At(61);Fire(a,{Formation::AimedRing,1,6,std::array{6,14,16,16}.at(difficulty),difficulty==3 ? 2 : 1,
            difficulty==0 ? 10.7142859F : 5.35714293F,2.67857146F,0,0,4});
        a.soundEvents.push_back(8);a.ElapseLocalFrames(cycle*5);
        co_await a.At(101);if(repeat<2)a.Rewind(1);
    }
}
Routine IcicleStream(Actor& a) {
    const auto difficulty=static_cast<std::size_t>(native_combat::DifficultyIndex(a.difficulty));
    for(int cycle=0;;++cycle) {
        float left=-0.196349546F,right=-2.94524312F;
        for(int remaining=11;remaining>0;--remaining) {
            const float jitter=a.Random(0.0490873866F);
            const int layers=(cycle<3 ? 3 : cycle<6 ? 4 : 5)+(difficulty>=2 ? 1 : 0);
            a.skills.projectile={.delayFrames=60,.turnAngle=1.57079637F,.turnSpeed=4.28571463F};
            Fire(a,{Formation::FixedFan,5,6,1,layers,17.4107151F,1.33928573F,left+jitter,0,68});
            a.skills.projectile.turnAngle=-1.57079637F;a.skills.projectile.turnSpeed=3.75F;
            Fire(a,{Formation::FixedFan,5,6,1,layers,17.4107151F,1.33928573F,right-jitter,0,68});
            left+=0.0981747732F;right-=0.0981747732F;
            const float playerX=a.PlayerPosition().x;
            const int divisor=playerX<0 ? 1 : playerX<2.85714286F ? 2 : 3;
            if(difficulty>0&&remaining%divisor==0)Fire(a,{Formation::AimedFan,3,13,5,1,5.35714293F,1.33928573F,0,0.261799395F,4});
            a.soundEvents.push_back(9);
            co_await a.At(27);a.Rewind(0);
        }
    }
}
/// @brief Hard/Lunatic 的 Icicle Fall 使用独立旋转环；随机角后仍消费源脚本未使用的抖动读取。
Routine HardIcicle(Actor& a) {
    const bool lunatic=a.difficulty==native_combat::Difficulty::Lunatic;
    for(int cycle=0;;++cycle) {
        const float turn=cycle%2==0 ? 0.392699093F : -0.392699093F;
        for(int wave=0;wave<8;++wave) {
            co_await a.At(wave*20);
            const float angle=a.Random(2*pi)-pi;
            (void)a.Random(0.0981747732F);
            a.skills.projectile={.delayFrames=60,.turnAngle=turn,
                .turnSpeed=(1.4F+0.15F*cycle)*2.67857143F};
            Fire(a,{Formation::FixedRing,5,6,cycle+(lunatic ? 13 : 8),3,
                13.3928576F,4.01785707F,angle,0,260});
            a.soundEvents.push_back(9);
        }
        co_await a.At(160);Wander(a,2.67857143F,120);
        if(cycle>=10)a.phaseFrames=99999;
        co_await a.At(280);a.Rewind(0);
    }
}
Routine SecondVolley(Actor& a,int cycle) {
    const auto difficulty=static_cast<std::size_t>(native_combat::DifficultyIndex(a.difficulty));
    Wander(a,8.03571415F,60);a.skills.offset={0,0,-0.357142866F};
    for(int repeat=0;repeat<8;++repeat) {
        Fire(a,{Formation::AimedRing,1,6,cycle+std::array{5,8,14,20}.at(difficulty),difficulty==3 ? 2 : 1,
            std::array{5.35714293F,8.03571415F,10.7142859F,10.7142859F}.at(difficulty),2.67857146F,0,0,4});a.soundEvents.push_back(8);
        co_await a.At(10);Fire(a,{Formation::AimedOffsetRing,0,5,8,1,3.48214269F,2.67857146F,0,0,5});
        co_await a.At(20);if(repeat<7)a.Rewind(0);
    }
}
Routine MovingLasers(Actor& a) {
    const auto difficulty=static_cast<std::size_t>(native_combat::DifficultyIndex(a.difficulty));
    Wander(a,8.03571415F,60);a.skills.offset={0,0,-0.357142866F};
    for(int repeat=0;repeat<3;++repeat) {
        a.soundEvents.push_back(16);
        for(int side=0;side<3;++side)a.Fire(skill::Laser{.color=side==0 ? 6 : 5,.length=8.5714283F,
            .width=0.267857134F,.grow=0,.sustain=9999,.fade=30,.warning=0,.harmlessTail=30,
            .angle=side==0 ? 0.0F : side==1 ? 0.392699093F : -0.392699093F,.speed=10.7142859F});
        if(difficulty>=2)Fire(a,{Formation::AimedRing,1,6,difficulty==2 ? 16 : 32,2,5.35714293F,2.67857146F,0,0,4});
        co_await a.At(50);if(repeat<2)a.Rewind(0);
    }
}
/// @brief 冻结和再加速只修改敌方普通弹；每弹一次方向读取，动画年龄与显现计时不重置。
void Freeze(Actor& a,bool release) {
    a.ModifyEnemyProjectiles([&](Bullet& bullet) {
        bullet.nativeColor=15;bullet.visual=NativeBulletVisual(bullet.nativeType,15);bullet.color=WHITE;
        if(!release)bullet.SetVelocity(bullet.baseVelocity={});
        else {
            const float angle=a.Random(2*pi)-pi;
            bullet.acceleration={1.60714281F*std::cos(angle),0,-1.60714281F*std::sin(angle)};
            if(a.skills.spatial) {
                const auto index=a.spatialIndex++;
                const auto direction=spatial::EqualAreaDirection(spatial::Frame::FromAxis({0,0,-1}),
                    static_cast<int>(index%97),97,-1,angle);
                bullet.acceleration={direction.x*1.60714281F,direction.y*1.60714281F,direction.z*1.60714281F};
            }
            bullet.accelerationSeconds=220.0F/60;
        }
    });
}
Routine FreezeCycle(Actor& a) {
    const auto difficulty=static_cast<std::size_t>(native_combat::DifficultyIndex(a.difficulty));
    for(int cycle=0;;++cycle) {
        Wander(a,5.35714293F,120);
        for(int repeat=0;repeat<6;++repeat) {
            for(int color=0;color<5;++color) {
                co_await a.At((color+1)*5);
                Fire(a,{Formation::RandomAngleSpeed,1,std::array{6,2,10,13,14}[color],cycle+std::array{4,7,10,16}.at(difficulty),1,
                    10.7142859F,2.67857146F,pi,-pi,4});a.soundEvents.push_back(9);
            }
            if(repeat<5)a.Rewind(0);
        }
        co_await a.At(85);Freeze(a,false);
        co_await a.At(145);Wander(a,5.35714293F,120);
        for(int repeat=0;repeat<6;++repeat) {
            co_await a.At(cycle<3 ? 155 : cycle<6 ? 165 : 175);
            Fire(a,{Formation::AimedFan,3,6,cycle<3 ? 3 : 5,3,cycle<6 ? 10.7142859F : 13.3928576F,
                5.35714293F,0,cycle<6 ? 0.392699093F : 0.196349546F,4});
            a.Rewind(175);a.soundEvents.push_back(9);if(repeat<5)a.Rewind(145);
        }
        co_await a.At(295);Freeze(a,true);
        co_await a.At(475);a.Rewind(0);
    }
}
}
Routine CirnoBoss::Behaviors::FirstNonspell(Actor& a) {
    InitializeCirno(a);a.damageable=false;
    co_await a.At(20);a.damageable=true;
    constexpr std::array<Vector3,4> axes{Vector3{-.7F,.3F,.7F},Vector3{-1,.3F,.7F},Vector3{-.7F,.3F,.3F},Vector3{-0.0F,.3F,1}};
    for(int n=0;n<4;++n) {co_await a.At(20+n*10);a.AttachEffect(axes[n],2.14285714F,{255,224,224,255});}
    co_await a.At(70);
    for(int cycle=0;;++cycle) { co_await FirstVolley(a);co_await FirstRings(a,cycle); }
}
Routine CirnoBoss::Behaviors::IcicleFall(Actor& a) {
    a.ClearBullets();a.damageable=false;
    MoveTo(a,120,{0,0,5.71428585F});
    co_await a.At(120);a.Animate(131);a.damageable=true;
    if(a.difficulty==native_combat::Difficulty::Hard||a.difficulty==native_combat::Difficulty::Lunatic)
        co_await HardIcicle(a);
    else co_await IcicleStream(a);
}
Routine CirnoBoss::Behaviors::SecondNonspell(Actor& a) {
    a.ClearBullets();a.damageable=false;
    a.dropEvents.push_back(5);
    co_await a.At(200);a.damageable=true;
    for(int cycle=0;;++cycle) {
        co_await SecondVolley(a,cycle);co_await MovingLasers(a);
        if(cycle>=11)a.phaseFrames=99999;
    }
}
Routine CirnoBoss::Behaviors::PerfectFreeze(Actor& a) {
    a.ClearBullets();a.damageable=false;
    a.skills.offset={0,0,-0.357142866F};
    a.skills.rankInfluence={-0.803571463F,2.67857146F,-1,5,0,0};MoveTo(a,120,{0,0,5.71428585F});
    co_await a.At(120);a.poses={128,131,131,129,130};a.Animate(131);a.damageable=true;
    co_await FreezeCycle(a);
}
Routine CirnoBoss::Behaviors::DiamondBlizzard(Actor& a) {
    a.poses={128,128,128,129,130};a.ClearBullets();a.damageable=false;a.phaseFrames=0;
    a.EmitEffect(effects::Kind::Flash,1,{128,128,255,255});
    a.soundEvents.push_back(7);a.dropEvents.push_back(5);
    a.skills.rankInfluence={-1.33928573F,0.803571463F,0,4,0,0};
    co_await a.At(60);
    const auto difficulty=static_cast<std::size_t>(native_combat::DifficultyIndex(a.difficulty));
    const skill::Shot shot{Formation::RandomAngleSpeed,5,6,std::array{6,10,14,24}.at(difficulty),1,5.35714293F,1.07142866F,pi,-pi,5,30};
    const auto configuration=a.skills;
    MoveTo(a,120,{0,0,5.71428585F});
    co_await a.At(180);a.Animate(131);a.damageable=true;
    for(;;) {
        Wander(a,3.21428585F,120);
        for(int repeat=0;repeat<12;++repeat) {
            co_await a.At(190+repeat*10);
            auto origin=a.Position();origin.x+=a.Random(5.71428585F)-2.85714293F;
            origin.z-=a.Random(4.28571415F)-2.14285707F;
            a.FireAt(shot,origin,&configuration);a.soundEvents.push_back(9);
        }
        a.Rewind(180);
    }
}
Routine CirnoBoss::Behaviors::Defeated(Actor& a) {
    a.damageable=false;a.interactable=false;a.timeoutFrames=-1;a.lifeThreshold=-1;
    a.poses={128,128,128,129,130};Wander(a,1.60714293F,60);
    constexpr std::array<Color,5> colors{WHITE,Color{255,128,128,255},Color{128,255,128,255},Color{128,128,255,255},Color{255,255,128,255}};
    for(int repeat=0;repeat<6;++repeat) {
        for(int n=0;n<5;++n) {co_await a.At(repeat*10+(n+1)*2);a.EmitEffect(effects::Kind::Burst,2,colors[n]);}
        a.soundEvents.push_back(7);
    }
    a.soundEvents.push_back(18);a.ClearBullets();a.pendingCallback=End;
}
Routine CirnoBoss::Behaviors::End(Actor& a) { a.finished=true;co_return; }

}
