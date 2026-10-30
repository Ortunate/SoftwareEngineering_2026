// 本文件实现第 3 面普通演员的命名行为与资源绑定；Boss 专属行为由角色模块拥有。
#include "stages/stage3/behaviors.hpp"
#include "stages/stage3/ids.hpp"
#include "gameplay/spatial.hpp"
#include <numbers>
#include <cmath>
namespace koumakyou::stage3 {
namespace {
using behavior::Actor;using behavior::Routine;using skill::Formation;
constexpr float pi=std::numbers::pi_v<float>;

float Aim(const Actor& a) {const auto p=a.PlayerPosition(),q=a.Position();return std::atan2(q.z-p.z,p.x-q.x);}
void Fire(Actor& a,skill::Shot shot) {
    if(a.skills.spatial) {
        shot.spatialCount=shot.count>1||shot.formation==Formation::FixedFan||shot.formation==Formation::AimedFan ? shot.count*3 : 1;
        if(shot.formation==Formation::RandomAngle||shot.formation==Formation::RandomAngleSpeed) {
            // 随机单发也须跨轮推进高度序列；每轮从零采样会使所有雨弹集中在同一个切面。
            shot.sampleIndex=static_cast<int>(a.spatialIndex&0x7fffffffU);a.spatialIndex+=shot.spatialCount*shot.layers;
        }
        if(shot.count==1&&(shot.formation==Formation::FixedRing||shot.formation==Formation::AimedRing)) {
            shot.formation=Formation::AimedFan;shot.sampleIndex=static_cast<int>(a.spatialIndex++&0x7fffffffU);shot.coneHalfAngle=pi;
        }
    }
    a.Fire(shot);
}
/// @brief 停顿前沿用上一帧速度；加速度从 40 帧开启，70 帧关闭，不能提前把负速度钳成零。
Routine StopForAttack(Actor& a,float acceleration) {
    co_await a.At(40);a.motion.Apply({.acceleration=acceleration});
    co_await a.At(70);a.motion.Apply({.acceleration=0});
}
/// @brief 按难度执行原连续波；附加环在首波前发射，随机读取与单发时序不变。
Routine Stream(Actor& a,int count,float angle,float increment,int fanCount=1,float spacing=0.130899698F,bool openingRing=false) {
    // 附加环保留瞄准阵型：angle 是额外旋转，即使已取自瞄准角也不能省去阵型自身的瞄准。
    if(openingRing)Fire(a,{Formation::AimedRing,3,6,16,1,4.28571415F,0,angle,0.392699093F,4});
    float speed=4.28571415F;
    for(int shot=0;shot<count;++shot) {
        co_await a.At(shot*2);Fire(a,{Formation::FixedFan,4,6,fanCount,1,speed,0,angle,spacing,4});
        if((count-shot)%3==0)a.soundEvents.push_back(22);
        speed+=increment;
    }
    co_await a.At(count*2);
}
Routine Lifetime(Actor& a) {
    co_await a.At(std::max(0,10000-static_cast<int>(a.phaseFrames)));a.finished=true;
}
void Depart(Actor& a,float speed) {a.motion.Polar(a.Random(pi/2)+pi/4,speed);}
/// @brief 普通妖精自动发射以配置时 rank 快照计算周期；读取一次整数初相，帧末先增计数。
int Period(Actor& a,int base) {return base+base/5-(base/5*2*a.skills.rank)/32;}
/// @brief 弹幕快照与初相按源顺序设置；结束帧停发，同时关闭转弯速度。
Routine MovingEmitter(Actor& a,float angular,int stop) {
    const bool lunatic=a.difficulty==native_combat::Difficulty::Lunatic;
    skill::Shot shot=lunatic ? skill::Shot{Formation::FixedRing,4,6,10,2,8.03571415F,2.67857143F,0,0.130899698F,4} :
        skill::Shot{Formation::AimedFan,4,6,1,1,5.35714293F,0,0,0.130899698F,4};
    if(a.skills.spatial)shot.spatialCount=shot.count*3;
    a.ConfigureEmitter(shot);
    const int period=Period(a,lunatic?200:60);
    a.SetEmitterPeriod(period,lunatic?static_cast<int>(a.RandomIndex(period)):0);
    co_await a.At(30);a.motion.Apply({.angularVelocity=angular});
    co_await a.At(stop);a.motion.Apply({.angularVelocity=0});a.SetEmitterPeriod(0);
    co_await Lifetime(a);
}
Routine HookOrb(Actor& a) {co_await MovingEmitter(a,-3.92699099F,115);}
Routine RisingOrb(Actor& a) {co_await MovingEmitter(a,2.09439516F,90);}

}
void ConfigureHookOrb(behavior::Pattern& p,native_combat::Difficulty d) {
    if(native_combat::DifficultyIndex(d)>=2) {p.movement.count=0;p.routine=HookOrb;}
}
void ConfigureRisingOrb(behavior::Pattern& p,native_combat::Difficulty d) {
    if(native_combat::DifficultyIndex(d)>=2) {p.movement.count=0;p.routine=RisingOrb;}
}
void ConfigureBehavior(Actor& a,bool spatial) {
    a.skills.bulletBase=kNativeBulletBase;a.skills.laserBase=kNativeLaserBase;a.skills.fallback=kBlueBullet;a.skills.spatial=spatial;
    if(!spatial)a.liftedMotion.reset();
}
Routine AcceleratingStream(Actor& a) {
    co_await StopForAttack(a,-8.03571415F);
    const int d=native_combat::DifficultyIndex(a.difficulty);
    constexpr int counts[]{15,30,30,30},fans[]{1,1,3,5};
    constexpr float spacing[]{0.130899698F,0.130899698F,0.392699093F,0.261799395F};
    co_await Stream(a,counts[d],Aim(a),0.5625F,fans[d],spacing[d],d>=2);
    Depart(a,4.01785707F);co_await Lifetime(a);
}
Routine RetreatingRing(Actor& a) {
    co_await StopForAttack(a,-8.03571415F);
    const float speed=a.Random(2.67857143F)+2.67857143F;
    const int d=native_combat::DifficultyIndex(a.difficulty);
    constexpr int counts[]{8,16,24,24},layers[]{1,1,1,3};
    constexpr float spacing[]{0.52359879F,0.261799395F,0.209439516F,0.224399477F};
    Fire(a,{Formation::AimedFan,1,2,counts[d],layers[d],speed,d>=2?2.67857143F:0,0,spacing[d],4});a.soundEvents.push_back(8);
    const float direction=a.Random(pi/2)+pi/4;
    co_await a.At(30);a.motion.Polar(direction,-4.01785707F);
    co_await Lifetime(a);
}
Routine TurningShooter(Actor& a) {
    const int d=native_combat::DifficultyIndex(a.difficulty);
    constexpr int periods[]{300,190,120,90},counts[]{1,3,3,7};
    const int interval=Period(a,periods[d]),initial=static_cast<int>(a.RandomIndex(interval));const auto context=a.skills;
    int shot=interval-initial-1;
    for(int frame=0;frame<=220;++frame) {
        co_await a.At(frame);
        if(frame==40)a.motion.Apply({.angularVelocity=-1.47262156F});
        if(frame==120)a.motion.Apply({.angularVelocity=1.17809725F});
        if(frame==220)a.motion.Apply({.angularVelocity=0});
        if(frame==shot&&frame<220) {
            a.FireAt({Formation::AimedFan,0,1,counts[d],1,d>=2?6.69642878F:4.01785707F,0,0,d==3?0.034906585F:0.052359879F,4,9},a.Position(),&context);shot+=interval;
        }
    }
    co_await Lifetime(a);
}
Routine RisingStream(Actor& a) {
    co_await StopForAttack(a,-8.03571415F);
    const int d=native_combat::DifficultyIndex(a.difficulty);
    constexpr int counts[]{12,16,16,16},fans[]{1,1,1,5};
    constexpr float spacing[]{0.130899698F,0.130899698F,0.392699093F,0.52359879F};
    co_await Stream(a,counts[d],pi/2,0.803571463F,fans[d],spacing[d],d==2);
    Depart(a,-4.01785707F);co_await Lifetime(a);
}
Routine RainOrb(Actor& a) {
    co_await StopForAttack(a,-13.3928576F);
    a.skills.projectile.accelerationVector=Vector3{0,0,-4.01785707F};
    constexpr int counts[]{8,14,20,22};
    Fire(a,{Formation::RandomAngleSpeed,3,15,counts[native_combat::DifficultyIndex(a.difficulty)],1,5.35714293F,0.803571463F,0,-pi,25});a.soundEvents.push_back(22);
    Depart(a,4.01785707F);co_await Lifetime(a);
}
Routine SummonedStream(Actor& a) {
    co_await a.At(40);a.motion.Apply({.acceleration=-8.03571415F});
    for(int repeat=0;repeat<4;++repeat) {
        co_await a.At(70);a.motion.Apply({.acceleration=0});
        const bool hard=native_combat::DifficultyIndex(a.difficulty)>=2;
        co_await Stream(a,30,Aim(a),0.482142895F,hard?3:1,hard?0.52359879F:0.130899698F,hard);a.Rewind(72);
        co_await a.At(232);if(repeat<3)a.Rewind(40);
    }
    Depart(a,4.01785707F);co_await Lifetime(a);
}

}
