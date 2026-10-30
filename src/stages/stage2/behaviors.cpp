// 本文件实现第 2 面普通演员的命名行为与资源绑定；Boss 专属行为由角色模块拥有。
#include "stages/stage2/behaviors.hpp"
#include "stages/stage2/ids.hpp"
#include "gameplay/spatial.hpp"
#include <array>
#include <cmath>
#include <numbers>
namespace koumakyou::stage2 {
namespace {
using behavior::Actor;
using behavior::Routine;
using skill::Formation;
constexpr float pi=std::numbers::pi_v<float>;

/// @brief Boss 移动保持原 X/Z 路程预算，仅以独立序号分配保守高度，不消耗攻击随机流。

float AimAngle(const Actor& a) {
    const auto p=a.PlayerPosition(),origin=a.Position();return std::atan2(origin.z-p.z,p.x-origin.x);
}
/// @brief 多向图案直接采样三倍方向预算；逐发单方向环转为连续球面扫描，避免所有弹停留在同一切面。
/// 中性对照不改变图案和随机读取；空间序号独立于演员的随机流。
void Fire(Actor& a,skill::Shot shot) {
    if(a.skills.spatial) {
        shot.spatialCount=shot.count>1 ? shot.count*3 : 1;
        if(shot.count==1&&(shot.formation==Formation::FixedRing||shot.formation==Formation::AimedRing)) {
            shot.formation=Formation::AimedFan;shot.sampleIndex=static_cast<int>(a.spatialIndex++);
            shot.coneHalfAngle=pi;
        }
    }
    a.Fire(shot);
}
/// @brief 琪露诺两组非符/符卡共用姿态槽；中断/退场不重新初始化整场演出的视觉年龄。

}
void ConfigureDarkFairyDifficulty(behavior::Pattern& pattern,native_combat::Difficulty difficulty) {
    const int index=native_combat::DifficultyIndex(difficulty);
    if(index<0)return;
    // 定时扇形来自同一时间点的互斥难度指令，移动和外观不变。
    auto& shot=pattern.skill->shot;
    shot.count=std::array{3,7,9,11}[index];shot.layers=std::array{1,2,3,5}[index];
    shot.firstSpeed=std::array{3.75F,3.75F,3.75F,6.4285717F}[index];
    shot.spacing=std::array{0.785398185F,0.628318548F,0.392699093F,0.261799395F}[index];
}
void ConfigureRedOrbDifficulty(behavior::Pattern& pattern,native_combat::Difficulty difficulty) {
    // Lunatic 下红球也启用周期发射；协程在出生时应用同一减速度。
    if(difficulty==native_combat::Difficulty::Lunatic) {
        pattern.movement.count=0;pattern.routine=DeceleratingShooter;
    }
}
void ConfigureBehavior(Actor& a,bool spatial) {
    a.skills.bulletBase=kNativeBulletBase;a.skills.laserBase=kNativeLaserBase;a.skills.fallback=kBlueBullet;
    a.skills.spatial=spatial;if(!spatial)a.liftedMotion.reset();
}
Routine DeceleratingShooter(Actor& a) {
    a.motion.Apply({.acceleration=-2.41071439F});
    // 源随机初相在配置周期时读取一次；计时器在同一内容帧末先递增，故首发减 1。
    // 周期与 rank 弹数/速度都是配置时快照，不能在自动发射时重新读取变化后的 rank。
    // 原生先对 rank 项作整数截断再加端点；合并为一次浮点截断会使部分 rank 的周期少一帧。
    if(a.difficulty==native_combat::Difficulty::Easy) {
        co_await a.At(10000);a.finished=true;co_return;
    }
    const int interval=180+36-static_cast<int>(a.skills.rank*72/32.0F);
    const int initial=static_cast<int>(a.RandomIndex(interval));
    const int cutoff=std::abs(a.motion.angle-0.785398185F)<0.001F ? 180 : 200;
    const auto configuration=a.skills;
    const bool lunatic=a.difficulty==native_combat::Difficulty::Lunatic;
    const skill::Shot shot{lunatic ? Formation::AimedRing : Formation::AimedOffsetRing,4,11,
        lunatic ? 10 : 4,lunatic ? 2 : 1,lunatic ? 6.69642878F : 5.35714293F,0,0,0,4,lunatic ? 30 : 12};
    for(int frame=interval-initial-1;frame<cutoff;frame+=interval) {
        co_await a.At(frame);a.FireAt(shot,a.Position(),&configuration);
    }
    co_await a.At(10000);a.finished=true;
}
Routine OrbDeathVolley(Actor& a) {
    const int index=native_combat::DifficultyIndex(a.difficulty);
    co_await a.At(1);
    Fire(a,{Formation::AimedOffsetRing,2,2,std::array{2,4,6,10}.at(index),1,
        std::array{3.21428585F,3.21428585F,4.82142878F,5.35714293F}.at(index),0,0,0,4});
    a.finished=true;
}
Routine AimedTurningFairy(Actor& a) {
    a.motion.Polar(AimAngle(a),6.4285717F);
    if(a.difficulty!=native_combat::Difficulty::Lunatic) {
        co_await a.At(180);a.motion.Apply({.angularVelocity=-1.47262156F});
        co_await a.At(280);a.motion.Apply({.angularVelocity=0});
        co_await a.At(10000);a.finished=true;co_return;
    }
    // 自动发射与运动共用局部时钟；排序合并到期点以保持同帧先移动变更再发射。
    const int interval=120+24-static_cast<int>(a.skills.rank*48/32.0F);
    int next=interval-static_cast<int>(a.RandomIndex(interval))-1;
    const auto configuration=a.skills;
    const skill::Shot shot{Formation::AimedFan,0,6,3,1,6.69642878F,0,0,0.196349546F,4,9};
    for(int frame=0;frame<10000;++frame) {
        if(frame!=next&&frame!=180&&frame!=280)continue;
        co_await a.At(frame);
        if(frame==180)a.motion.Apply({.angularVelocity=-1.47262156F});
        if(frame==280)a.motion.Apply({.angularVelocity=0});
        if(frame==next) {a.FireAt(shot,a.Position(),&configuration);next+=interval;}
    }
    co_await a.At(10000);a.finished=true;
}
Routine WanderingOrb(Actor& a) {
    a.RandomDirection(0.785398185F,2.3561945F,false);
    co_await a.At(10000);a.finished=true;
}

}
