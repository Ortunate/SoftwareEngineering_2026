// 本文件声明第 5 面的命名 Boss 启动配置与事件入口；机体分支在启动前解析。
#pragma once
#include "gameplay/bosses/config.hpp"
namespace koumakyou::stage5 {
/// @brief SakuyaMidboss 的完整配方；shot 为四机体之一，返回独立配置，素材借用本关。
bosses::StartConfig SakuyaMidbossConfig(ShotType shot=ShotType::HomingAmulet);
/// @brief 静态战斗定义，可由 StartBoss 事件安全借用。
extern const bosses::Battle kSakuyaMidboss;
/// @brief SakuyaFinal 的完整配方；shot 为四机体之一，返回独立配置，素材借用本关。
bosses::StartConfig SakuyaFinalConfig(ShotType shot=ShotType::HomingAmulet);
/// @brief 静态战斗定义，可由 StartBoss 事件安全借用。
extern const bosses::Battle kSakuyaFinal;
}
