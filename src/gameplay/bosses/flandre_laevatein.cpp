// 本文件实现芙兰朵露莱瓦汀的准备、挥剑和沿光束采样；生产仅保存世界几何与命名时序。
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
void Move(Actor& actor,int frames,Vector3 target) {
    if(actor.liftedMotion)actor.liftedMotion->Rebase(actor.motion);
    Motion(actor).Interpolate(frames,target,2);
    if(actor.liftedMotion)actor.liftedMotion->Aim(actor.motion,-1.25F+2.5F*spatial::RadicalInverse(actor.spatialIndex++,2));
}
// 三维剑面绕世界Z倾斜，保留剑长、发弹数和直角侧射关系；平面对照的倾角恒为零。
Vector3 Direction(float angle,float tilt) {return {std::cos(angle)*std::cos(tilt),std::cos(angle)*std::sin(tilt),-std::sin(angle)};}
void Relocate(Actor& actor,float angle,float tilt) {
    if(auto* beam=actor.Beam(0)) {
        beam->beamAnchor=actor.Position();beam->beamDirection=Direction(angle,tilt);
        const auto d=beam->beamDirection,p=beam->beamAnchor;const float start=beam->beamStartOffset;
        beam->position={p.x+d.x*start,p.y+d.y*start,p.z+d.z*start};
    }
}
void Sword(Actor& actor,int slot,float angle,float tilt,bool trail,bool first=false) {
    actor.FireBeam(slot,skill::Laser{.color=2,.length=first||trail?15.8928566F:15.0F,.width=trail?0.535714269F:1.07142854F,
        .grow=trail?1.0F:60.0F,.sustain=trail?6.0F:800.0F,.fade=trail?1.0F:20.0F,.warning=trail?1.0F:60.0F,
        .harmlessTail=trail?8.0F:18.0F,.aimed=false,.startOffset=1.42857146F,
        .direction=Direction(angle,tilt),.animationScript=1});
}
// 原生92直接转入淡出并重启该段时钟，不能删掉长激光，也不能继续保留原800帧的危险窗口。
void FadeSword(Actor& actor) {
    if(auto* beam=actor.Beam(0)) {
        beam->maxAge=beam->age+beam->beamFadeSeconds;
        beam->dangerEndSeconds=beam->age+2.0F/60;
    }
}
int SampleSwords(Actor& actor,const skill::Shot& shot) {
    int active=0;
    for(int slot=0;slot<8;++slot)if(const auto* beam=actor.Beam(slot)) {
        ++active;
        const auto p=beam->beamAnchor,d=beam->beamDirection;
        // 起距闭、终距开；每48源像素对应固定世界步长，不能随光束长度重新均分。
        // 用整数序号计算相对距离，避免反复加起距的舍入在恰好七个间隔的剑上多发终点一弹。
        for(int sample=0;sample*2.14285707F<beam->beamLength;++sample) {
            const float distance=beam->beamStartOffset+sample*2.14285707F;
            actor.FireAt(shot,{p.x+d.x*distance,p.y+d.y*distance,p.z+d.z*distance});
        }
    }
    return active;
}
float Wrap(float angle) {while(angle>pi)angle-=2*pi;while(angle<-pi)angle+=2*pi;return angle;}
Routine Sweep(Actor& actor,int frames,float angle,float sideAngle,float step,float tilt) {
    float swordAngle=angle;
    for(int frame=0;frame<frames;++frame) {
        co_await actor.At(frame);
        Sword(actor,1,angle,tilt,true);swordAngle+=step;Relocate(actor,swordAngle,tilt);
        sideAngle=Wrap(sideAngle+step);angle=Wrap(angle+step);
        if(frame%5==0) {
            actor.skills.projectile.acceleration=3.21428561F;actor.skills.projectile.angularVelocity=0;
            actor.skills.projectile.accelerationFrames=96;actor.skills.projectile.accelerationVector.reset();
            skill::Shot shot{skill::Formation::FixedFan,2,2,1,1,0.0267857146F,2.67857143F,sideAngle,0,532};
            if(actor.skills.spatial)shot.spatialAxis=Direction(sideAngle,tilt);
            actor.Fire(shot);
            // 原生14返回活动槽数而非弹数；空槽分支直接把局部时钟跳到返回点，同步交还父行为，不能误加一帧等待。
            if(SampleSwords(actor,shot)==0) {FadeSword(actor);co_return;}
        }
    }
    co_await actor.At(frames);FadeSword(actor);
}
Routine CarrySword(Actor& actor,int frames,float angle,float tilt) {
    for(int frame=0;frame<frames;++frame) {co_await actor.At(frame);Relocate(actor,angle,tilt);}
    co_await actor.At(frames);
}
}
Routine FlandreBoss::Behaviors::Laevatein(Actor& actor) {
    actor.ClearBullets();actor.defeatActors=true;actor.SetEmitterPeriod(0);actor.SetEmitterSound(-1);actor.ClearBeamSlots();
    actor.SetPoses({160,163,164,161,162});actor.damageable=false;actor.contactEnabled=true;
    actor.skills.offset={};actor.skills.projectile.sampleAtStepStart=true;
    Move(actor,88,{0,0,6.42857143F});
    co_await actor.At(90);actor.SetPoses({172,166,166,161,162});
    co_await actor.At(120);actor.SetAmbientEffects(effects::AmbientMode::Living);actor.damageable=true;
    Motion(actor).bounds={{-8.5714283F,4.64285707F},{8.5714283F,10}};Motion(actor).clamped=true;
    for(int cycle=0;;++cycle) {
        const float tilt=actor.skills.spatial?(cycle%2?-.42F:.42F):0;
        actor.SetPoses({173,173,173,173,173});Sword(actor,0,-2.98451304F,tilt,false,true);actor.soundEvents.push_back(16);
        co_await Sweep(actor,120,-2.98451304F,-1.41371667F,0.0392699093F,tilt);
        actor.SetPoses({160,163,164,161,162});Move(actor,120,{-8.5714283F,0,8.21428585F});
        co_await actor.At(240+cycle*240); // 父局部时钟不包括Sweep/CarrySword的时长。
        actor.SetPoses({174,175,175,175,175});Sword(actor,0,pi/2,tilt,false);actor.soundEvents.push_back(16);
        co_await CarrySword(actor,60,pi/2,tilt);
        Move(actor,90,{7.14285707F,0,5.71428585F});co_await Sweep(actor,64,pi/2,0,0,tilt);
        actor.SetPoses({160,163,164,161,162});Move(actor,30,{8.5714283F,0,8.21428585F});
        co_await CarrySword(actor,30,pi/2,tilt);
        actor.SetPoses({174,175,175,175,175});Sword(actor,0,pi/2,tilt,false);actor.soundEvents.push_back(16);
        co_await actor.At(300+cycle*240);
        Move(actor,90,{-7.14285707F,0,7.76785707F});co_await Sweep(actor,64,pi/2,pi,0,tilt);
        actor.SetPoses({160,163,164,161,162});Move(actor,120,{0,0,6.42857143F});
        Sword(actor,0,-0.157079637F,tilt,false);actor.soundEvents.push_back(16);
        co_await Sweep(actor,120,-0.157079637F,-1.72787595F,-0.0392699093F,tilt);
        actor.SetPoses({172,172,172,172,172});co_await actor.At(360+cycle*240);
    }
}
}
