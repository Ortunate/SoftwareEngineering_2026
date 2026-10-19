// 本文件定义 Start 选择流程的页面身份；编号仅供 UI 动画使用，不写入玩家配置。
#pragma once

namespace koumakyou {
/// @brief 正常开局经过难度、主人公、御札、模式、最终确认；整关练习在模式后增加 Stage，符卡练习使用 Spell 的独立三级目录。
/// 普通开局与整关练习仅 Ready 允许加载；符卡练习在难度确认后直接加载，返回保留已选内容。
enum class StartSetupStep { Difficulty, Character, Shot, Mode, Ready, Spell, Stage, Count };
} // namespace koumakyou
