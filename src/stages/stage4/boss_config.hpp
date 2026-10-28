// 本文件声明第 4 面的命名 Boss 启动配置与事件入口；机体分支在启动前解析。
#pragma once
#include "gameplay/bosses/config.hpp"
namespace koumakyou::stage4 {
/// @brief KoakumaMidboss 的完整配方；shot 为四机体之一，返回独立配置，素材借用本关。
bosses::StartConfig KoakumaMidbossConfig(ShotType shot=ShotType::HomingAmulet);
/// @brief 静态战斗定义，可由 StartBoss 事件安全借用。
extern const bosses::Battle kKoakumaMidboss;
/// @brief PatchouliFinal 的完整配方；shot 为四机体之一，返回独立配置，素材借用本关。
bosses::StartConfig PatchouliFinalConfig(ShotType shot=ShotType::HomingAmulet);
/// @brief 静态战斗定义，可由 StartBoss 事件安全借用。
extern const bosses::Battle kPatchouliFinal;
}
