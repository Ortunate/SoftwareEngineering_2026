// 本文件实现芙兰朵露铭刻往昔之钟的三段扇形和四向旋转激光使魔；空间量为世界单位，时钟为60 Hz内容帧。
#include "gameplay/bosses/flandre.hpp"
#include "gameplay/spatial.hpp"
#include "stages/common/ids.hpp"
#include <numbers>

namespace koumakyou::bosses {
namespace {
using behavior::Actor;using behavior::Routine;
constexpr float pi=std::numbers::pi_v<float>;
enum class EndRow {Middle,Lower};
movement::State& Motion(Actor& actor) {
    if(actor.skills.spatial&&!actor.liftedMotion)actor.liftedMotion.emplace(actor.motion);
    return actor.liftedMotion?actor.liftedMotion->planar:actor.motion;
}
void Height(Actor& actor) {
    if(actor.liftedMotion)actor.liftedMotion->Aim(actor.motion,-1.25F+2.5F*spatial::RadicalInverse(actor.spatialIndex++,2));
}
Vector3 Direction(float angle,float tilt) {return {std::cos(angle)*std::cos(tilt),std::cos(angle)*std::sin(tilt),-std::sin(angle)};}
void Follow(Actor& actor,const std::array<float,4>& angles,float tilt) {
    for(int slot=0;slot<4;++slot)if(auto* beam=actor.Beam(slot)) {
        beam->beamAnchor=actor.Position();beam->beamDirection=Direction(angles[slot],tilt);
        beam->position=beam->beamAnchor;
    }
}
void Fade(Actor& actor) {
    for(int slot=0;slot<4;++slot)if(auto* beam=actor.Beam(slot)) {
        beam->maxAge=beam->age+beam->beamFadeSeconds;beam->dangerEndSeconds=beam->age+2.0F/60;
    }
}
template<EndRow Row>
Routine ClockOrb(Actor& actor) {
    actor.SetPoses({-1,-1,-1,-1,-1});actor.Animate(16);actor.damageable=actor.contactEnabled=actor.interactable=false;
    actor.hitRadius=1.25F;actor.skills.rankInfluence={};actor.skills.offset={};actor.SetEmitterSound(22);
    const bool left=actor.Position().x<0;
    const float tilt=actor.skills.spatial?(left?.42F:-.42F):0;
    const float rotation=left?0.0196349546F:-0.0196349546F;
    std::array<float,4> angles{-pi/2,0,pi/2,pi};
    co_await actor.At(30);
    for(int slot=0;slot<4;++slot)actor.FireBeam(slot,skill::Laser{.color=6,.length=9.64285755F,.width=1.07142854F,
        .grow=60,.sustain=800,.fade=20,.warning=60,.harmlessTail=18,.aimed=false,.direction=Direction(angles[slot],tilt),.animationScript=1});
    actor.soundEvents.push_back(16);
    // 每个固定步跟随一次。半帧推进角度，不把120 Hz退化成60 Hz跳动；平面对照保留逐次float32累加。
    const double step=actor.ContentStepFrames();
    for(double frame=30;frame<360;frame+=step) {
        co_await actor.At(frame);
        if(frame==80) {
            auto target=Motion(actor).position;target.x=-target.x;target.z=Row==EndRow::Lower?-2.85714293F:2.85714293F;
            Motion(actor).Interpolate(220,target,1);Height(actor);
        }
        if(frame==300) {Fade(actor);actor.InterruptAnimation(1);}
        for(auto& angle:angles)angle+=rotation*static_cast<float>(step);
        Follow(actor,angles,tilt);
    }
    co_await actor.At(390);actor.ClearBeamSlots();actor.finished=true;
}
void SpawnClock(Actor& actor,Vector3 position,EndRow target) {
    const behavior::Pattern pattern{.movement={.speed=0},.initialAnimation=16,.deathEffect=0,.spatialShots=actor.skills.spatial,
        .routine=target==EndRow::Lower?ClockOrb<EndRow::Lower>:ClockOrb<EndRow::Middle>};
    // 原出生Z只携带目标行，已在此展开成命名路线；生产position.y始终是真实高度，不能借作数据通道。
    actor.spawnEvents.push_back({.position=position,.pattern=pattern,.difficulty=actor.difficulty,.sourceHealth=10,
        .sourceScore=10,.seed=++actor.spawnIndex,.sourceItem=-2,.visual=stage_assets::kOrb,.hitRadius=1.25F,
        .bulletBase=actor.skills.bulletBase,.laserBase=actor.skills.laserBase,.defeatBase=stage_assets::kNativeDefeat,
        .fallback=actor.skills.fallback,.retireOutside=false,.damageable=false,.stageClearResponse=behavior::StageClearResponse::Remove});
}
Routine Pulses(Actor& actor,int frames,int count,int interval,float speed) {
    for(int frame=0;frame<frames;++frame) {
        co_await actor.At(frame);
        if(frame%16==0) {
            skill::Shot fixed{skill::Formation::FixedFan,3,2,32,1,13.9285707F,3.21428585F,-pi/2,0.130899698F,0};
            fixed.coneHalfAngle=2.02894521F;actor.Fire(fixed);
        }
        if(frame%interval==0) {
            skill::Shot aimed{skill::Formation::AimedFan,3,2,count,1,speed,3.21428585F,0,0.157079637F,0};
            aimed.coneHalfAngle=(count-1)*.5F*0.157079637F;actor.Fire(aimed);
        }
    }
    co_await actor.At(frames);
}
}
Routine FlandreBoss::Behaviors::ClockOfThePast(Actor& actor) {
    actor.ClearBullets();actor.defeatActors=true;actor.SetEmitterPeriod(0);actor.SetEmitterSound(-1);
    actor.SetPoses({160,166,166,161,162});actor.damageable=false;actor.contactEnabled=true;
    actor.skills.offset={};actor.skills.projectile.sampleAtStepStart=true;
    Motion(actor).Interpolate(120,{0,0,6.42857143F},2);Height(actor);
    co_await actor.At(120);actor.SetAmbientEffects(effects::AmbientMode::Living);actor.damageable=true;
    for(;;) {
        actor.soundEvents.push_back(5);SpawnClock(actor,{-5.71428585F,0,-2.85714293F},EndRow::Lower);SpawnClock(actor,{5.71428585F,0,2.85714293F},EndRow::Middle);
        co_await actor.At(210);co_await Pulses(actor,240,17,24,4.8214283F);
        co_await actor.At(230);actor.soundEvents.push_back(5);SpawnClock(actor,{-5.71428585F,0,2.85714293F},EndRow::Middle);SpawnClock(actor,{5.71428585F,0,-2.85714293F},EndRow::Lower);
        co_await actor.At(320);co_await Pulses(actor,250,7,16,10.4464273F);
        co_await actor.At(330);actor.soundEvents.push_back(5);SpawnClock(actor,{-5.71428585F,0,4.28571415F},EndRow::Lower);SpawnClock(actor,{5.71428585F,0,4.28571415F},EndRow::Lower);
        co_await actor.At(420);co_await Pulses(actor,250,17,16,4.8214283F);
        // 三个子时钟消费240/250/250帧，父从477回120，整轮1097帧；源临时加速值未被后续发射读取。
        co_await actor.At(477);actor.Rewind(120);
    }
}
}
