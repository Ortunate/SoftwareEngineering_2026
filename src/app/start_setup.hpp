// 本文件定义 Start 选择流程的页面身份；编号仅供 UI 动画使用，不写入玩家配置。
#pragma once

namespace koumakyou {
/// @brief 正常开局依次经过难度、主人公、御札、模式、最终确认；符卡练习从 Spell 直接进入确认。
/// 仅 Ready 页允许真正加载关卡，返回确认页之前的步骤必须保留已选内容。
enum class StartSetupStep { Difficulty, Character, Shot, Mode, Ready, Spell };
} // namespace koumakyou
