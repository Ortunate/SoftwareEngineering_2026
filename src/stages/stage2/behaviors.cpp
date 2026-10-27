// 本文件实现二面普通演员、大妖精三种攻击及琪露诺 Normal 五阶段；不执行 ECL 或源坐标换算。
#include "stages/stage2/behaviors.hpp"
#include "stages/stage2/ids.hpp"
#include "gameplay/data/native_combat.hpp"
#include "gameplay/spatial.hpp"
#include <cmath>
#include <numbers>
namespace koumakyou::stage2 {
namespace {
using behavior::Actor;
using behavior::Routine;
using skill::Formation;
constexpr float pi=std::numbers::pi_v<float>;
/// @brief 阶段 HP/阈值/超时只读取本关统一目录的唯一 Normal 常量，缺失立即失败。
int Setting(int sub,int opcode) { return native_combat::RequireLiteral(native_combat::Setting(2,sub,opcode)); }
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
void ConfigureBehavior(Actor& a,bool spatial) {
    a.skills.bulletBase=kNativeBulletBase;a.skills.laserBase=kNativeLaserBase;a.skills.fallback=kBlueBullet;
    a.skills.spatial=spatial;if(!spatial)a.liftedMotion.reset();
}
Routine DeceleratingShooter(Actor& a) {
    a.motion.Apply({.acceleration=-2.41071439F});
    // 源随机初相在配置周期时读取一次；计时器在同一内容帧末先递增，故首发减 1。
    // 周期与 rank 弹数/速度都是配置时快照，不能在自动发射时重新读取变化后的 rank。
    // 原生先对 rank 项作整数截断再加端点；合并为一次浮点截断会使部分 rank 的周期少一帧。
    const int interval=180+36-static_cast<int>(a.skills.rank*72/32.0F);
    const int initial=static_cast<int>(a.RandomIndex(interval));
    const int cutoff=std::abs(a.motion.angle-0.785398185F)<0.001F ? 180 : 200;
    const auto configuration=a.skills;
    const skill::Shot shot{Formation::AimedOffsetRing,4,11,4,1,5.35714293F,0,0,0,4,12};
    for(int frame=interval-initial-1;frame<cutoff;frame+=interval) {
        co_await a.At(frame);a.FireAt(shot,a.Position(),&configuration);
    }
    co_await a.At(10000);a.finished=true;
}
Routine OrbDeathVolley(Actor& a) {
    co_await a.At(1);Fire(a,{Formation::AimedOffsetRing,2,2,4,1,3.21428585F,0,0,0,4});a.finished=true;
}
Routine AimedTurningFairy(Actor& a) {
    a.motion.Polar(AimAngle(a),6.4285717F);
    co_await a.At(180);a.motion.Apply({.angularVelocity=-1.47262156F});
    co_await a.At(280);a.motion.Apply({.angularVelocity=0});
    co_await a.At(10000);a.finished=true;
}
Routine WanderingOrb(Actor& a) {
    a.RandomDirection(0.785398185F,2.3561945F,false);
    co_await a.At(10000);a.finished=true;
}
namespace daiyousei {
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
Routine Midboss(Actor& a) {
    a.damageable=false;a.deathEffect=2;a.Animate(64);a.timeoutFrames=Setting(20,115);
    a.timeoutCallback=Timeout;a.deathCallback=Defeated;
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
Routine Defeated(Actor& a) {
    a.damageable=false;a.itemEvents.push_back(3);a.finished=true;co_return;
}
Routine Timeout(Actor& a) {
    a.damageable=false;
    co_await a.At(10);MoveTo(a,60,{0,0,12.8571424F});
    co_await a.At(70);a.finished=true;
}
}
namespace cirno {
namespace {
Routine FirstVolley(Actor& a) {
    MoveTo(a,40,{0,0,5.71428585F});
    float spacing=0.0560998693F;
    for(int wave=0;wave<3;++wave) {
        co_await a.At(41+wave*70);a.skills.rankInfluence={-1.33928573F,2.67857146F,0,2,0,0};
        const float angle=AimAngle(a);a.soundEvents.push_back(8);
        for(int i=0;i<6;++i) {
            co_await a.At(41+wave*70+i*2);
            Fire(a,{Formation::FixedFan,5,6,i+1,1,13.3928576F-i*1.33928573F,2.67857146F,angle,spacing,4});
        }
        spacing+=0.0490873866F;
    }
    co_await a.At(251);
}
Routine FirstRings(Actor& a,int cycle) {
    Motion(a).Apply({.speed=8.03571415F});
    a.skills.rankInfluence={-1.33928573F,0.803571463F,0,4,0,0};
    for(int repeat=0;repeat<3;++repeat) {
        co_await a.At(21);a.RandomDirection(-pi,pi);Motion(a).Decelerate(60);AimHeight(a);
        a.skills.projectile={.delayFrames=60,.turnSpeed=10.7142859F};
        Fire(a,{Formation::AimedRing,1,6,16,1,5.35714293F,2.67857146F,0,0,4});a.soundEvents.push_back(8);
        a.ElapseLocalFrames(cycle*3);
        co_await a.At(41);Fire(a,{Formation::AimedRing,5,15,32,1,8.03571415F,2.67857146F,0,0,132});
        a.soundEvents.push_back(8);a.ElapseLocalFrames(cycle*3);
        co_await a.At(61);Fire(a,{Formation::AimedRing,1,6,14,1,5.35714293F,2.67857146F,0,0,4});
        a.soundEvents.push_back(8);a.ElapseLocalFrames(cycle*5);
        co_await a.At(101);if(repeat<2)a.Rewind(1);
    }
}
Routine IcicleStream(Actor& a) {
    for(int cycle=0;;++cycle) {
        float left=-0.196349546F,right=-2.94524312F;
        for(int remaining=11;remaining>0;--remaining) {
            const float jitter=a.Random(0.0490873866F);
            const int layers=cycle<3 ? 3 : cycle<6 ? 4 : 5;
            a.skills.projectile={.delayFrames=60,.turnAngle=1.57079637F,.turnSpeed=4.28571463F};
            Fire(a,{Formation::FixedFan,5,6,1,layers,17.4107151F,1.33928573F,left+jitter,0,68});
            a.skills.projectile.turnAngle=-1.57079637F;a.skills.projectile.turnSpeed=3.75F;
            Fire(a,{Formation::FixedFan,5,6,1,layers,17.4107151F,1.33928573F,right-jitter,0,68});
            left+=0.0981747732F;right-=0.0981747732F;
            const float playerX=a.PlayerPosition().x;
            const int divisor=playerX<0 ? 1 : playerX<2.85714286F ? 2 : 3;
            if(remaining%divisor==0)Fire(a,{Formation::AimedFan,3,13,5,1,5.35714293F,1.33928573F,0,0.261799395F,4});
            a.soundEvents.push_back(9);
            co_await a.At(27);a.Rewind(0);
        }
    }
}
Routine SecondVolley(Actor& a,int cycle) {
    Wander(a,8.03571415F,60);a.skills.offset={0,0,-0.357142866F};
    for(int repeat=0;repeat<8;++repeat) {
        Fire(a,{Formation::AimedRing,1,6,cycle+8,1,8.03571415F,2.67857146F,0,0,4});a.soundEvents.push_back(8);
        co_await a.At(10);Fire(a,{Formation::AimedOffsetRing,0,5,8,1,3.48214269F,2.67857146F,0,0,5});
        co_await a.At(20);if(repeat<7)a.Rewind(0);
    }
}
Routine MovingLasers(Actor& a) {
    Wander(a,8.03571415F,60);a.skills.offset={0,0,-0.357142866F};
    for(int repeat=0;repeat<3;++repeat) {
        a.soundEvents.push_back(16);
        for(int side=0;side<3;++side)a.Fire(skill::Laser{.color=side==0 ? 6 : 5,.length=8.5714283F,
            .width=0.267857134F,.grow=0,.sustain=9999,.fade=30,.warning=0,.harmlessTail=30,
            .angle=side==0 ? 0.0F : side==1 ? 0.392699093F : -0.392699093F,.speed=10.7142859F});
        co_await a.At(50);if(repeat<2)a.Rewind(0);
    }
}
/// @brief 冻结和再加速只修改敌方普通弹；每弹一次方向读取，动画年龄与显现计时不重置。
void Freeze(Actor& a,bool release) {
    a.ModifyEnemyProjectiles([&](Bullet& bullet) {
        bullet.nativeColor=15;bullet.visual=NativeBulletVisual(bullet.nativeType,15);bullet.color=WHITE;
        if(!release)bullet.velocity=bullet.baseVelocity={};
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
    for(int cycle=0;;++cycle) {
        Wander(a,5.35714293F,120);
        for(int repeat=0;repeat<6;++repeat) {
            for(int color=0;color<5;++color) {
                co_await a.At((color+1)*5);
                Fire(a,{Formation::RandomAngleSpeed,1,std::array{6,2,10,13,14}[color],cycle+7,1,
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
Routine FirstNonspell(Actor& a) {
    InitializeCirno(a);a.damageable=false;
    a.health=a.maxHealth=Setting(22,111);a.lifeThreshold=Setting(22,113);a.lifeCallback=IcicleFall;
    a.timeoutFrames=Setting(22,115);a.timeoutCallback=IcicleFall;a.deathCallback=SecondNonspell;
    co_await a.At(20);a.damageable=true;
    co_await a.At(70);
    for(int cycle=0;;++cycle) { co_await FirstVolley(a);co_await FirstRings(a,cycle); }
}
Routine IcicleFall(Actor& a) {
    a.ClearBullets();a.damageable=false;a.timeoutFrames=Setting(31,115);a.timeoutCallback=SecondNonspell;
    MoveTo(a,120,{0,0,5.71428585F});
    co_await a.At(120);a.Animate(131);a.damageable=true;
    co_await IcicleStream(a);
}
Routine SecondNonspell(Actor& a) {
    a.ClearBullets();a.damageable=false;a.health=a.maxHealth=Setting(25,111);
    a.lifeThreshold=Setting(25,113);a.lifeCallback=PerfectFreeze;a.timeoutFrames=Setting(25,115);
    a.timeoutCallback=PerfectFreeze;a.deathCallback=Defeated;a.dropEvents.push_back(5);
    co_await a.At(200);a.damageable=true;
    for(int cycle=0;;++cycle) {
        co_await SecondVolley(a,cycle);co_await MovingLasers(a);
        if(cycle>=11)a.phaseFrames=99999;
    }
}
Routine PerfectFreeze(Actor& a) {
    a.ClearBullets();a.damageable=false;a.lifeThreshold=Setting(36,113);a.lifeCallback=DiamondBlizzard;
    a.timeoutCallback=DiamondBlizzard;a.timeoutFrames=Setting(37,115);a.skills.offset={0,0,-0.357142866F};
    a.skills.rankInfluence={-0.803571463F,2.67857146F,-1,5,0,0};MoveTo(a,120,{0,0,5.71428585F});
    co_await a.At(120);a.poses={128,131,131,129,130};a.Animate(131);a.damageable=true;
    co_await FreezeCycle(a);
}
Routine DiamondBlizzard(Actor& a) {
    a.poses={128,128,128,129,130};a.ClearBullets();a.damageable=false;a.phaseFrames=0;
    a.soundEvents.push_back(7);a.dropEvents.push_back(5);
    a.skills.rankInfluence={-1.33928573F,0.803571463F,0,4,0,0};
    co_await a.At(60);a.timeoutFrames=Setting(40,115);a.timeoutCallback=Defeated;
    const skill::Shot shot{Formation::RandomAngleSpeed,5,6,10,1,5.35714293F,1.07142866F,pi,-pi,5,30};
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
Routine Defeated(Actor& a) {
    a.damageable=false;a.interactable=false;a.timeoutFrames=-1;a.lifeThreshold=-1;
    a.poses={128,128,128,129,130};Wander(a,1.60714293F,60);
    for(int repeat=0;repeat<6;++repeat) { co_await a.At(10+repeat*10);a.soundEvents.push_back(7); }
    a.soundEvents.push_back(18);a.ClearBullets();a.pendingCallback=End;
}
Routine End(Actor& a) { a.finished=true;co_return; }
}
}
