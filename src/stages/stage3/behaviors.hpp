// 本文件声明三面普通演员配方、美铃命名阶段和动态小怪；时间采用 60 Hz 内容帧，空间量均为世界单位。
#pragma once
#include "gameplay/actors/behavior.hpp"
namespace koumakyou::stage3 {
/// @brief 减速后逐发加速的蓝弹串；锁定进入攻击时的方向，完成后独立选择退场方向。
behavior::Routine AcceleratingStream(behavior::Actor& actor);
/// @brief 红色环形单次发射，出生时前进、停顿后倒退离场。
behavior::Routine RetreatingRing(behavior::Actor& actor);
/// @brief 缓慢转弯的普通妖精，随机初相的周期瞄准发射在 220 帧关闭。
behavior::Routine TurningShooter(behavior::Actor& actor);
/// @brief 固定向下的短弹串，随后向上退场；与瞄准款分开保存攻击参数。
behavior::Routine RisingStream(behavior::Actor& actor);
/// @brief 随机速率球环带固定世界加速度，发射后选择向下离场方向。
behavior::Routine RainOrb(behavior::Actor& actor);
/// @brief Boss 召唤的四轮瞄准小怪；独占生命、技能、随机流和协程，不借用 Boss 状态。
behavior::Routine SummonedStream(behavior::Actor& actor);
inline constexpr behavior::Pattern kBlueHookOrb{
    .movement={.angle=0.52359879F,.speed=12.0535717F,.keys={{{.frame=30,.angularVelocity=-3.92699099F},
        {.frame=115,.angularVelocity=0}}},.count=2},.initialAnimation=13,.deathEffect=0,.spatialShots=true};
inline constexpr behavior::Pattern kRedHookOrb{
    .movement={.angle=0.52359879F,.speed=12.0535717F,.keys={{{.frame=30,.angularVelocity=-3.92699099F},
        {.frame=115,.angularVelocity=0}}},.count=2},.initialAnimation=14,.deathEffect=0,.spatialShots=true};
inline constexpr behavior::Pattern kBlueRisingOrb{
    .movement={.angle=-1.04719758F,.speed=10.7142859F,.keys={{{.frame=30,.angularVelocity=2.09439516F},
        {.frame=90,.angularVelocity=0}}},.count=2},.initialAnimation=13,.deathEffect=0,.spatialShots=true};
inline constexpr behavior::Pattern kAimedStreamOrb{
    .movement={.speed=4.01785707F},.initialAnimation=15,.deathEffect=0,.spatialShots=true,.routine=AcceleratingStream};
inline constexpr behavior::Pattern kRetreatingRingOrb{
    .movement={.speed=4.01785707F},.initialAnimation=16,.deathEffect=0,.spatialShots=true,.routine=RetreatingRing};
inline constexpr behavior::Pattern kTurningShooter{
    .movement={.speed=5.35714293F},.initialAnimation=0,.spatialShots=true,.routine=TurningShooter};
inline constexpr behavior::Pattern kRisingStreamOrb{
    .movement={.speed=4.01785707F},.initialAnimation=15,.deathEffect=0,.spatialShots=true,.routine=RisingStream};
inline constexpr behavior::Pattern kRainOrb{
    .movement={.speed=6.69642878F},.initialAnimation=15,.deathEffect=0,.spatialShots=true,.routine=RainOrb};
inline constexpr behavior::Pattern kSummonedStream{
    .movement={.speed=4.01785707F},.initialAnimation=15,.deathEffect=0,.spatialShots=true,.routine=SummonedStream};
/// @brief 初始化本关共用弹种素材和三维模式；中性对照显式传 false，不影响独立随机流。
void ConfigureBehavior(behavior::Actor& actor,bool spatial=true);

}
