// 本文件声明第 6 面的命名 Boss 启动配置与事件入口；机体分支在启动前解析。
#pragma once
#include "gameplay/bosses/config.hpp"
namespace koumakyou::stage6 {
/// @brief SakuyaMidboss 的完整配方；shot 为四机体之一，difficulty 仅 N/H/L，Easy 本篇在五面结束。
/// 返回独立配置，素材借用本关；非法难度或机体抛 invalid_argument。
bosses::StartConfig SakuyaMidbossConfig(ShotType shot=ShotType::HomingAmulet,
    native_combat::Difficulty difficulty=native_combat::Difficulty::Normal);
/// @brief 静态战斗定义，可由 StartBoss 事件安全借用。
extern const bosses::Battle kSakuyaMidboss;
/// @brief RemiliaFinal九技能配方，按N/H/L选择五组符卡，Easy及非法机体抛invalid_argument。
/// 返回独立配置，素材借用本关；生命/时限由角色绑定，血条明确保留源分段比例。
bosses::StartConfig RemiliaFinalConfig(ShotType shot=ShotType::HomingAmulet,
    native_combat::Difficulty difficulty=native_combat::Difficulty::Normal);
/// @brief 静态战斗定义，可由 StartBoss 事件安全借用。
extern const bosses::Battle kRemiliaFinal;
/// @brief 返回101…118号单符卡的静态战斗定义，借用期覆盖整个练习；卡号与N/H/L不匹配时抛invalid_argument。
const bosses::Battle& Stage6SpellBattle(int spellCard,native_combat::Difficulty difficulty);
}
