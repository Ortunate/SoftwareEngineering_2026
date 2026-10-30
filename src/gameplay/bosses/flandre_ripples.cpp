// 本文件实现芙兰朵露最终符的生命驱动环弹与60内容帧退场；生产空间量均为世界单位。
#include "gameplay/bosses/flandre.hpp"
#include "gameplay/spatial.hpp"
#include <numbers>
namespace koumakyou::bosses {
namespace {
using behavior::Actor;using behavior::Routine;
constexpr float pi=std::numbers::pi_v<float>;
movement::State& Motion(Actor& actor) {
    if(actor.skills.spatial&&!actor.liftedMotion)actor.liftedMotion.emplace(actor.motion);
    return actor.liftedMotion?actor.liftedMotion->planar:actor.motion;
}
void Height(Actor& actor) {
    if(actor.liftedMotion)actor.liftedMotion->Aim(actor.motion,-1.25F+2.5F*spatial::RadicalInverse(actor.spatialIndex++,2));
}
}
Routine FlandreBoss::Behaviors::RipplesOf495Years(Actor& actor) {
    actor.ClearBullets();actor.defeatActors=true;actor.SetEmitterPeriod(0);actor.SetEmitterSound(-1);
    actor.SetPoses({166,166,166,161,162});actor.damageable=false;actor.contactEnabled=true;actor.skills.offset={};
    Motion(actor).Interpolate(120,{0,0,6.42857143F},2);Height(actor);
    co_await actor.At(120);actor.SetAmbientEffects(effects::AmbientMode::Living);actor.damageable=true;actor.soundEvents.push_back(5);
    for(int n=0;n<30;++n) {co_await actor.At(120+n*2);actor.EmitEffect(effects::Kind::Charge,6,{128,128,255,255});}
    co_await actor.At(180);actor.SetEmitterSound(7);
    const auto size=battle::EnemyBulletLogicalSize(2);
    actor.skills.projectile={.sampleAtStepStart=true,.bounceCount=1,.bounceSpeed=-1,
        .bounceLower={-8.57142857F,-10,-10},.bounceUpper={8.57142857F,10,10},
        .bounceHalfExtents={size.x/2,size.x/2,size.y/2}};
    double next=180;
    for(;;) {
        // 原生16读取整数生命；截止点依据阶段总钟，不能使用会回拨的协程局部时钟。
        const int hp=actor.phaseFrames<7200?static_cast<int>(actor.health):0;
        const float speed=(2-static_cast<float>(hp)/6000)*2.67857143F;
        const int wait=40+hp*240/6000;
        const float angle=actor.Random(2*pi)-pi;
        actor.Fire({skill::Formation::FixedRing,2,6,88,1,speed,2.67857143F,angle,0,2560});
        // 先发当前轮，再保存下一轮偏移。三维高度用独立面积序列，不改变角/X/Z的随机读取顺序。
        const float width=14.2857141F-static_cast<float>(hp)*7.14285707F/6000;
        const float depth=5.71428585F-static_cast<float>(hp)*2.85714293F/6000;
        const float x=-width*.5F+actor.Random(width),z=5.71428585F+depth*.5F-actor.Random(depth);
        const float y=actor.skills.spatial?-1.25F+2.5F*spatial::RadicalInverse(actor.spatialIndex++,2):actor.Position().y;
        actor.skills.offset={x-actor.Position().x,y-actor.Position().y,z-actor.Position().z};
        // 源的计数回跳消费wait帧，随后4→2还需一帧；不能把间隔缩短成wait或多加一次调用帧。
        next+=wait+1;co_await actor.At(next);
    }
}
Routine FlandreBoss::Behaviors::FinalExit(Actor& actor,bool damage) {
    actor.SetAmbientEffects(effects::AmbientMode::Disabled);
    actor.interactable=true;actor.damageable=actor.contactEnabled=false;actor.SetEmitterPeriod(0);
    actor.RandomDirection(-pi,pi);Motion(actor).Apply({.speed=1.60714293F});Motion(actor).Decelerate(60);Height(actor);
    constexpr std::array<Color,5> colors{WHITE,Color{255,128,128,255},Color{128,255,128,255},Color{128,128,255,255},Color{255,255,128,255}};
    for(int round=0;round<6;++round) {
        for(int n=0;n<5;++n) {co_await actor.At(round*10+(n+1)*2);actor.EmitEffect(effects::Kind::Burst,2,colors[n]);}
        actor.soundEvents.push_back(7);
    }
    // 91设置模式3：伤害击破在进入95前已消费，超时仍保留；后者在本步写HP0后恢复1，下一步执行96删除。
    actor.soundEvents.push_back(18);actor.ClearBullets();actor.health=damage?0:1;
    if(!damage)co_await actor.At(60+actor.ContentStepFrames());
    actor.finished=true;
}
}
