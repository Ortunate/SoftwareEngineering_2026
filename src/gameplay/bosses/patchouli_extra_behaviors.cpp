// 本文件实现帕秋莉Extra道中的三符、五种使魔与入退场；空间量为世界单位，等待仍按60 Hz内容帧。
#include "gameplay/bosses/patchouli.hpp"
#include "stages/common/ids.hpp"
#include "gameplay/spatial.hpp"
#include <cmath>
#include <numbers>

namespace koumakyou::bosses {
namespace {
using behavior::Actor;using behavior::Routine;using skill::Formation;
constexpr float pi=std::numbers::pi_v<float>;
movement::State& Motion(Actor& actor) {
    if(actor.skills.spatial&&!actor.liftedMotion)actor.liftedMotion.emplace(actor.motion);
    return actor.liftedMotion ? actor.liftedMotion->planar : actor.motion;
}
void AimHeight(Actor& actor) {
    if(actor.liftedMotion)actor.liftedMotion->Aim(actor.motion,-1.25F+2.5F*spatial::RadicalInverse(actor.spatialIndex++,2));
}
void Fire(Actor& actor,skill::Shot shot) {
    if(actor.skills.spatial) {
        // 随机弹维持源方向数，避免三维密度扩充额外消费随机流并改写后续巡航；高度由序号补齐。
        shot.spatialCount=shot.formation==Formation::RandomAngleSpeed ? shot.count : shot.count*3;
        if(shot.formation==Formation::RandomAngleSpeed) {
            shot.sampleIndex=static_cast<int>(actor.spatialIndex&0x7fffffffU);actor.spatialIndex+=shot.spatialCount*shot.layers;
        }
    }
    actor.Fire(shot);
}
Routine Prepare(Actor& actor,bool moon) {
    actor.AnimateAuxiliary(0,17);
    actor.ClearBullets();actor.SetEmitterPeriod(0);actor.SetEmitterSound(-1);
    actor.defeatActors=true;
    actor.soundEvents.push_back(5);actor.deathEffect=2;actor.interactable=true;actor.contactEnabled=true;actor.damageable=false;
    actor.hitRadius=0.892857134F;actor.skills.offset={};actor.skills.projectile.sampleAtStepStart=true;
    actor.SetPoses(moon ? std::array<int,5>{71,69,70,67,68} : std::array<int,5>{65,69,70,67,68});
    Motion(actor).Interpolate(120,{0,0,6.42857143F},2);AimHeight(actor);
    co_await actor.At(120);actor.damageable=true;
    if(!moon) {actor.SetPoses({71,69,70,67,68});actor.SetEmitterSound(7);}
}
void InitializeStone(Actor& actor) {
    actor.Animate(13);actor.AnimateAuxiliary(0,16);actor.interactable=false;actor.SetEmitterSound(7);
    actor.hitRadius=0.625F;actor.skills.projectile.sampleAtStepStart=true;
}
float Aim(const Actor& actor) {
    const auto p=actor.PlayerPosition(),origin=actor.Position();return std::atan2(origin.z-p.z,p.x-origin.x);
}
void SpawnStone(Actor& actor,Vector3 position,behavior::Entry entry,std::uint32_t seed) {
    const behavior::Pattern pattern{.movement={.speed=0},.initialAnimation=13,.deathEffect=0,
        .spatialShots=actor.skills.spatial,.routine=entry,.onDefeated=PatchouliBoss::Behaviors::StoneDefeated};
    // 95的负短生命载荷保留原生模板1 HP；使魔关闭交互，不能改成60000 HP或借用父本体生命。
    actor.spawnEvents.push_back({.position=position,.pattern=pattern,.difficulty=actor.difficulty,.sourceHealth=1,
        .sourceScore=10,.seed=seed,.sourceItem=-2,.visual=stage_assets::kOrb,.hitRadius=0.625F,
        .bulletBase=actor.skills.bulletBase,.laserBase=actor.skills.laserBase,.defeatBase=stage_assets::kNativeDefeat,
        .fallback=actor.skills.fallback,.retireOutside=false});
}
}

void PatchouliBoss::Behaviors::ConfigureExtraPractice(Actor& actor,bool spatial) {
    actor.skills.spatial=spatial;actor.skills.bulletBase=stage_assets::kNativeBulletBase;
    actor.skills.laserBase=stage_assets::kNativeLaserBase;actor.skills.fallback=stage_assets::kBlueBullet;
    if(!spatial)actor.liftedMotion.reset();
    Motion(actor).bounds={{-7.14285714F,4.64285707F},{7.14285714F,7.85714293F}};Motion(actor).clamped=true;
    actor.SetPoses({65,69,70,67,68});
}

Routine PatchouliBoss::Behaviors::SilentSelene(Actor& actor) {
    co_await Prepare(actor,true);
    for(int wave=0;;++wave) {
        co_await actor.At(wave*6);
        for(int n=0;n<2;++n) {
            // 源落点为绝对随机位置，仍以相对演员偏移交付；不能随巡航平移整个弹雨区域。
            const float x=actor.Random(17.1428566F)-8.5714283F;
            const float z=10-actor.Random(8.03571415F);
            const auto position=actor.Position();actor.skills.offset={x-position.x,0,z-position.z};
            actor.skills.projectile.acceleration=actor.Random(3.21428561F)+0.803571403F;
            actor.skills.projectile.accelerationVector.reset();actor.skills.projectile.accelerationFrames=96;
            Fire(actor,{Formation::RandomAngleSpeed,2,6,1,1,0,0,1.59534001F,1.54625261F,24});
        }
        actor.soundEvents.push_back(24);
        if(wave%32==7) {
            actor.RandomDirection(-pi,pi);Motion(actor).Apply({.speed=4.01785707F});Motion(actor).Decelerate(90);AimHeight(actor);
        }
        actor.skills.offset={};
        Fire(actor,{Formation::FixedFan,2,8,wave/5+12,1,7.23214293F,5.35714286F,-pi/2,0.261799395F,0});
    }
}

Routine PatchouliBoss::Behaviors::RoyalFlare(Actor& actor) {
    // 后继技能入口先交付前段结束时的原生掉落，不能等日符自身结束才结算。
    actor.dropEvents.push_back(10);
    co_await Prepare(actor,false);
    float speed=0.535714269F,angle=0,offsetAngle=0;
    int frame=0,calls=0;
    for(;;) {
        for(int phase=0;phase<4;++phase) {
            if(phase!=0)speed+=0.267857134F;
            const int duration=phase<2?280:phase==2?180:200,directions=phase==0?3:phase==2?6:5;
            float radius=0;
            actor.skills.projectile.acceleration=phase==3?3.21428561F:2.41071415F;
            actor.skills.projectile.accelerationVector.reset();actor.skills.projectile.accelerationFrames=100;
            for(int n=0;n<duration;++n) {
                co_await actor.At(frame+n);
                if(calls%6==0) {
                    // 原生13从当前角开始逐次加2π/N；六次调用计数不因换段、空窗或外层回跳重置。
                    float direction=angle;const float step=2*pi/directions;
                    for(int i=0;i<directions;++i) {
                        const auto position=actor.Position();
                        actor.skills.offset={std::cos(direction)*radius-position.x,-position.y,-std::sin(direction)*radius-position.z};
                        Fire(actor,{Formation::FixedFan,1,2,phase==2||phase==3?1:2,1,phase==2?0:speed,0,
                            direction+offsetAngle,phase==2?1.04719758F:0.52359879F,536});
                        direction+=step;
                    }
                }
                ++calls;radius+=0.0446428582F;
                angle+=phase==0||phase==3 ? 0.0245436933F : -0.0245436933F;
                offsetAngle+=phase==0 ? -0.0245436933F : phase==2 ? 0.00981747732F : phase==3 ? -0.0654498488F : 0.0245436933F;
            }
            frame+=duration;co_await actor.At(frame);
        }
        frame+=60;co_await actor.At(frame);
    }
}
Routine PatchouliBoss::Behaviors::ExtraRetreat(Actor& actor) {
    Motion(actor).Apply({.speed=0});actor.Animate(65);actor.SetEmitterPeriod(0);actor.ClearBullets();
    Motion(actor).clamped=false;actor.defeatActors=true;actor.damageable=false;actor.contactEnabled=false;actor.finished=true;co_return;
}
Routine PatchouliBoss::Behaviors::ExtraEntrance(Actor& actor) {
    actor.damageable=false;actor.contactEnabled=false;actor.hitRadius=1.25F;
    actor.SetPoses({65,69,70,67,68});actor.skills.offset={0,0,0.535714269F};
    Motion(actor).clamped=false;Motion(actor).Interpolate(60,{0,0,4.64285707F},2);AimHeight(actor);
    co_await actor.At(60);Motion(actor).clamped=true;
    for(;;) {co_await actor.At(1060);actor.Rewind(60);}
}
Routine PatchouliBoss::Behaviors::ExtraDefeated(Actor& actor) {
    Motion(actor).Apply({.speed=0});actor.soundEvents.push_back(18);actor.dropEvents.push_back(20);actor.itemEvents.push_back(5);
    co_await ExtraRetreat(actor);
}
Routine PatchouliBoss::Behaviors::PhilosophersStone(Actor& actor) {
    actor.dropEvents.push_back(10);co_await Prepare(actor,false);
    // 出生点为独立绝对世界坐标，五种行为各自拥有固定种子，不继承父技能尚未消费的随机流。
    SpawnStone(actor,{0,0,3.57142857F},FireStone,29001);
    SpawnStone(actor,{-5.71428585F,0,5},WaterStone,29002);
    SpawnStone(actor,{2.85714293F,0,5.71428585F},WoodStone,29003);
    SpawnStone(actor,{5.71428585F,0,5},MetalStone,29004);
    SpawnStone(actor,{-2.85714293F,0,5.71428585F},EarthStone,29005);
    for(;;) {co_await actor.At(10000);actor.Rewind(0);}
}
Routine PatchouliBoss::Behaviors::FireStone(Actor& actor) {
    InitializeStone(actor);
    for(int cycle=0;;++cycle) {
        co_await actor.At(32+cycle*94);float angle=actor.Random(2*pi)-pi;
        Fire(actor,{Formation::FixedRing,5,2,10,2,5.35714286F,2.67857143F,angle,0,512});
        // 第二个随机角在首轮发射后立即快照；不能挪到4帧后的实际发射点。
        angle=actor.Random(2*pi)-pi;co_await actor.At(36+cycle*94);
        Fire(actor,{Formation::FixedRing,5,2,10,2,5.35714286F,2.67857143F,angle,0,512});
    }
}
Routine PatchouliBoss::Behaviors::WaterStone(Actor& actor) {
    InitializeStone(actor);
    for(int n=0;;++n) {
        co_await actor.At(64+n*50);(void)actor.Random(2*pi);
        Fire(actor,{Formation::AimedFan,5,6,11,2,8.03571415F,6.96428537F,0,0.261799395F,520});
    }
}
Routine PatchouliBoss::Behaviors::WoodStone(Actor& actor) {
    InitializeStone(actor);
    actor.skills.projectile.accelerationVector={std::cos(2.3561945F)*1.92857146F,0,-std::sin(2.3561945F)*1.92857146F};
    actor.skills.projectile.accelerationFrames=120;
    for(int n=0;;++n) {
        co_await actor.At(16+n*20);
        Fire(actor,{Formation::RandomAngleSpeed,5,10,10,1,5.35714286F,0.803571463F,pi,-pi,531});
    }
}
Routine PatchouliBoss::Behaviors::MetalStone(Actor& actor) {
    InitializeStone(actor);actor.skills.projectile.delayFrames=60;actor.skills.projectile.turnCount=1;actor.skills.projectile.turnSpeed=4.01785707F;
    for(int n=0;;++n) {
        co_await actor.At(48+n*70);actor.skills.projectile.turnAngle=Aim(actor);
        Fire(actor,{Formation::FixedRing,5,0,16,1,10.7142859F,2.67857143F,0,0,769});
    }
}
Routine PatchouliBoss::Behaviors::EarthStone(Actor& actor) {
    InitializeStone(actor);actor.skills.projectile.delayFrames=180;actor.skills.projectile.turnCount=1;actor.skills.projectile.turnSpeed=3.75F;
    for(int n=0;;++n) {
        co_await actor.At(16+n*40);
        for(int i=0;i<6;++i) {
            actor.skills.projectile.turnAngle=actor.Random(pi)-pi/2;const float aim=Aim(actor);
            Fire(actor,{Formation::RandomAngleSpeed,5,13,1,1,6.4285717F,2.67857143F,aim+pi/2,aim-pi/2,579});
        }
    }
}
Routine PatchouliBoss::Behaviors::StoneDefeated(Actor& actor) {
    actor.dropEvents.push_back(8);actor.InterruptAnimation(1);actor.InterruptAuxiliary(0,1);co_await actor.At(30);actor.finished=true;
}
}
