// 本文件实现芙兰朵露额外第一符的交替扫描使魔；所有生产空间常量使用世界单位。
#include "gameplay/bosses/flandre.hpp"
#include "gameplay/spatial.hpp"
#include "stages/common/ids.hpp"
#include <numbers>
namespace koumakyou::bosses {
namespace {
using behavior::Actor;using behavior::Routine;
constexpr float pi=std::numbers::pi_v<float>;
constexpr movement::Bounds kEmitterBounds{{-10,-11.7857143F},{10,11.7857143F}};
void Normalize(float& angle) {while(angle>pi)angle-=2*pi;while(angle<-pi)angle+=2*pi;}
movement::State& Motion(Actor& actor) {
    if(actor.skills.spatial&&!actor.liftedMotion)actor.liftedMotion.emplace(actor.motion);
    return actor.liftedMotion?actor.liftedMotion->planar:actor.motion;
}
void Height(Actor& actor) {
    if(actor.liftedMotion)actor.liftedMotion->Aim(actor.motion,-1.25F+2.5F*spatial::RadicalInverse(actor.spatialIndex++,2));
}
float Aim(const Actor& actor) {
    const auto p=actor.PlayerPosition(),a=actor.Position();
    return p.x==a.x&&p.z==a.z?pi/2:std::atan2(a.z-p.z,p.x-a.x);
}
void Wander(Actor& actor) {
    if(actor.liftedMotion)actor.liftedMotion->Rebase(actor.motion);
    actor.RandomDirection(-pi,pi,false);auto& motion=Motion(actor);float angle=motion.angle;
    // 源随机巡航的反射带不随120帧时收窄的夹取矩形改变，不能从新bounds推导这些阈值。
    if(motion.position.x<-2.85714293F&&std::abs(angle)>pi/2)angle=std::copysign(pi,angle)-angle;
    if(motion.position.x>2.85714293F&&std::abs(angle)<pi/2)angle=std::copysign(pi,angle)-angle;
    if(motion.position.z>5.71428585F&&angle<0)angle=-angle;
    if(motion.position.z<6.78571415F&&angle>0)angle=-angle;
    motion.Polar(angle,3.21428585F);motion.Decelerate(90);Height(actor);
}
Routine WingEmitter(Actor& actor) {
    if(!actor.polarSweep)throw std::invalid_argument("Wing emitter requires polar sweep parameters");
    const auto sweep=*actor.polarSweep;
    actor.SetPoses({-1,-1,-1,-1,-1});actor.Animate(167);
    actor.damageable=actor.contactEnabled=actor.interactable=actor.visible=false;
    const auto p=actor.Position();const float direction=actor.motion.angle;
    // 源54以二次加速推进140内容帧，总路程18.75世界单位；逻辑使魔保持平面路线。
    actor.motion.Interpolate(140,{p.x+std::cos(direction)*18.75F,p.y,p.z-std::sin(direction)*18.75F},-2);
    actor.soundEvents.push_back(22);actor.skills.projectile={.acceleration=0.803571429F,.accelerationFrames=300,.sampleAtStepStart=true};
    float offsetAngle=actor.Random(2*pi)-pi,shotAngle=actor.Random(2*pi)-pi,radius=4.46428585F;
    int n=0;
    for(;;) {
        // 原生在本步ECL与插值终点补偿前回收；不能等公共Update末尾把越界对象再吸到插值终点。
        if(!kEmitterBounds.Contains(actor.Position())) {
            actor.motion.Polar(direction,0);actor.finished=true;co_return;
        }
        if(n<sweep.emissions&&actor.Frame()>=n) {
            actor.skills.offset={std::cos(offsetAngle)*radius,0,-std::sin(offsetAngle)*radius};
            // 先保存本轮偏移再递减半径；略过零后的负值保留，剩余发射次数不能因此被截短。
            if(radius>0)radius-=0.0401785709F;
            skill::Shot shot{skill::Formation::FixedFan,5,(1+2*n)%14,1,1,0,0,shotAngle,0,530};
            if(actor.skills.spatial) {shot.sampleIndex=n;shot.coneHalfAngle=0.5F;}
            actor.Fire(shot);offsetAngle+=sweep.offsetAngleStep;Normalize(offsetAngle);shotAngle+=sweep.shotAngleStep;Normalize(shotAngle);++n;
        }
        // 源尾部没有删除；发弹结束后只保留移动/回收检查，不补发、不重新读取随机数。
        co_await actor.At(actor.Frame()+0.5);
    }
}
void SpawnWing(Actor& actor,float angle,float offsetStep,float shotStep,int emissions) {
    const behavior::Pattern pattern{.movement={.angle=angle,.speed=0},.initialAnimation=167,
        .spatialShots=actor.skills.spatial,.routine=WingEmitter,.polarSweep=skill::PolarSweep{offsetStep,shotStep,emissions}};
    actor.spawnEvents.push_back({.position=actor.Position(),.pattern=pattern,.difficulty=actor.difficulty,
        .sourceHealth=1200,.sourceScore=10,.seed=++actor.spawnIndex,.sourceItem=-2,.visual=actor.visual,
        .hitRadius=native_sizes::kDefaultEnemyRadius,.retirementBounds=kEmitterBounds,
        .bulletBase=actor.skills.bulletBase,.laserBase=actor.skills.laserBase,.defeatBase=stage_assets::kNativeDefeat,
        // 本协程在步首按同一显式边界回收；公共步末回收会让最后一次插值补偿提前一帧删除对象。
        .fallback=actor.skills.fallback,.retireOutside=false,.damageable=false,.visible=false,
        .stageClearResponse=behavior::StageClearResponse::Remove});
}
}
Routine FlandreBoss::Behaviors::FragileWing(Actor& actor) {
    actor.SetAmbientEffects(effects::AmbientMode::Active);
    actor.ClearBullets();actor.defeatActors=true;actor.SetEmitterPeriod(0);actor.SetEmitterSound(-1);
    actor.visible=actor.interactable=actor.contactEnabled=true;actor.damageable=false;actor.deathEffect=2;
    actor.SetPoses({166,166,166,161,162});actor.skills.offset={};
    Motion(actor).bounds={{-7.14285714F,4.64285707F},{7.14285714F,7.85714293F}};Motion(actor).clamped=true;
    Motion(actor).Interpolate(110,{0,0,4.28571415F},2);Height(actor);
    co_await actor.At(120);actor.SetAmbientEffects(effects::AmbientMode::Living);Motion(actor).bounds={{-2.85714293F,3.57142854F},{2.85714293F,6.42857143F}};
    actor.damageable=true;actor.soundEvents.push_back(5);actor.Animate(171);actor.SetPoses({171,166,166,161,162});
    // 首次40轮外部118粒子占80帧；以后回跳到首发点，不重新蓄力或重置发射次数。
    for(int n=0;n<40;++n) {co_await actor.At(120+n*2);actor.EmitEffect(effects::Kind::Charge,6,{128,128,255,255});}
    co_await actor.At(200);actor.SetEmitterSound(7);int emissions=115;
    for(int cycle=0;;++cycle) {
        const int start=200+cycle*830;float step=-pi/14;
        SpawnWing(actor,Aim(actor),step,step*1.5F,emissions);
        co_await actor.At(start+120);step*=-1;SpawnWing(actor,Aim(actor),step,step*1.5F,emissions);Wander(actor);
        co_await actor.At(start+240);step*=-1;SpawnWing(actor,Aim(actor),step,step*1.5F,emissions);
        co_await actor.At(start+360);step*=-1;SpawnWing(actor,Aim(actor),step,step*1.5F,emissions);Wander(actor);
        co_await actor.At(start+480);actor.Animate(171);actor.SetPoses({171,166,166,161,162});actor.soundEvents.push_back(5);
        // 30轮×2帧后，源局部时钟还需从484等待到514，故扇形出生位于首发后570帧。
        for(int n=0;n<30;++n) {co_await actor.At(start+480+n*2);actor.EmitEffect(effects::Kind::Charge,6,{128,128,255,255});}
        co_await actor.At(start+570);float angle=-pi/2+0.392699003F+Aim(actor),shotStep=step*1.5F;emissions-=10;
        for(int n=0;n<5;++n) {SpawnWing(actor,angle,step,shotStep,emissions);angle+=0.589048982F;shotStep*=-1;Normalize(angle);}
        co_await actor.At(start+730);actor.Animate(160);actor.SetPoses({160,166,166,161,162});
        co_await actor.At(start+830);emissions+=10;if(emissions<140)emissions+=5;
    }
}
}
