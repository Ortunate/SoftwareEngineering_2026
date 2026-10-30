// 本文件定义一面道中敌人的常量行为配方，并声明露米娅阶段，复用带参移动与技能组件。
#pragma once
#include "gameplay/actors/behavior.hpp"
namespace koumakyou::stage1 {
/// @brief Boss 的保守高度范围；移动路程和时长仍由原编排决定，不使用全体积随机路点。
inline constexpr Vector2 kBossHeights{-1.25F,1.25F};
/// @brief 按难度复制并调整命名配方；配置只作用于出生实例，不改变共享常量。
void ConfigureTurningFairy(behavior::Pattern&,native_combat::Difficulty);
void ConfigureGentleTurningFairy(behavior::Pattern&,native_combat::Difficulty);
void ConfigureLateTurningFairy(behavior::Pattern&,native_combat::Difficulty);
void ConfigureShootingDarkFairy(behavior::Pattern&,native_combat::Difficulty);
void ConfigurePassingDarkFairy(behavior::Pattern&,native_combat::Difficulty);
// 以下配方的时间均为出生起算的 60 Hz 局部帧，速度为世界单位/秒，角速度为弧度/秒。
// 小怪恢复原 X/Z 路线，同一波仅整体平移到固定高度，保留队列间距与转弯形状。
// 配方无可变执行状态，可跨事件共享；演员在创建时复制配置并持有独立计时。

/// @brief 普通妖精的两段反向转弯路径，不发弹；实际横向由出生镜像标记决定。
inline constexpr behavior::Pattern kTurningFairy{
    .movement={.angle=1.5707964F,.speed=5.35714293F,.keys={{
        {.frame=40,.angularVelocity=-1.47262156F},
        {.frame=120,.angularVelocity=1.17809725F},
        {.frame=220,.angularVelocity=0.0F},
    }},.count=3},
    .spatialShots=true,
    .configureDifficulty=ConfigureTurningFairy,
};

/// @brief 第二次转弯角速度减半的普通妖精，其他时点沿用普通转弯路径。
inline constexpr behavior::Pattern kGentleTurningFairy{
    .movement={.angle=1.5707964F,.speed=5.35714293F,.keys={{
        {.frame=40,.angularVelocity=-1.47262156F},
        {.frame=120,.angularVelocity=1.17809725F/2.0F},
        {.frame=220,.angularVelocity=0.0F},
    }},.count=3},
    .spatialShots=true,
    .configureDifficulty=ConfigureGentleTurningFairy,
};

/// @brief 延后转弯的普通妖精，先直行再沿固定曲率转向，不发弹。
inline constexpr behavior::Pattern kLateTurningFairy{
    .movement={.angle=1.5707964F,.speed=5.35714293F,.keys={{
        {.frame=100,.angularVelocity=1.17809725F},
        {.frame=200,.angularVelocity=0.0F},
    }},.count=2},
    .spatialShots=true,
    .configureDifficulty=ConfigureLateTurningFairy,
};

/// @brief 停车后发射一次七向瞄准弹，再加速转弯离场；发弹偏移使用世界 XYZ。
inline constexpr behavior::Pattern kShootingDarkFairy{
    .movement={.angle=1.5707964F,.speed=5.35714293F,.keys={{
        {.frame=60,.speed=0.0F},
        {.frame=130,.angularVelocity=3.14159274F,.acceleration=8.03571415F},
        {.frame=190,.angularVelocity=0.0F},
    }},.count=3},
    .skill=skill::TimedShot{70,{skill::Formation::AimedFan,1,2,7,1,3.75F,0,0,0.62831855F,
                              skill::LaunchBoost|skill::SpawnSmall,21,1.1F}},
    .initialAnimation=3,.stoppedAnimation=5,.deathEffect=0,.muzzleOffset={0.535714269F,0,0.535714269F},
    .spatialShots=true,
    .configureDifficulty=ConfigureShootingDarkFairy,
};

/// @brief 沿停车、加速转弯路径通过战场的暗色妖精，无攻击配置。
inline constexpr behavior::Pattern kPassingDarkFairy{
    .movement={.angle=1.5707964F,.speed=5.35714293F,.keys={{
        {.frame=60,.speed=0.0F},
        {.frame=130,.angularVelocity=3.14159274F,.acceleration=8.03571415F},
        {.frame=190,.angularVelocity=0.0F},
    }},.count=3},
    .initialAnimation=3,.stoppedAnimation=5,.deathEffect=0,.muzzleOffset={0.535714269F,0,0.535714269F},
    .spatialShots=true,
    .configureDifficulty=ConfigurePassingDarkFairy,
};

/// @brief 配置一面素材与空间发弹模式；Boss 阶段按需启用等路程移动，Actor::Reset 后调用，不借用外部资源。
/// spatial=false 只关闭新增空间适配，用于独立旧行为差分；不改变更新频率、随机源或内容时钟。
void ConfigureBehavior(behavior::Actor& actor,bool spatial=true);
/// @brief 按本关难度解析暗妖精出生配方；复制输入，不更改静态时间轴。
/// Lunatic 转弯妖精以实例独占协程保留原移动，并开启独立周期发射计数。
behavior::Spawn ConfigureSpawn(const behavior::Spawn& spawn,native_combat::Difficulty difficulty);
} // namespace koumakyou::stage1
