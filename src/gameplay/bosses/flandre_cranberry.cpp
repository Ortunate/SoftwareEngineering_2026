// 本文件实现芙兰朵露Cranberry Trap及边界发射体；生产路线和速度均为世界单位，不解码源寄存器。
#include "gameplay/bosses/flandre.hpp"
#include "gameplay/spatial.hpp"
#include "stages/common/ids.hpp"
#include <numbers>
namespace koumakyou::bosses {
namespace {
using behavior::Actor;using behavior::Routine;
enum class Corner {TopLeft,TopRight,BottomRight,BottomLeft};
enum class Aim {Center,Player};
constexpr std::array<Vector3,4> kCorners{{{-8.5714283F,0,10},{8.5714283F,0,10},{8.5714283F,0,-10},{-8.5714283F,0,-10}}};
// 路线作为编译期配方保存；出生HP只保留源值，不再承担生产控制流。每边十发相隔12帧；源循环结束后还等1帧才启动下一边，不能压成无缝120帧。
template<Corner Start,int Edges,bool Clockwise,Aim Target>
Routine EdgeEmitter(Actor& actor) {
    actor.Animate(16);actor.damageable=actor.contactEnabled=false;actor.hitRadius=0.625F;
    actor.skills.projectile.sampleAtStepStart=true;
    co_await actor.At(60);actor.SetEmitterSound(7);
    int corner=static_cast<int>(Start);actor.motion.position=kCorners[corner];
    for(int edge=0;edge<Edges;++edge) {
        co_await actor.At(60+edge*121);
        corner=(corner+(Clockwise?1:3))%4;
        actor.motion.Interpolate(120,kCorners[corner],2);
        for(int n=0;n<10;++n) {
            co_await actor.At(60+edge*121+n*12);
            const auto p=actor.Position();
            const float angle=std::atan2(p.z,-p.x);
            skill::Shot shot{Target==Aim::Player?skill::Formation::AimedFan:skill::Formation::FixedFan,
                Target==Aim::Player?3:1,Target==Aim::Player?6:4,1,1,3.48214269F,2.67857143F,Target==Aim::Player?0:angle,0,512};
            // 单发扫描保留每12帧一弹的节奏，沿球冠补高度，不复制三份弹，也不读取攻击随机流。
            if(actor.skills.spatial)shot.sampleIndex=static_cast<int>(actor.spatialIndex++&0x7fffffffU);
            actor.Fire(shot);
        }
    }
    co_await actor.At(60+Edges*121);actor.InterruptAnimation(1);
    co_await actor.At(90+Edges*121);actor.finished=true;
}
template<Corner Start,int Edges,bool Clockwise,Aim Target>
void SpawnEdge(Actor& actor,int health) {
    const behavior::Pattern pattern{.movement={.speed=0},.initialAnimation=16,.deathEffect=0,.spatialShots=actor.skills.spatial,
        .routine=EdgeEmitter<Start,Edges,Clockwise,Target>};
    // 源出生X=1.3只是后续弹速的载体；其前60帧的真实位置仍按绝对世界点保存，不做往返换算。
    // 独立子种子沿项目spawnIndex显式递增，与父攻击随机流及事件表行号无关。
    actor.spawnEvents.push_back({.position={-8.51339245F,0,10},.pattern=pattern,.difficulty=actor.difficulty,
        .sourceHealth=static_cast<float>(health),.sourceScore=10,.seed=++actor.spawnIndex,.sourceItem=-2,
        .visual=stage_assets::kOrb,.hitRadius=0.625F,.bulletBase=actor.skills.bulletBase,.laserBase=actor.skills.laserBase,
        .defeatBase=stage_assets::kNativeDefeat,.fallback=actor.skills.fallback,.retireOutside=false,.damageable=false});
}
movement::State& Motion(Actor& actor) {
    if(actor.skills.spatial&&!actor.liftedMotion)actor.liftedMotion.emplace(actor.motion);
    return actor.liftedMotion?actor.liftedMotion->planar:actor.motion;
}
void Wander(Actor& actor) {
    if(actor.liftedMotion)actor.liftedMotion->Rebase(actor.motion);
    actor.RandomDirection(-std::numbers::pi_v<float>,std::numbers::pi_v<float>);
    Motion(actor).Apply({.speed=6.69642878F});Motion(actor).Decelerate(90);
    if(actor.liftedMotion)actor.liftedMotion->Aim(actor.motion,-1.25F+2.5F*spatial::RadicalInverse(actor.spatialIndex++,2));
}
}
behavior::Routine FlandreBoss::Behaviors::CranberryTrap(behavior::Actor& actor) {
    actor.ClearBullets();actor.SetEmitterPeriod(0);actor.SetEmitterSound(-1);actor.defeatActors=true;
    actor.SetPoses({160,166,166,161,162});actor.damageable=false;actor.contactEnabled=true;
    actor.skills.offset={};actor.skills.projectile.sampleAtStepStart=true;
    Motion(actor).Interpolate(120,{0,0,6.42857143F},2);
    if(actor.liftedMotion)actor.liftedMotion->Aim(actor.motion,-1.25F+2.5F*spatial::RadicalInverse(actor.spatialIndex++,2));
    co_await actor.At(120);actor.SetAmbientEffects(effects::AmbientMode::Living);actor.damageable=true;
    for(int cycle=0;;++cycle) {
        const int start=120+cycle*807;
        co_await actor.At(start);
        SpawnEdge<Corner::TopLeft,2,false,Aim::Center>(actor,20);SpawnEdge<Corner::BottomRight,2,false,Aim::Center>(actor,22);Wander(actor);
        co_await actor.At(start+160);
        SpawnEdge<Corner::TopRight,2,true,Aim::Player>(actor,1121);SpawnEdge<Corner::BottomLeft,2,true,Aim::Player>(actor,1123);Wander(actor);
        co_await actor.At(start+320);
        SpawnEdge<Corner::TopLeft,2,false,Aim::Center>(actor,20);SpawnEdge<Corner::BottomRight,2,false,Aim::Center>(actor,22);
        co_await actor.At(start+400);
        SpawnEdge<Corner::TopRight,1,true,Aim::Center>(actor,111);SpawnEdge<Corner::BottomLeft,1,true,Aim::Center>(actor,113);Wander(actor);
        co_await actor.At(start+600);
        SpawnEdge<Corner::TopRight,1,true,Aim::Player>(actor,1111);SpawnEdge<Corner::BottomLeft,1,true,Aim::Player>(actor,1113);
        SpawnEdge<Corner::TopLeft,1,false,Aim::Player>(actor,1010);SpawnEdge<Corner::BottomRight,1,false,Aim::Player>(actor,1012);Wander(actor);
        co_await actor.At(start+800);
        SpawnEdge<Corner::TopRight,1,true,Aim::Center>(actor,111);SpawnEdge<Corner::BottomLeft,2,true,Aim::Center>(actor,123);
        SpawnEdge<Corner::TopLeft,1,false,Aim::Center>(actor,10);SpawnEdge<Corner::BottomRight,2,false,Aim::Center>(actor,22);
    }
}
}
