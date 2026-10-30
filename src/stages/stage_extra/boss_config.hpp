// 本文件声明Extra道中、十阶段主战、条件后继和119…131单符练习的命名战斗配置。
#pragma once
#include "gameplay/bosses/config.hpp"
namespace koumakyou::stage_extra {
/// @brief 帕秋莉三符完整配方，入场等待对白放行；难度只接受Normal作为Extra单套内部上下文。
/// 返回独立配置，本体使用本关ids.hpp的kPatchouli槽并绑定ExtraEnemies，另需公共弹幕；非法机体/难度抛invalid_argument。
bosses::StartConfig PatchouliMidbossConfig(ShotType shot=ShotType::HomingAmulet,
    native_combat::Difficulty difficulty=native_combat::Difficulty::Normal);
/// @brief 芙兰朵露十阶段主战；入场等待ResumeCombat，保留非符阈值/归零的不同后继。
/// 素材须提供Flandre本体、ExtraEnemies辅助光圈和公共弹幕；仅接受单套Normal内部上下文。
bosses::StartConfig FlandreMainConfig(ShotType shot=ShotType::HomingAmulet,
    native_combat::Difficulty difficulty=native_combat::Difficulty::Normal);
/// @brief 条件后继三符完整配置；只定义遭遇，是否触发由真实MSG流程决定，不能无条件拼到主战后。
bosses::StartConfig FlandreEncoreConfig(ShotType shot=ShotType::HomingAmulet,
    native_combat::Difficulty difficulty=native_combat::Difficulty::Normal);
extern const bosses::Battle kFlandreMain,kFlandreEncore;
/// @brief 返回119…131的静态单符工厂；非法编号/非Normal内部上下文抛invalid_argument，132…134不属于练习目录。
/// 调用方借用返回值至程序结束；工厂创建独占角色，使用正式Extra素材，保留角色准备、血量、时限和退场。
const bosses::Battle& ExtraSpellBattle(int spellCard,native_combat::Difficulty difficulty=native_combat::Difficulty::Normal);
/// @brief 时间轴可安全借用至程序结束的道中静态工厂。
extern const bosses::Battle kPatchouliMidboss;
}
