// 本文件声明五面普通晶体的命名行为和静态配方；生产不按原子程序编号选择行为。
#pragma once
#include "gameplay/actors/behavior.hpp"
namespace koumakyou::stage5 {
// 以下协程只修改传入的独占 Actor；等待采用 60 Hz 内容帧，坐标/速度使用世界单位。
// 调用方须通过 Actor::Start/Call 持有协程，不能在其挂起期间销毁演员或借用的弹池。
/// @brief 停顿后逐轮提高弹速的蓝环发射体，两种轮数分别保留自己的击破掉落。
behavior::Routine AcceleratingBlueRings(behavior::Actor& actor);
behavior::Routine ShortBlueRings(behavior::Actor& actor);
/// @brief 双段变速环按蓝/红/绿外观分别命名，颜色不从路径推导。
behavior::Routine BlueLayeredRings(behavior::Actor& actor);
behavior::Routine RedLayeredRings(behavior::Actor& actor);
behavior::Routine GreenLayeredRings(behavior::Actor& actor);
behavior::Routine RandomBlueFans(behavior::Actor& actor);
/// @brief 横穿队列在出生时读取一次整数初相，持续配方按当时 rank 快照，不因后续 rank 改变而重算。
behavior::Routine PassingRandomCrystals(behavior::Actor& actor);
behavior::Routine PassingBlueFans(behavior::Actor& actor);
behavior::Routine PassingRedFans(behavior::Actor& actor);
behavior::Routine PassingRandomRedRings(behavior::Actor& actor);
behavior::Routine DropFour(behavior::Actor& actor);
behavior::Routine DropSix(behavior::Actor& actor);
inline constexpr behavior::Pattern kAcceleratingBlueRings{
    .movement={.speed=5.35714293F},.initialAnimation=12,.deathEffect=0,.spatialShots=true,.routine=AcceleratingBlueRings};
inline constexpr behavior::Pattern kShortBlueRings{
    .movement={.speed=5.35714293F},.initialAnimation=12,.deathEffect=0,.spatialShots=true,.routine=ShortBlueRings,.onDefeated=DropFour};
inline constexpr behavior::Pattern kBlueLayeredRings{
    .movement={.speed=5.35714293F},.initialAnimation=12,.deathEffect=0,.spatialShots=true,.routine=BlueLayeredRings,.onDefeated=DropSix};
inline constexpr behavior::Pattern kRedLayeredRings{
    .movement={.speed=5.35714293F},.initialAnimation=12,.deathEffect=0,.spatialShots=true,.routine=RedLayeredRings,.onDefeated=DropSix};
inline constexpr behavior::Pattern kGreenLayeredRings{
    .movement={.speed=5.35714293F},.initialAnimation=12,.deathEffect=0,.spatialShots=true,.routine=GreenLayeredRings,.onDefeated=DropSix};
inline constexpr behavior::Pattern kRandomBlueFans{
    .movement={.speed=5.35714293F},.initialAnimation=11,.deathEffect=0,.spatialShots=true,.routine=RandomBlueFans};
inline constexpr behavior::Pattern kPassingRandomCrystals{
    .movement={.angle=0,.speed=6.69642878F},.initialAnimation=10,.deathEffect=0,.spatialShots=true,.routine=PassingRandomCrystals};
inline constexpr behavior::Pattern kPassingBlueFans{
    .movement={.angle=0,.speed=6.69642878F},.initialAnimation=10,.deathEffect=0,.spatialShots=true,.routine=PassingBlueFans};
inline constexpr behavior::Pattern kPassingRedFans{
    .movement={.angle=0,.speed=6.69642878F},.initialAnimation=11,.deathEffect=0,.spatialShots=true,.routine=PassingRedFans};
inline constexpr behavior::Pattern kPassingRandomRedRings{
    .movement={.angle=0,.speed=6.69642878F},.initialAnimation=12,.deathEffect=0,.spatialShots=true,.routine=PassingRandomRedRings};
/// @brief 给独占 Boss 演员安装当前关卡的弹幕素材；空间模式不改变内容时钟或随机读取顺序。
void ConfigureBehavior(behavior::Actor& actor,bool spatial);
namespace sakuya {
/// @brief 入场只移动和等待；对白完成由 Boss 接收端切入 Midboss 或 FirstNonspell。
behavior::Routine Entrance(behavior::Actor& actor);
behavior::Routine Midboss(behavior::Actor& actor);
behavior::Routine Misdirection(behavior::Actor& actor);
behavior::Routine MidbossRetreat(behavior::Actor& actor);
behavior::Routine MidbossDefeated(behavior::Actor& actor);
behavior::Routine FirstNonspell(behavior::Actor& actor);
behavior::Routine ClockCorpse(behavior::Actor& actor);
behavior::Routine SecondNonspell(behavior::Actor& actor);
behavior::Routine LunaClock(behavior::Actor& actor);
behavior::Routine ThirdNonspell(behavior::Actor& actor);
behavior::Routine Marionette(behavior::Actor& actor);
behavior::Routine Defeated(behavior::Actor& actor);
/// @brief 阶段回调只标记交接，实际生命、血条和完整退场由 Stage5Boss 统一推进。
behavior::Routine PhaseComplete(behavior::Actor& actor);
/// @brief 原生时停弧线与改向的世界坐标实现；只在 Actor 的 Update 内调用，使用其借用弹池。
/// 弧线每次九枚、pulse 为从零起算的九帧周期编号；改向最多成功修改十四枚，保留槽位和随机读取顺序。
void EmitClockArc(behavior::Actor& actor,int pulse);
void RedirectStoppedBullets(behavior::Actor& actor);
}
}
