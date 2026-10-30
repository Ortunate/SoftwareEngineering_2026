// 本文件声明第 4 面的命名 Boss 启动配置与事件入口；机体分支在启动前解析。
#pragma once
#include "gameplay/bosses/config.hpp"
namespace koumakyou::stage4 {
/// @brief KoakumaMidboss 的完整配方；shot 为四机体之一，返回独立配置，素材借用本关。
bosses::StartConfig KoakumaMidbossConfig(ShotType shot=ShotType::HomingAmulet);
/// @brief 道中本体的四难度配方；返回独立配置，整关入口是否开放由目录验收状态决定。
bosses::StartConfig KoakumaMidbossConfig(ShotType shot,native_combat::Difficulty difficulty);
/// @brief 静态战斗定义，可由 StartBoss 事件安全借用。
extern const bosses::Battle kKoakumaMidboss;
/// @brief PatchouliFinal 的完整配方；shot 为四机体之一，返回独立配置，素材借用本关。
bosses::StartConfig PatchouliFinalConfig(ShotType shot=ShotType::HomingAmulet);
/// @brief 四难度关底组合；Easy 省略第一元素符卡且只有一张组合，高难使用三张组合。
bosses::StartConfig PatchouliFinalConfig(ShotType shot,native_combat::Difficulty difficulty);
/// @brief 静态战斗定义，可由 StartBoss 事件安全借用。
extern const bosses::Battle kPatchouliFinal;
/// @brief 返回指定 1 起算符卡编号/难度的静态单技能练习遭遇；素材借用四面，未实现或难度不符抛 invalid_argument。
/// 练习复用角色命名攻击、准备与退场，展开编号恒为 1，不伪造原生 Spell Practice 指令路径。
const bosses::Battle& PatchouliSpellBattle(int spellCard,native_combat::Difficulty difficulty);
}
