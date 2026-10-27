// 本文件声明四面普通演员的命名配方；角度、速度和坐标均为世界约定，内容帧保持 60 Hz。
#pragma once
#include "gameplay/actors/behavior.hpp"
namespace koumakyou::stage4 {
/// @brief 减速晶体在停顿点生成独立发射体，之后选择一次退场方向。
behavior::Routine DescendingSpawner(behavior::Actor& actor);
behavior::Routine CrossingSpawner(behavior::Actor& actor);
behavior::Routine LaserSpawner(behavior::Actor& actor);
/// @brief 六种发射晶体共享周期计数实现，路线及开火窗口按各命名入口分别给出。
behavior::Routine HookFanShooter(behavior::Actor& actor);
behavior::Routine SwerveFanShooter(behavior::Actor& actor);
behavior::Routine HookLayerShooter(behavior::Actor& actor);
behavior::Routine SwerveLayerShooter(behavior::Actor& actor);
behavior::Routine DescendingFanShooter(behavior::Actor& actor);
behavior::Routine CrossingFanShooter(behavior::Actor& actor);
behavior::Routine PassingHookCrystal(behavior::Actor& actor);
behavior::Routine PassingRisingCrystal(behavior::Actor& actor);
/// @brief 停顿晶体发射 64 轮双速度层旋转环，击破后按正常掉落外另散落三个道具。
behavior::Routine StrongSpiralCrystal(behavior::Actor& actor);
behavior::Routine GentleSpiralCrystal(behavior::Actor& actor);
behavior::Routine CrystalDefeated(behavior::Actor& actor);
/// @brief 固定场上发射体先关闭交互，完成轮次后中断 ANM 并保留淡出等待；退场不能清掉已发弹。
behavior::Routine GreenRingEmitter(behavior::Actor& actor);
behavior::Routine BlueRingEmitter(behavior::Actor& actor);
behavior::Routine OffsetRingEmitter(behavior::Actor& actor);
behavior::Routine EmitterDefeated(behavior::Actor& actor);
behavior::Routine HiddenRandomEmitter(behavior::Actor& actor);
behavior::Routine HiddenLaserEmitter(behavior::Actor& actor);
inline constexpr behavior::Pattern kDescendingSpawner{
    .movement={.speed=5.35714293F},.initialAnimation=12,.deathEffect=0,.spatialShots=true,.routine=DescendingSpawner};
inline constexpr behavior::Pattern kCrossingSpawner{
    .movement={.angle=0,.speed=5.35714293F},.initialAnimation=12,.deathEffect=0,.spatialShots=true,.routine=CrossingSpawner};
inline constexpr behavior::Pattern kLaserSpawner{
    .movement={.speed=5.35714293F},.initialAnimation=12,.deathEffect=0,.spatialShots=true,.routine=LaserSpawner};
inline constexpr behavior::Pattern kHookFanShooter{
    .movement={.angle=0,.speed=11.25F},.initialAnimation=8,.deathEffect=0,.spatialShots=true,.routine=HookFanShooter};
inline constexpr behavior::Pattern kSwerveFanShooter{
    .movement={.angle=-1.04719758F,.speed=11.25F},.initialAnimation=8,.deathEffect=0,.spatialShots=true,.routine=SwerveFanShooter};
inline constexpr behavior::Pattern kHookLayerShooter{
    .movement={.angle=0,.speed=11.25F},.initialAnimation=8,.deathEffect=0,.spatialShots=true,.routine=HookLayerShooter};
inline constexpr behavior::Pattern kSwerveLayerShooter{
    .movement={.angle=-1.04719758F,.speed=12.0535717F},.initialAnimation=8,.deathEffect=0,.spatialShots=true,.routine=SwerveLayerShooter};
inline constexpr behavior::Pattern kPassingHookCrystal{
    .movement={.angle=0,.speed=12.0535717F},.initialAnimation=8,.deathEffect=0,.spatialShots=true,.routine=PassingHookCrystal};
inline constexpr behavior::Pattern kPassingRisingCrystal{
    .movement={.angle=-1.04719758F,.speed=12.0535717F},.initialAnimation=8,.deathEffect=0,.spatialShots=true,.routine=PassingRisingCrystal};
inline constexpr behavior::Pattern kDescendingFanShooter{
    .movement={.speed=12.0535717F},.initialAnimation=8,.deathEffect=0,.spatialShots=true,.routine=DescendingFanShooter};
inline constexpr behavior::Pattern kCrossingFanShooter{
    .movement={.angle=0,.speed=12.0535717F},.initialAnimation=8,.deathEffect=0,.spatialShots=true,.routine=CrossingFanShooter};
inline constexpr behavior::Pattern kStrongSpiralCrystal{
    .movement={.speed=5.35714293F},.initialAnimation=10,.deathEffect=0,.spatialShots=true,.routine=StrongSpiralCrystal,.onDefeated=CrystalDefeated};
inline constexpr behavior::Pattern kGentleSpiralCrystal{
    .movement={.angle=0,.speed=5.35714293F},.initialAnimation=10,.deathEffect=0,.spatialShots=true,.routine=GentleSpiralCrystal,.onDefeated=CrystalDefeated};
inline constexpr behavior::Pattern kGreenRingEmitter{
    .movement={.speed=0},.initialAnimation=13,.deathEffect=0,.spatialShots=true,.routine=GreenRingEmitter,.onDefeated=EmitterDefeated};
inline constexpr behavior::Pattern kBlueRingEmitter{
    .movement={.speed=0},.initialAnimation=14,.deathEffect=0,.spatialShots=true,.routine=BlueRingEmitter,.onDefeated=EmitterDefeated};
inline constexpr behavior::Pattern kOffsetRingEmitter{
    .movement={.speed=0},.initialAnimation=13,.deathEffect=0,.spatialShots=true,.routine=OffsetRingEmitter,.onDefeated=EmitterDefeated};
inline constexpr behavior::Pattern kHiddenRandomEmitter{
    .movement={.speed=0},.initialAnimation=16,.deathEffect=0,.spatialShots=true,.routine=HiddenRandomEmitter};
inline constexpr behavior::Pattern kHiddenLaserEmitter{
    .movement={.speed=0},.initialAnimation=16,.deathEffect=0,.spatialShots=true,.routine=HiddenLaserEmitter};
/// @brief 配置本关弹幕素材与三维模式；false 仅供独立源参数对照，不消耗演员随机流。
void ConfigureBehavior(behavior::Actor& actor,bool spatial=true);
namespace koakuma {
/// @brief 小恶魔道中、击破与超时；原始 HP/超时由统一目录初始化，召唤独立发射体。
behavior::Routine Encounter(behavior::Actor& actor);
behavior::Routine Defeated(behavior::Actor& actor);
behavior::Routine Timeout(behavior::Actor& actor);
}
namespace patchouli {
/// @brief 帕秋莉入场等待独立开战通知；两个非符阶段保存自己的激光槽与跨子行为的自动发射。
behavior::Routine Entrance(behavior::Actor& actor);
behavior::Routine FirstNonspell(behavior::Actor& actor);
behavior::Routine SecondNonspell(behavior::Actor& actor);
/// @brief Normal 按机体选用的元素符卡及其强化版；均含 120 内容帧准备期，返回条件由 Boss 生命周期设置。
behavior::Routine AgniShine(behavior::Actor& actor);
behavior::Routine AgniShineAdvanced(behavior::Actor& actor);
behavior::Routine PrincessUndine(behavior::Actor& actor);
behavior::Routine SylphyHorn(behavior::Actor& actor);
behavior::Routine SylphyHornAdvanced(behavior::Actor& actor);
behavior::Routine LazyTrilithon(behavior::Actor& actor);
behavior::Routine LazyTrilithonAdvanced(behavior::Actor& actor);
behavior::Routine MetalFatigue(behavior::Actor& actor);
/// @brief 组合符卡的命名攻击，含自身准备期；组合阶段额外的演出等待由 CombinationEntrance 负责。
behavior::Routine LavaCromlech(behavior::Actor& actor);
behavior::Routine ForestBlaze(behavior::Actor& actor);
behavior::Routine WaterElf(behavior::Actor& actor);
behavior::Routine MercuryPoison(behavior::Actor& actor);
behavior::Routine EmeraldMegalith(behavior::Actor& actor);
behavior::Routine CombinationEntrance(behavior::Actor& actor);
/// @brief 组合段在包装层等待后重设自身超时并执行命名攻击；entry 必须为本关的组合符卡入口，timeout 为内容帧。
behavior::Routine Combination(behavior::Actor& actor,behavior::Entry entry,int timeout);
/// @brief 供统一阶段回调使用的交接标记；必须由 Boss 在 Update 外推进，不能在协程中销毁当前调用链。
behavior::Routine PhaseComplete(behavior::Actor& actor);
behavior::Routine Defeated(behavior::Actor& actor);
}
}
