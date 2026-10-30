// 本文件实现芙兰朵露条件后继的入场与短命演出对象；所有空间常量均使用世界单位。
#include "gameplay/bosses/flandre.hpp"
#include "stages/common/ids.hpp"
namespace koumakyou::bosses {
namespace {
using behavior::Actor;using behavior::Routine;
// 入场演出保留源平面的位置与镜像组合；不取随机高度，避免相同中心的多个残像在三维中散开。
Routine ConvergingAfterimage(Actor& actor) {
    actor.SetPoses({-1,-1,-1,-1,-1});actor.contactEnabled=false;
    actor.motion.Interpolate(90,{0,0,5.71428585F},2);
    co_await actor.At(90);actor.finished=true;
}
Routine StationaryAfterimage(Actor& actor) {
    actor.SetPoses({-1,-1,-1,-1,-1});actor.contactEnabled=false;
    co_await actor.At(90);actor.finished=true;
}
Routine HiddenConvergence(Actor& actor) {
    actor.SetPoses({-1,-1,-1,-1,-1});actor.contactEnabled=false;
    // 原生58选取1-r^4缓出；不能按指令顺序猜成三次插值。
    actor.motion.Interpolate(60,{0,0,5.71428585F},4);
    // 源118每两内容帧请求6粒子，40轮后在80帧直接删除；运动先在60帧收束。
    for(int n=0;n<40;++n) {co_await actor.At(n*2);actor.EmitEffect(effects::Kind::Charge,6,{255,0,0,255});}
    co_await actor.At(80);actor.finished=true;
}
// 演出模板不继承普通敌人的默认巡航速度；静止残像和插值结束后的对象都必须保持原位。
constexpr behavior::Pattern kConvergingAfterimage{.movement={.speed=0},.initialAnimation=168,.routine=ConvergingAfterimage};
constexpr behavior::Pattern kStationaryAfterimage{.movement={.speed=0},.initialAnimation=169,.routine=StationaryAfterimage};
constexpr behavior::Pattern kHiddenConvergence{.movement={.speed=0},.initialAnimation=160,.routine=HiddenConvergence};
void SpawnImage(Actor& actor,Vector3 position,const behavior::Pattern& pattern,bool visible=true) {
    // 98/99/100没有105或103，沿用原生模板的受伤开关和12源像素全宽；104只关闭接触。
    // 隐藏100沿用项目显式Remove清场适配，不能由此声称与原生96的特殊演员保留窗口等价。
    actor.spawnEvents.push_back({.position=position,.pattern=pattern,.difficulty=actor.difficulty,
        .sourceHealth=15000,.sourceScore=200000,.seed=++actor.spawnIndex,.sourceItem=-2,
        .visual=actor.visual,.hitRadius=native_sizes::kDefaultEnemyRadius,
        .bulletBase=actor.skills.bulletBase,.laserBase=actor.skills.laserBase,
        .defeatBase=stage_assets::kNativeDefeat,.fallback=actor.skills.fallback,.retireOutside=false,
        .visible=visible,.stageClearResponse=visible?behavior::StageClearResponse::Defeat:behavior::StageClearResponse::Remove});
}
}
Routine FlandreBoss::Behaviors::EncoreEntrance(Actor& actor) {
    actor.damageable=actor.contactEnabled=actor.visible=false;actor.deathEffect=2;actor.hitRadius=1.25F;
    actor.SetEmitterPeriod(0);actor.SetPoses({-1,-1,-1,-1,-1});actor.Animate(166);
    actor.skills.offset={0,0,0.535714269F};actor.AnimateAuxiliary(0,18,stage_assets::kOrb);
    actor.liftedMotion.reset();actor.motion={};actor.motion.position={0,0,5.71428585F};
    actor.soundEvents.push_back(5);SpawnImage(actor,{0,0,-8.5714283F},kHiddenConvergence,false);
    co_await actor.At(30);
    SpawnImage(actor,{0,0,5.71428585F},kConvergingAfterimage);
    SpawnImage(actor,{0,0,4.28571415F},kStationaryAfterimage);
    co_await actor.At(38);
    SpawnImage(actor,{-1.42857146F,0,6.07142878F},kConvergingAfterimage);
    SpawnImage(actor,{0,0,4.64285707F},kStationaryAfterimage);
    co_await actor.At(46);
    SpawnImage(actor,{0.357142866F,0,4.28571415F},kConvergingAfterimage);
    SpawnImage(actor,{0,0,5},kStationaryAfterimage);
    co_await actor.At(54);
    SpawnImage(actor,{-0.357142866F,0,7.14285707F},kConvergingAfterimage);
    SpawnImage(actor,{0,0,5.35714293F},kStationaryAfterimage);
    co_await actor.At(62);
    SpawnImage(actor,{1.42857146F,0,6.07142878F},kConvergingAfterimage);
    SpawnImage(actor,{0,0,5.71428585F},kStationaryAfterimage);
    co_await actor.At(100);actor.Animate(166);actor.visible=true;
    actor.motion.bounds={{-7.14285714F,4.64285707F},{7.14285714F,7.85714293F}};actor.motion.clamped=true;
    // 1100→100只是保活回跳，不重新出生残像；对白结束须由ResumeCombat放行，不按演出长度猜测。
    for(;;) {co_await actor.At(1100);actor.Rewind(100);}
}
}
