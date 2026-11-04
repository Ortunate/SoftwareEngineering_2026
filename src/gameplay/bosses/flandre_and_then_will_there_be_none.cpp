// 本文件实现芙兰朵露生存符的隐藏追踪使魔和五种边界弹幕；世界坐标与60 Hz内容帧独立于显示频率。
#include "gameplay/bosses/flandre.hpp"
#include "gameplay/spatial.hpp"
#include "stages/common/ids.hpp"
#include <numbers>

namespace koumakyou::bosses {
namespace {
using behavior::Actor;using behavior::Routine;
constexpr float pi=std::numbers::pi_v<float>;
enum class BorderPattern {Inward,Layered,Split,Clockwise,Counterclockwise};
movement::State& Motion(Actor& actor) {
    if(actor.skills.spatial&&!actor.liftedMotion)actor.liftedMotion.emplace(actor.motion);
    return actor.liftedMotion?actor.liftedMotion->planar:actor.motion;
}
Routine Pursuer(Actor& actor) {
    actor.visible=actor.interactable=actor.damageable=actor.contactEnabled=false;
    actor.SetPoses({-1,-1,-1,-1,-1});actor.Animate(16);actor.hitRadius=1.25F;
    actor.skills.rankInfluence={};actor.skills.offset={};actor.skills.projectile.sampleAtStepStart=true;
    skill::Shot fan{skill::Formation::AimedFan,3,6,3,1,1.33928573F,0,0,pi/3,skill::SpawnSmall|skill::PlaySound};
    fan.coneHalfAngle=pi/3;actor.ConfigureEmitter(fan);actor.SetEmitterSound(27);
    // 零rank弹速修正不关闭周期的全局rank修正；6帧基准仍按原整数算式得到5/6/7帧。
    actor.SetEmitterPeriod(7-(2*actor.skills.rank)/32);
    const int traversals=static_cast<int>(actor.health)%10;
    for(int n=0;n<traversals;++n) {
        co_await actor.At(60+n*192);
        // 每段开始捕获一次当前自机位置；192帧内不重新瞄准移动终点，周期发射计数持续运行。
        // 三维模式直接追向自机真实高度，不额外读取攻击随机流。
        actor.motion.Interpolate(192,actor.PlayerPosition(),2);
    }
    co_await actor.At(60+traversals*192);actor.finished=true;
}
void SpawnPursuer(Actor& actor,Vector3 position,int traversals) {
    const behavior::Pattern pattern{.movement={.speed=0},.initialAnimation=16,.deathEffect=0,
        .spatialShots=actor.skills.spatial,.routine=Pursuer};
    actor.spawnEvents.push_back({.position=position,.pattern=pattern,.difficulty=actor.difficulty,.sourceHealth=static_cast<float>(traversals),
        .sourceScore=10,.seed=++actor.spawnIndex,.sourceItem=-2,.visual=stage_assets::kOrb,.hitRadius=1.25F,
        .bulletBase=actor.skills.bulletBase,.laserBase=actor.skills.laserBase,.defeatBase=stage_assets::kNativeDefeat,
        .fallback=actor.skills.fallback,.retireOutside=false,.damageable=false,.visible=false,
        .stageClearResponse=behavior::StageClearResponse::Remove});
}
void Emit(Actor& actor,Vector3 point,int color,int count,int layers,float first,float last,float angle,float spacing,int flags=skill::SpawnLarge) {
    skill::Shot shot{skill::Formation::FixedFan,3,color,count,layers,first,last,angle,spacing,flags};
    shot.coneHalfAngle=count>1?(count-1)*.5F*spacing:pi/4;
    // 单发边界扫描在三维球冠中连续取样，避免每轮都重复同一高度；不消费源随机流。
    if(count==1&&layers==1)shot.sampleIndex=static_cast<int>(actor.spatialIndex++&0x7fffffffU);
    actor.FireAt(shot,point);
}
void Borders(Actor& actor,BorderPattern pattern) {
    const bool alternate=pattern==BorderPattern::Layered||pattern==BorderPattern::Clockwise;
    const int color=pattern==BorderPattern::Inward?2:pattern==BorderPattern::Layered?6:pattern==BorderPattern::Split?10:pattern==BorderPattern::Clockwise?14:13;
    actor.skills.offset={};
    // 四角朝内的五速弹先于四条边；角点交错和方向顺序均是源图案的一部分。
    if(alternate) {
        Emit(actor,{-9.10714245F,0,10},color,1,5,8.5714283F,2.67857146F,0,0);
        Emit(actor,{8.5714283F,0,10.5357141F},color,1,5,8.5714283F,2.67857146F,pi/2,0);
        Emit(actor,{9.10714245F,0,-10},color,1,5,8.5714283F,2.67857146F,pi,0);
        Emit(actor,{-8.5714283F,0,-10.5357141F},color,1,5,8.5714283F,2.67857146F,-pi/2,0);
    } else {
        Emit(actor,{-8.5714283F,0,10.5357141F},color,1,5,8.5714283F,2.67857146F,pi/2,0);
        Emit(actor,{9.10714245F,0,10},color,1,5,8.5714283F,2.67857146F,pi,0);
        Emit(actor,{8.5714283F,0,-10.5357141F},color,1,5,8.5714283F,2.67857146F,-pi/2,0);
        Emit(actor,{-9.10714245F,0,-10},color,1,5,8.5714283F,2.67857146F,0,0);
    }
    const bool curved=pattern==BorderPattern::Clockwise||pattern==BorderPattern::Counterclockwise;
    if(curved) {
        actor.skills.projectile.angularFrames=220;
        actor.skills.projectile.angularVelocity=pattern==BorderPattern::Clockwise?-.448798954F:.448798954F;
        actor.skills.projectile.speedAcceleration=0;
    }
    for(int edge=0;edge<4;++edge) {
        const bool vertical=edge<2;
        const int count=pattern==BorderPattern::Inward?(vertical?14:12):curved?(vertical?11:9):(vertical?10:9);
        float along=actor.Random(1.42857146F);
        float angle=curved ? (pattern==BorderPattern::Clockwise?(edge==0||edge==2?pi/2:edge==1?pi:0):(edge==1?pi/2:edge==3?-pi/2:0)) : (edge==0?0:edge==1?pi:edge==2?pi/2:-pi/2);
        const float step=pattern==BorderPattern::Inward?1.42857146F:curved?1.78571427F:2.0F;
        for(int n=0;n<count;++n) {
            Vector3 point=vertical?Vector3{edge==0?(curved?-8.5714283F:-9.10714245F):(curved?8.5714283F:9.10714245F),0,10-along}:
                Vector3{-8.5714283F+along,0,edge==2?(curved?10:10.5357141F):(curved?-10:-10.5357141F)};
            if(pattern==BorderPattern::Inward)angle=std::atan2(point.z,-point.x);
            if(curved)Emit(actor,point,color,1,1,2.67857146F,2.67857146F,angle,pi/4,skill::SpawnLarge|skill::Velocity);
            else if(pattern==BorderPattern::Inward)Emit(actor,point,color,1,1,3.21428585F,0,angle,pi/3);
            else if(pattern==BorderPattern::Layered)Emit(actor,point,color,1,2,4.01785707F,1.875F,angle,0);
            else Emit(actor,point,color,2,1,2.14285707F,2.67857146F,angle,pi/3);
            along+=step;
            if(curved) {
                const float increment=vertical?.142799661F:.17453292F;
                angle+=(edge==0||edge==3)?-increment:increment;
                if(pattern==BorderPattern::Counterclockwise&&edge==2) {
                    while(angle>pi)angle-=2*pi;while(angle<-pi)angle+=2*pi;
                }
            }
        }
    }
    actor.soundEvents.push_back(7);
}
}
Routine FlandreBoss::Behaviors::AndThenWillThereBeNone(Actor& actor) {
    actor.defeatActors=true;actor.SetEmitterPeriod(0);actor.SetEmitterSound(-1);
    actor.SetPoses({160,166,166,161,162});actor.damageable=false;actor.contactEnabled=true;actor.skills.offset={};
    actor.skills.projectile.sampleAtStepStart=true;
    Motion(actor).Interpolate(120,{0,0,6.42857143F},2);
    if(actor.liftedMotion)actor.liftedMotion->Aim(actor.motion,-1.25F+2.5F*spatial::RadicalInverse(actor.spatialIndex++,2));
    co_await actor.At(120);actor.SetAmbientEffects(effects::AmbientMode::Living);actor.soundEvents.push_back(14);
    actor.EmitEffect(effects::Kind::Flash,1,{255,128,128,255});
    actor.visible=actor.interactable=actor.damageable=actor.contactEnabled=false;
    SpawnPursuer(actor,{0,0,5.71428585F},4);
    co_await actor.At(920);SpawnPursuer(actor,{0,0,5.71428585F},4);
    co_await actor.At(984);SpawnPursuer(actor,{-8.5714283F,0,10},4);
    co_await actor.At(1048);SpawnPursuer(actor,{8.5714283F,0,10},3);
    co_await actor.At(1112);SpawnPursuer(actor,{-8.5714283F,0,-10},3);
    co_await actor.At(1176);SpawnPursuer(actor,{8.5714283F,0,-10},2);
    // 边界子程序全部为零时长，同帧返回；最后一组在时限之后，不得提前压入有效战斗窗口。
    constexpr std::array<int,15> at{1976,2296,2616,2936,3256,3576,3816,4056,4296,4536,4676,4816,4956,5096,5236};
    constexpr std::array<BorderPattern,5> order{BorderPattern::Inward,BorderPattern::Layered,BorderPattern::Split,BorderPattern::Clockwise,BorderPattern::Counterclockwise};
    for(std::size_t n=0;n<at.size();++n) {co_await actor.At(at[n]);Borders(actor,order[n%order.size()]);}
    for(;;)co_await actor.At(actor.Frame()+1);
}
}
