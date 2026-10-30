// 本文件声明第 3 面的命名 Boss 启动配置与事件入口；机体分支在启动前解析。
#pragma once
#include "gameplay/bosses/config.hpp"
namespace koumakyou::stage3 {
/// @brief MeilingMidboss 的完整配方；shot 为四机体之一，返回独立配置，素材借用本关。
bosses::StartConfig MeilingMidbossConfig(ShotType shot=ShotType::HomingAmulet);
/// @brief 四难度道中组合；高难使用 Selaginella，非符超时仍直接退场，不自动进入符卡。
bosses::StartConfig MeilingMidbossConfig(ShotType shot,native_combat::Difficulty difficulty);
/// @brief 静态战斗定义，可由 StartBoss 事件安全借用。
extern const bosses::Battle kMeilingMidboss;
/// @brief MeilingFinal 的完整配方；shot 为四机体之一，返回独立配置，素材借用本关。
bosses::StartConfig MeilingFinalConfig(ShotType shot=ShotType::HomingAmulet);
/// @brief 四难度关底组合；Easy 省略台风，高难增加华想梦葛并以彩光乱舞替换彩雨。
bosses::StartConfig MeilingFinalConfig(ShotType shot,native_combat::Difficulty difficulty);
/// @brief 静态战斗定义，可由 StartBoss 事件安全借用。
extern const bosses::Battle kMeilingFinal;
}
