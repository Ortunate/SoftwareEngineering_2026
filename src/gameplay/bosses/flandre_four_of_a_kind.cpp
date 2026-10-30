// 本文件实现芙兰朵露Four of a Kind及三个独立分身；世界单位配方不依赖源子程序编号。
#include "gameplay/bosses/flandre.hpp"
#include "gameplay/spatial.hpp"
#include "stages/common/ids.hpp"
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
void Wander(Actor& actor) {
    if(actor.liftedMotion)actor.liftedMotion->Rebase(actor.motion);
    actor.RandomDirection(-pi,pi);Motion(actor).Apply({.speed=3.21428585F});Motion(actor).Decelerate(90);Height(actor);
}
void StartRings(Actor& actor,int count,int base,float angle) {
    skill::Shot shot{skill::Formation::FixedRing,2,2,count,1,5.35714286F,2.67857143F,angle,0,513};
    actor.ConfigureEmitter(shot);
    const int period=base+base/5-(base/5*2*actor.skills.rank)/32;
    // 原生77使用一次完整32位随机读取取模；不能用Random(1)*period替代，也不能在回跳时保留旧计数。
    actor.SetEmitterPeriod(period,static_cast<int>(actor.RandomIndex(period)));
}
// 分身的出生、入位、发射周期及蓄力弹独立；roamFrames/cycleFrames均为60 Hz内容帧。
struct CloneRecipe {Vector3 birth,target;int period,roamFrames,cycleFrames;skill::Shot burst;};
constexpr CloneRecipe kLeftClone{{-10,0,5.71428585F},{-5.71428585F,0,5.71428585F},60,390,580,
    {skill::Formation::FixedRing,6,3,40,3,8.03571415F,2.67857143F,0,0,512}};
constexpr CloneRecipe kUpperClone{{2.85714293F,0,11.4285717F},{2.85714293F,0,5.71428585F},40,290,480,
    {skill::Formation::FixedRing,6,6,8,10,13.3928576F,2.67857143F,0,0,512}};
constexpr CloneRecipe kRightClone{{10,0,5.71428585F},{5.71428585F,0,5.71428585F},40,340,530,
    {skill::Formation::AimedFan,6,5,7,5,8.5714283F,2.67857143F,0,0.314159274F,512}};
template<const CloneRecipe& Recipe>
Routine Clone(Actor& actor) {
    actor.damageable=false;actor.SetPoses({160,163,164,161,162});actor.Animate(160);actor.deathEffect=2;
    actor.skills.offset={0,0,0.535714269F};actor.skills.projectile.sampleAtStepStart=true;
    auto& motion=Motion(actor);motion.clamped=false;motion.position=Recipe.birth;actor.motion.position=motion.position;
    motion.Interpolate(60,Recipe.target,2);Height(actor);
    co_await actor.At(60);actor.damageable=true;
    motion.bounds={{-7.14285714F,4.64285707F},{7.14285714F,7.85714293F}};motion.clamped=true;actor.SetEmitterSound(7);
    for(int cycle=0;;++cycle) {
        const int start=60+cycle*Recipe.cycleFrames;
        co_await actor.At(start);const float angle=actor.Random(2*pi)-pi;
        StartRings(actor,32,Recipe.period,angle);Wander(actor);
        co_await actor.At(start+Recipe.roamFrames);actor.SetEmitterPeriod(0);actor.soundEvents.push_back(5);
        co_await actor.At(start+Recipe.roamFrames+100);
        auto burst=Recipe.burst;if constexpr(&Recipe==&kLeftClone)burst.angle=angle;
        actor.Fire(burst);
    }
}
template<const CloneRecipe& Recipe>
void SpawnClone(Actor& actor) {
    const behavior::Pattern pattern{.movement={.speed=0},.initialAnimation=160,.deathEffect=2,
        .spatialShots=actor.skills.spatial,.routine=Clone<Recipe>};
    // 原请求的位置为源原点，分身首步自行绝对定位；出生种子独立递增，不消耗本体的攻击随机流。
    actor.spawnEvents.push_back({.position={-8.5714283F,0,10},.pattern=pattern,.difficulty=actor.difficulty,
        .sourceHealth=1200,.sourceScore=10,.seed=++actor.spawnIndex,.sourceItem=2,
        .visual=actor.visual,.hitRadius=1.25F,.bulletBase=actor.skills.bulletBase,.laserBase=actor.skills.laserBase,
        .defeatBase=stage_assets::kNativeDefeat,.fallback=actor.skills.fallback,.retireOutside=false,.damageable=false});
}
}
Routine FlandreBoss::Behaviors::FourOfAKind(Actor& actor) {
    actor.ClearBullets();actor.SetEmitterPeriod(0);actor.SetEmitterSound(-1);actor.defeatActors=true;
    actor.SetPoses({160,166,166,161,162});actor.damageable=false;actor.contactEnabled=true;
    actor.skills.offset={};actor.skills.projectile.sampleAtStepStart=true;
    Motion(actor).Interpolate(120,{0,0,6.42857143F},2);Height(actor);
    co_await actor.At(120);actor.SetAmbientEffects(effects::AmbientMode::Living);actor.damageable=true;actor.soundEvents.push_back(5);
    // 118(17,6,color)在120…213每3帧发出外部粒子；最后一次循环检查在216帧才放行分身。
    for(int n=0;n<32;++n) {co_await actor.At(120+n*3);actor.EmitEffect(effects::Kind::Charge,6,{128,128,255,255});}
    co_await actor.At(216);
    Motion(actor).Interpolate(60,{-2.85714293F,0,5.71428585F},2);Height(actor);
    SpawnClone<kLeftClone>(actor);SpawnClone<kUpperClone>(actor);SpawnClone<kRightClone>(actor);
    co_await actor.At(276);
    // 源先配置32×2环弹再同帧覆盖成48×1；废弃配置仍读取一次随机方向。
    static_cast<void>(actor.Random(2*pi));
    for(int cycle=0;;++cycle) {
        const int start=276+cycle*760;
        co_await actor.At(start);const float angle=actor.Random(2*pi)-pi;
        StartRings(actor,48,40,angle);
        for(int move=0;move<3;++move) {co_await actor.At(start+move*190);Wander(actor);}
        co_await actor.At(start+570);actor.SetEmitterPeriod(0);actor.soundEvents.push_back(5);
        co_await actor.At(start+670);
        actor.Fire({skill::Formation::FixedRing,6,3,40,3,8.03571415F,2.67857143F,angle,0.0628318563F,512});
    }
}
}
