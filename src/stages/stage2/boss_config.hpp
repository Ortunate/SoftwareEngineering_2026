// 本文件声明第 2 面的命名 Boss 启动配置与事件入口；机体分支在启动前解析。
#pragma once
#include "gameplay/bosses/config.hpp"
namespace koumakyou::stage2 {
/// @brief DaiyouseiMidboss 的完整配方；shot 为四机体之一，返回独立配置，素材借用本关。
bosses::StartConfig DaiyouseiMidbossConfig(ShotType shot=ShotType::HomingAmulet);
/// @brief 静态战斗定义，可由 StartBoss 事件安全借用。
extern const bosses::Battle kDaiyouseiMidboss;
/// @brief CirnoFinal 的完整配方；shot 为四机体之一，返回独立配置，素材借用本关。
bosses::StartConfig CirnoFinalConfig(ShotType shot=ShotType::HomingAmulet);
/// @brief 按难度解析技能组合；Easy 的第二组以 Perfect Freeze 结束。
bosses::StartConfig CirnoFinalConfig(ShotType shot,native_combat::Difficulty difficulty);
/// @brief 静态战斗定义，可由 StartBoss 事件安全借用。
extern const bosses::Battle kCirnoFinal;
}
