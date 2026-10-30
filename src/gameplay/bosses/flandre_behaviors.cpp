// 本文件实现芙兰朵露入场与八段非符；周期发射独立于巡航协程，生产采用120 Hz三维运动。
#include "gameplay/bosses/flandre.hpp"
#include "stages/common/ids.hpp"
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
// 各配方只保存世界单位弹速和60 Hz内容帧；发射器与巡航各自计时，不能在巡航回跳时重启。
struct NonspellPattern {skill::Shot shot;int onset,period,deceleration;bool drop;bool attachHalo=false;};
constexpr NonspellPattern kFirstNonspell{{skill::Formation::FixedRing,2,2,64,2,5.35714286F,2.67857143F,0,0,513},70,60,60,false,true};
constexpr NonspellPattern kSecondNonspell{{skill::Formation::FixedRing,2,2,64,2,5.35714286F,2.67857143F,0,0,513},90,30,50,true};
constexpr NonspellPattern kThirdNonspell{{skill::Formation::FixedRing,2,2,32,3,10.7142857F,2.67857143F,0,0,513},90,60,90,true};
constexpr NonspellPattern kFourthNonspell{{skill::Formation::FixedRing,2,6,32,2,5.35714286F,2.67857143F,0,0,513},90,40,90,true};
constexpr NonspellPattern kFifthNonspell{{skill::Formation::FixedRing,2,6,32,2,6.69642878F,2.67857143F,0,0,513},90,30,90,true};
constexpr NonspellPattern kSixthNonspell{{skill::Formation::FixedRing,2,6,32,2,9.375F,2.67857143F,0,0,513},90,30,90,true};
constexpr NonspellPattern kSeventhNonspell{{skill::Formation::FixedRing,2,6,32,3,6.69642878F,2.67857143F,0,0.0654498488F,513},90,30,90,true};
constexpr NonspellPattern kEighthNonspell{{skill::Formation::AimedRing,2,6,5,6,14.7321424F,2.67857143F,0,0,513},90,20,90,true};
Routine Nonspell(Actor& actor,const NonspellPattern& pattern) {
    actor.SetAmbientEffects(effects::AmbientMode::Active);
    actor.ClearBullets();actor.defeatActors=true;actor.SetEmitterPeriod(0);actor.SetEmitterSound(7);
    actor.SetPoses({160,163,164,161,162});actor.interactable=actor.contactEnabled=true;actor.damageable=false;actor.deathEffect=2;
    actor.skills.offset={0,0,0.535714269F};actor.skills.projectile.sampleAtStepStart=true;
    Motion(actor).bounds={{-7.14285714F,4.64285707F},{7.14285714F,7.85714293F}};Motion(actor).clamped=true;
    co_await actor.At(30);actor.damageable=true;
    if(pattern.drop)actor.dropEvents.push_back(20);
    if(pattern.attachHalo) {
        // 五条轴独立出生，第一/第三条轴相同但相位年龄不同，不能合并或一次同时生成。
        actor.AttachEffect({-.3F,.3F,.7F},2.14285714F,{224,224,255,255});
        co_await actor.At(40);actor.AttachEffect({-1,.3F,.7F},2.14285714F,{224,224,255,255});
        co_await actor.At(50);actor.AttachEffect({-.3F,.3F,.7F},2.14285714F,{224,224,255,255});
        co_await actor.At(60);actor.AttachEffect({.4F,.3F,.7F},2.14285714F,{224,224,255,255});
        co_await actor.At(70);actor.AttachEffect({1.1F,.3F,.7F},2.14285714F,{224,224,255,255});
    }
    co_await actor.At(pattern.onset);
    // 固定环弹仅在配置时随机方向。第八段虽使用实时瞄准，源仍读取一次未采用的方向，须保持随机流。
    auto shot=pattern.shot;const float direction=actor.Random(2*pi)-pi;
    if(shot.formation==skill::Formation::FixedRing)shot.angle=direction;
    if(actor.skills.spatial)shot.spatialCount=shot.count*3;
    actor.ConfigureEmitter(shot);
    const int base=pattern.period;
    actor.SetEmitterPeriod(base+base/5-(base/5*2*actor.skills.rank)/32);
    for(int cycle=0;;++cycle) {
        co_await actor.At(pattern.onset+cycle*110);
        auto& motion=Motion(actor);if(actor.liftedMotion)actor.liftedMotion->Rebase(actor.motion);
        actor.RandomDirection(-pi,pi);motion.Apply({.speed=6.69642878F});motion.Decelerate(pattern.deceleration);Height(actor);
    }
}
}
void FlandreBoss::Behaviors::ConfigurePractice(Actor& actor,bool spatial) {
    actor.skills.spatial=spatial;actor.skills.bulletBase=stage_assets::kNativeBulletBase;
    actor.skills.laserBase=stage_assets::kNativeLaserBase;actor.skills.fallback=stage_assets::kBlueBullet;
    if(!spatial)actor.liftedMotion.reset();
    Motion(actor).bounds={{-7.14285714F,4.64285707F},{7.14285714F,7.85714293F}};Motion(actor).clamped=true;
    actor.SetPoses({160,163,164,161,162});actor.hitRadius=1.25F;
}
Routine FlandreBoss::Behaviors::Entrance(Actor& actor) {
    actor.damageable=actor.contactEnabled=false;actor.deathEffect=1;actor.hitRadius=1.25F;
    actor.skills.offset={0,0,0.535714269F};actor.SetPoses({160,163,164,161,162});
    actor.AnimateAuxiliary(0,18,stage_assets::kOrb);
    auto& motion=Motion(actor);motion.clamped=false;motion.position={-10,0,5.71428585F};
    // 直接测试或正式StartBoss均从同一世界出生点开始；不让配置的缓存位置覆盖首次源定位。
    actor.motion.position=motion.position;motion.Interpolate(120,{0,0,5.71428585F},2);Height(actor);
    co_await actor.At(120);motion.clamped=true;
    for(;;) {co_await actor.At(1120);actor.Rewind(120);}
}
Routine FlandreBoss::Behaviors::FirstNonspell(Actor& actor) {co_await Nonspell(actor,kFirstNonspell);}
Routine FlandreBoss::Behaviors::SecondNonspell(Actor& actor) {co_await Nonspell(actor,kSecondNonspell);}
Routine FlandreBoss::Behaviors::ThirdNonspell(Actor& actor) {co_await Nonspell(actor,kThirdNonspell);}
Routine FlandreBoss::Behaviors::FourthNonspell(Actor& actor) {co_await Nonspell(actor,kFourthNonspell);}
Routine FlandreBoss::Behaviors::FifthNonspell(Actor& actor) {co_await Nonspell(actor,kFifthNonspell);}
Routine FlandreBoss::Behaviors::SixthNonspell(Actor& actor) {co_await Nonspell(actor,kSixthNonspell);}
Routine FlandreBoss::Behaviors::SeventhNonspell(Actor& actor) {co_await Nonspell(actor,kSeventhNonspell);}
Routine FlandreBoss::Behaviors::EighthNonspell(Actor& actor) {co_await Nonspell(actor,kEighthNonspell);}
Routine FlandreBoss::Behaviors::SurvivalPrelude(Actor& actor) {
    actor.SetAmbientEffects(effects::AmbientMode::Active);
    actor.ClearBullets();actor.defeatActors=true;actor.SetEmitterPeriod(0);actor.SetEmitterSound(7);
    actor.SetPoses({160,163,164,161,162});actor.interactable=actor.contactEnabled=true;actor.damageable=false;actor.deathEffect=2;
    actor.skills.offset={0,0,0.535714269F};actor.skills.projectile.sampleAtStepStart=true;
    Motion(actor).bounds={{-7.14285714F,4.64285707F},{7.14285714F,7.85714293F}};Motion(actor).clamped=true;
    co_await actor.At(30);actor.damageable=true;actor.dropEvents.push_back(20);
    co_await actor.At(90);
    // 源在抑制发射期间配置一组六层弹，随后直接调用下一符；没有任何周期到期，也没有巡航随机读取。
    // 此处只保留该次角度读取，并把控制权交回配置层，不能用finished把整个Boss遭遇提前结束。
    actor.Random(2*pi);actor.pendingCallback=SkillComplete;
}
Routine FlandreBoss::Behaviors::FinalPrelude(Actor& actor) {
    actor.SetAmbientEffects(effects::AmbientMode::Active);
    actor.ClearBullets();actor.defeatActors=true;actor.SetEmitterPeriod(0);actor.SetEmitterSound(7);
    actor.visible=actor.interactable=actor.contactEnabled=true;actor.damageable=false;actor.deathEffect=2;
    actor.SetPoses({160,163,164,161,162});actor.skills.offset={0,0,0.535714269F};
    actor.skills.projectile.sampleAtStepStart=true;
    Motion(actor).bounds={{-7.14285714F,4.64285707F},{7.14285714F,7.85714293F}};Motion(actor).clamped=true;
    co_await actor.At(30);actor.dropEvents.push_back(20);
    actor.AttachEffect({-.3F,.3F,.7F},2.14285714F,{224,224,255,255});
    actor.AttachEffect({-1,.3F,.7F},2.14285714F,{224,224,255,255});
    actor.AttachEffect({-.3F,.3F,.7F},2.14285714F,{224,224,255,255});
    actor.AttachEffect({.4F,.3F,.7F},2.14285714F,{224,224,255,255});
    actor.AttachEffect({1.1F,.3F,.7F},2.14285714F,{224,224,255,255});
    co_await actor.At(90);actor.Random(2*pi);actor.pendingCallback=SkillComplete;
}
}
