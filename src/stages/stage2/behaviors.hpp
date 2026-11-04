// 本文件定义二面普通演员配方与大妖精、琪露诺命名行为；时间为 60 Hz 内容帧，空间量均为世界单位。
#pragma once
#include "gameplay/actors/behavior.hpp"
namespace koumakyou::stage2 {
/// @brief 初始化减速球体的随机周期发射；配方初速/动画已由 Follow 应用，行为独占实例状态。
behavior::Routine DeceleratingShooter(behavior::Actor& actor);
/// @brief 球体被击破后第 1 内容帧的四向攻击；行为结束即释放原槽位，不能再次结算掉落。
behavior::Routine OrbDeathVolley(behavior::Actor& actor);
/// @brief 出生时锁定 X/Z 瞄准方向，再于 180～280 内容帧转弯；不消耗随机数。
behavior::Routine AimedTurningFairy(behavior::Actor& actor);
/// @brief 出生时仅抽取一次向下的随机方向，初速和外观由配方分别决定。
behavior::Routine WanderingOrb(behavior::Actor& actor);

// 五种斜向减速球体共享几何与技能；仅蓝色款有随机周期发射及击破攻击。
// 固定初速 8.035714 世界单位/秒、加速度 -2.410714 世界单位/秒²，不因显示 FPS 改变。
/// @brief 按难度修改红球周期射击；只修改传入的配方副本。
void ConfigureRedOrbDifficulty(behavior::Pattern& pattern,native_combat::Difficulty difficulty);
/// @brief 按难度修改暗精灵的定时扇形；调用前配方须包含该定时技能。
void ConfigureDarkFairyDifficulty(behavior::Pattern& pattern,native_combat::Difficulty difficulty);
inline constexpr behavior::Pattern kLeftBlueOrb{
    .movement={.angle=0.785398185F,.speed=8.03571415F},.initialAnimation=13,.deathEffect=0,
    .spatialShots=true,.routine=DeceleratingShooter,.onDefeated=OrbDeathVolley};
inline constexpr behavior::Pattern kLeftRedOrb{
    .movement={.angle=1.17809725F,.speed=8.03571415F,.keys={{{.frame=0,.acceleration=-2.41071439F}}},.count=1},
    .initialAnimation=14,.deathEffect=0,.spatialShots=true,.configureDifficulty=ConfigureRedOrbDifficulty};
inline constexpr behavior::Pattern kCenterBlueOrb{
    .movement={.angle=1.57079637F,.speed=8.03571415F},.initialAnimation=13,.deathEffect=0,
    .spatialShots=true,.routine=DeceleratingShooter,.onDefeated=OrbDeathVolley};
inline constexpr behavior::Pattern kRightRedOrb{
    .movement={.angle=1.96349537F,.speed=8.03571415F,.keys={{{.frame=0,.acceleration=-2.41071439F}}},.count=1},
    .initialAnimation=14,.deathEffect=0,.spatialShots=true,.configureDifficulty=ConfigureRedOrbDifficulty};
inline constexpr behavior::Pattern kRightBlueOrb{
    .movement={.angle=2.3561945F,.speed=8.03571415F},.initialAnimation=13,.deathEffect=0,
    .spatialShots=true,.routine=DeceleratingShooter,.onDefeated=OrbDeathVolley};
inline constexpr behavior::Pattern kAimedFairy{
    .movement={.speed=6.4285717F},.spatialShots=true,.routine=AimedTurningFairy};
inline constexpr behavior::Pattern kSmallDriftingOrb{
    .movement={.speed=8.03571415F},.initialAnimation=12,.spatialShots=true,.routine=WanderingOrb};
inline constexpr behavior::Pattern kBlueDriftingOrb{
    .movement={.speed=13.3928576F},.initialAnimation=8,.spatialShots=true,.routine=WanderingOrb,.rotateAnimation=true};
inline constexpr behavior::Pattern kRedDriftingOrb{
    .movement={.speed=13.3928576F},.initialAnimation=9,.spatialShots=true,.routine=WanderingOrb,.rotateAnimation=true};
inline constexpr behavior::Pattern kGreenDriftingOrb{
    .movement={.speed=13.3928576F},.initialAnimation=10,.spatialShots=true,.routine=WanderingOrb,.rotateAnimation=true};
inline constexpr behavior::Pattern kYellowDriftingOrb{
    .movement={.speed=13.3928576F},.initialAnimation=11,.spatialShots=true,.routine=WanderingOrb,.rotateAnimation=true};
inline constexpr behavior::Pattern kShootingDarkFairy{
    .movement={.angle=1.57079637F,.speed=5.35714293F,.keys={{{.frame=60,.speed=0.0F},
        {.frame=130,.angularVelocity=3.14159274F,.acceleration=8.03571415F},
        {.frame=190,.angularVelocity=0.0F}}},.count=3},
    .skill=skill::TimedShot{70,{skill::Formation::AimedFan,1,1,7,2,3.75F,2.14285707F,0,0.628318548F,3,21,1.1F}},
    .initialAnimation=3,.stoppedAnimation=5,.deathEffect=0,.muzzleOffset={0.535714269F,0,0.535714269F},.spatialShots=true,.configureDifficulty=ConfigureDarkFairyDifficulty};
/// @brief 配置二面发弹素材与三维模式；仅修改当前演员，不借用外部资源；spatial=false 用于源时钟对照。
void ConfigureBehavior(behavior::Actor& actor,bool spatial=true);

}
