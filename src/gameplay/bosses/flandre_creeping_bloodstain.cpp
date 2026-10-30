// 本文件实现芙兰朵露额外第二符、210帧真实过渡及两类留下血痕弹的隐藏使魔；空间量为世界单位。
#include "gameplay/bosses/flandre.hpp"
#include "stages/common/ids.hpp"
#include "gameplay/spatial.hpp"
#include <numbers>
namespace koumakyou::bosses {
namespace {
using behavior::Actor;using behavior::Routine;
constexpr float pi=std::numbers::pi_v<float>;
constexpr movement::Bounds kEmitterBounds{{-10,-11.7857143F},{10,11.7857143F}};
constexpr movement::Bounds kBattleBounds{{-7.14285714F,1.42857146F},{7.14285714F,7.85714293F}};
void Normalize(float& angle) {while(angle>pi)angle-=2*pi;while(angle<-pi)angle+=2*pi;}
float Aim(const Actor& actor) {
    const auto p=actor.PlayerPosition(),a=actor.Position();
    return p.x==a.x&&p.z==a.z?pi/2:std::atan2(a.z-p.z,p.x-a.x);
}
movement::State& Motion(Actor& actor) {
    if(actor.skills.spatial&&!actor.liftedMotion)actor.liftedMotion.emplace(actor.motion);
    return actor.liftedMotion?actor.liftedMotion->planar:actor.motion;
}
// 两条路线分别保留源的固定角步进和每次随机角步进；布尔模板只选择命名行为差异，不读取HP或脚本编号。
template<bool Falling>
Routine BloodTrail(Actor& actor) {
    actor.SetPoses({-1,-1,-1,-1,-1});actor.Animate(167);
    actor.visible=actor.damageable=actor.contactEnabled=actor.interactable=false;actor.soundEvents.push_back(22);
    const float direction=Falling?Aim(actor):actor.motion.angle;
    actor.motion.Polar(direction,Falling?10.7142859F:8.03571415F);
    actor.skills.offset={};actor.skills.projectile={.sampleAtStepStart=true};
    actor.Fire({skill::Formation::FixedFan,9,0,1,1,10.7142859F,0,direction,0,514});
    float angle=actor.Random(2*pi)-pi;int round=0;
    actor.skills.projectile={.acceleration=Falling?0.482142866F:0.321428597F,.accelerationFrames=2000,.sampleAtStepStart=true};
    for(;;) {
        if(!kEmitterBounds.Contains(actor.Position())) {actor.finished=true;co_return;}
        if(round<1000&&actor.Frame()>=round*3) {
            const float radius=actor.Random(Falling?2.14285707F:1.78571427F),offsetAngle=actor.Random(2*pi)-pi;
            actor.skills.offset={std::cos(offsetAngle)*radius,0,-std::sin(offsetAngle)*radius};
            // 原6(1)结果恒为0但仍推进随机流；不能把颜色1视为常量后省掉这次读取。
            const int color=static_cast<int>(actor.RandomIndex(1))+1;
            skill::Shot shot{skill::Formation::FixedFan,3,color,1,1,0,0,angle,0,530};
            if(actor.skills.spatial) {shot.sampleIndex=round;shot.coneHalfAngle=0.5F;}
            actor.Fire(shot);
            angle+=Falling?pi/20:actor.Random(2*pi)-pi;Normalize(angle);++round;
        }
        // 与源一致，1000次后只停止ECL发射；一般路线会更早按167逻辑精灵尺寸在步首自然回收。
        co_await actor.At(actor.Frame()+0.5);
    }
}
void SpawnTrail(Actor& actor,Vector3 position,float angle,bool falling) {
    const behavior::Pattern pattern{.movement={.angle=angle,.speed=0},.initialAnimation=167,
        .spatialShots=actor.skills.spatial,.routine=falling?BloodTrail<true>:BloodTrail<false>};
    actor.spawnEvents.push_back({.position=position,.pattern=pattern,.difficulty=actor.difficulty,
        .sourceHealth=1200,.sourceScore=10,.seed=++actor.spawnIndex,.sourceItem=-2,.visual=actor.visual,
        .hitRadius=native_sizes::kDefaultEnemyRadius,.retirementBounds=kEmitterBounds,
        .bulletBase=actor.skills.bulletBase,.laserBase=actor.skills.laserBase,.defeatBase=stage_assets::kNativeDefeat,
        .fallback=actor.skills.fallback,.retireOutside=false,.damageable=false,.visible=false,
        .stageClearResponse=behavior::StageClearResponse::Remove});
}
}
Routine FlandreBoss::Behaviors::CreepingBloodstainPrelude(Actor& actor) {
    actor.SetAmbientEffects(effects::AmbientMode::Active);
    actor.ClearBullets();actor.defeatActors=true;actor.SetEmitterPeriod(0);actor.SetEmitterSound(7);
    actor.visible=actor.interactable=actor.contactEnabled=true;actor.damageable=false;actor.deathEffect=2;
    actor.SetPoses({160,163,164,161,162});actor.skills.offset={0,0,0.535714269F};
    auto& motion=Motion(actor);motion.bounds=kBattleBounds;motion.clamped=true;motion.Interpolate(110,{0,0,1.42857146F},2);
    if(actor.liftedMotion)actor.liftedMotion->Aim(actor.motion,-1.25F+2.5F*spatial::RadicalInverse(actor.spatialIndex++,2));
    co_await actor.At(30);actor.dropEvents.push_back(40);actor.itemEvents.push_back(3);
    co_await actor.At(210);actor.pendingCallback=SkillComplete;
}
Routine FlandreBoss::Behaviors::CreepingBloodstain(Actor& actor) {
    actor.SetAmbientEffects(effects::AmbientMode::Living);
    actor.ClearBullets();actor.defeatActors=true;actor.SetEmitterPeriod(0);actor.SetEmitterSound(-1);
    actor.visible=actor.interactable=actor.contactEnabled=actor.damageable=true;
    actor.skills.offset={};actor.SetPoses({160,166,166,161,162});
    co_await actor.At(120);actor.soundEvents.push_back(5);actor.Animate(166);actor.SetPoses({166,166,166,161,162});
    // 准备内40轮外部118粒子占80帧，全部等待计入阶段时限；第0帧就开放受伤，不能照搬132的120帧保护。
    for(int n=0;n<40;++n) {co_await actor.At(120+n*2);actor.EmitEffect(effects::Kind::Charge,6,{128,128,255,255});}
    co_await actor.At(200);actor.SetEmitterSound(7);int wait=300;double next=200;
    for(;;) {
        actor.Animate(170);actor.SetPoses({170,166,166,161,162});float angle=Aim(actor);
        for(int n=0;n<10;++n) {SpawnTrail(actor,actor.Position(),angle,false);angle+=pi/5;Normalize(angle);}
        // 源计数跳转每轮确切消费wait帧，之后从局部1等到41；不是wait+41。
        next+=wait+40;co_await actor.At(next);if(wait>100)wait-=40;
        SpawnTrail(actor,{actor.Random(17.1428566F)-8.5714283F,0,10},0,true);
        for(int n=0;n<4;++n) {next+=60;co_await actor.At(next);SpawnTrail(actor,{actor.Random(17.1428566F)-8.5714283F,0,10},0,true);}
        next+=120;co_await actor.At(next);actor.soundEvents.push_back(5);actor.Animate(166);actor.SetPoses({166,166,166,161,162});
        for(int n=0;n<40;++n) {co_await actor.At(next+n*2);actor.EmitEffect(effects::Kind::Charge,6,{128,128,255,255});}
        next+=80;co_await actor.At(next);
    }
}
}
