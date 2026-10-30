// 本文件实现芙兰朵露Maze of Love的双向条件扇；角相位局部持有，运动与弹速均使用世界单位。
#include "gameplay/bosses/flandre.hpp"
#include "gameplay/spatial.hpp"
#include <cmath>
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
void Normalize(float& angle) {
    while(angle>pi)angle-=2*pi;
    while(angle<-pi)angle+=2*pi;
}
// 配方采用弧度与世界单位/秒。正反段分别保留源字面值，不能从圈数或平均速度重新推导门角。
// 三维球冠半张角取原扇最外侧1.5/11.5个间隔，保持窄扫描扇与宽弹墙的范围差异。
struct Sweep {int rounds,smallColor,largeColor;float phaseStep,phaseKick,gateStep;};
constexpr Sweep kForward{300,8,6,0.271616876F,0.392699093F,-0.628318548F};
constexpr Sweep kReverse{180,14,2,-0.271616876F,-0.392699093F,0.733038306F};
Routine SweepMaze(Actor& actor,const Sweep& sweep,float& phase) {
    float gate=pi/2;
    for(int round=0;round<sweep.rounds;++round) {
        co_await actor.At(round*2);
        skill::Shot narrow{skill::Formation::FixedFan,2,sweep.smallColor,4,1,3.75F,0,phase,0.0654498488F,512};
        narrow.coneHalfAngle=0.0981747732F;actor.Fire(narrow);
        phase+=sweep.phaseStep;Normalize(phase);
        // 比较原始角差的绝对值，不取最短弧；追加相位后不归一化，下一轮首发仍须看到该值。
        // 每次赋值维持float32边界，否则接近门阈值的分支会在长周期内漂移。
        const float difference=std::abs(phase-gate);
        if(difference<0.261799395F) {
            phase+=sweep.phaseKick;gate+=sweep.gateStep;Normalize(gate);
            skill::Shot wall{skill::Formation::FixedFan,3,sweep.largeColor,24,3,9.1071434F,4.8214283F,
                             phase+pi,0.196349546F,512};
            wall.coneHalfAngle=2.25801969F;actor.Fire(wall);
        }
    }
    co_await actor.At(sweep.rounds*2);
}
}
Routine FlandreBoss::Behaviors::MazeOfLove(Actor& actor) {
    actor.ClearBullets();actor.SetEmitterPeriod(0);actor.SetEmitterSound(-1);actor.defeatActors=true;
    actor.SetPoses({160,166,166,161,162});actor.damageable=false;actor.contactEnabled=true;
    actor.skills.offset={};actor.skills.projectile.sampleAtStepStart=true;
    Motion(actor).Interpolate(120,{0,0,6.42857143F},2);Height(actor);
    co_await actor.At(120);actor.SetAmbientEffects(effects::AmbientMode::Living);actor.damageable=true;
    Motion(actor).bounds={{-7.14285714F,-10},{7.14285714F,7.85714293F}};Motion(actor).clamped=true;
    Motion(actor).Interpolate(30,{0,0,0},2);Height(actor);
    for(;;) {
        float phase=1.5953400135F;
        co_await SweepMaze(actor,kForward,phase);
        // 两个子行为各消费600/360内容帧并冻结父局部时钟；父的60/7帧留白使整轮恰为1027帧。
        co_await actor.At(180);co_await SweepMaze(actor,kReverse,phase);
        co_await actor.At(187);actor.Rewind(120);
    }
}
}
