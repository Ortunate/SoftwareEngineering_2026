// 本文件声明第 5 面的命名 Boss 启动配置与事件入口；机体分支在启动前解析。
#pragma once
#include "gameplay/bosses/config.hpp"
namespace koumakyou::stage5 {
/// @brief SakuyaMidboss 的完整配方；shot 为四机体之一，返回独立配置，素材借用本关。
bosses::StartConfig SakuyaMidbossConfig(ShotType shot=ShotType::HomingAmulet);
/// @brief 四难度道中组合；非符超时直接退场，击破710阈值进入相应Misdirection，不决定整关目录开放。
bosses::StartConfig SakuyaMidbossConfig(ShotType shot,native_combat::Difficulty difficulty);
/// @brief 静态战斗定义，可由 StartBoss 事件安全借用。
extern const bosses::Battle kSakuyaMidboss;
/// @brief SakuyaFinal 的完整配方；shot 为四机体之一，返回独立配置，素材借用本关。
bosses::StartConfig SakuyaFinalConfig(ShotType shot=ShotType::HomingAmulet);
/// @brief 四难度关底三组配方；Hard/Lunatic使用各组的替换符卡，返回独立配置，不决定正式入口开放。
bosses::StartConfig SakuyaFinalConfig(ShotType shot,native_combat::Difficulty difficulty);
/// @brief 静态战斗定义，可由 StartBoss 事件安全借用。
extern const bosses::Battle kSakuyaFinal;
/// @brief 返回85…100中与difficulty一致的单技能静态工厂；不匹配的编号/难度抛invalid_argument，素材仍借用本关。
const bosses::Battle& SakuyaSpellBattle(int spellCard,native_combat::Difficulty difficulty);
}
