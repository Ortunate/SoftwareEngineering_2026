// 本文件定义一面道中敌人的常量行为配方，并声明露米娅阶段，复用带参移动与技能组件。
#pragma once
#include "gameplay/actors/behavior.hpp"
namespace koumakyou::stage1 {
// 以下配方的时间均为出生起算的 60 Hz 局部帧，速度为像素/帧，角速度为弧度/帧。
// 配方无可变执行状态，可跨事件共享；演员在创建时复制配置并持有独立计时。

/// @brief 普通妖精的两段反向转弯路径，不发弹；实际横向由出生镜像标记决定。
inline constexpr behavior::Pattern kTurningFairy{
    .movement={.angle=1.5707964F,.speed=2,.keys={{
        {.frame=40,.angularVelocity=-0.024543693F},
        {.frame=120,.angularVelocity=0.019634955F},
        {.frame=220,.angularVelocity=0},
    }},.count=3},
};

/// @brief 第二次转弯角速度减半的普通妖精，其他时点沿用普通转弯路径。
inline constexpr behavior::Pattern kGentleTurningFairy{
    .movement={.angle=1.5707964F,.speed=2,.keys={{
        {.frame=40,.angularVelocity=-0.024543693F},
        {.frame=120,.angularVelocity=0.019634955F/2.0F},
        {.frame=220,.angularVelocity=0},
    }},.count=3},
};

/// @brief 延后转弯的普通妖精，先直行再沿固定曲率转向，不发弹。
inline constexpr behavior::Pattern kLateTurningFairy{
    .movement={.angle=1.5707964F,.speed=2,.keys={{
        {.frame=100,.angularVelocity=0.019634955F},
        {.frame=200,.angularVelocity=0},
    }},.count=2},
};

/// @brief 停车后发射一次七向瞄准弹，再加速转弯离场；发弹偏移使用逻辑像素。
inline constexpr behavior::Pattern kShootingDarkFairy{
    .movement={.angle=1.5707964F,.speed=2,.keys={{
        {.frame=60,.speed=0},
        {.frame=130,.angularVelocity=0.05235988F,.acceleration=0.05F},
        {.frame=190,.angularVelocity=0},
    }},.count=3},
    .skill=skill::TimedShot{70,{skill::Formation::AimedFan,1,2,7,1,1.4F,0,0,0.62831855F,
                              skill::LaunchBoost|skill::SpawnSmall}},
    .initialAnimation=3,.stoppedAnimation=5,.deathEffect=0,.muzzleOffset={12,-12,0},
};

/// @brief 沿停车、加速转弯路径通过战场的暗色妖精，无攻击配置。
inline constexpr behavior::Pattern kPassingDarkFairy{
    .movement={.angle=1.5707964F,.speed=2,.keys={{
        {.frame=60,.speed=0},
        {.frame=130,.angularVelocity=0.05235988F,.acceleration=0.05F},
        {.frame=190,.angularVelocity=0},
    }},.count=3},
    .initialAnimation=3,.stoppedAnimation=5,.deathEffect=0,.muzzleOffset={12,-12,0},
};

/// @brief 配置一面弹幕素材编号；Actor::Reset 后调用，不借用外部资源。
void ConfigureBehavior(behavior::Actor& actor);
namespace rumia {
using behavior::Actor;
using behavior::Routine;
/// @brief 以下入口接管各阶段的移动、攻击、阈值及退场；HP/超时使用原始目录，角为弧度、时间为逻辑帧。
Routine Midboss(Actor& actor);
Routine MidbossDefeated(Actor& actor);
Routine MidbossTimeout(Actor& actor);
Routine FirstNonspell(Actor& actor);
Routine SecondNonspell(Actor& actor);
Routine NightBird(Actor& actor);
Routine Demarcation(Actor& actor);
Routine Defeated(Actor& actor);
Routine End(Actor& actor);
} // namespace rumia
} // namespace koumakyou::stage1
