// 本文件声明各关共享的玩家、弹幕、道具与自机立绘定义；不加载或借用 GPU 资源。
#pragma once
#include "resources/visual_assets.hpp"
#include <vector>
namespace koumakyou::stage_assets {
/// @brief 返回可由本关追加演员/Boss/对话槽的独立清单；字符串均借用静态存储。
/// 调用方须补齐 kFairy/kDarkFairy/kOrb/kBoss 与 kBossPortraits 预留槽并按 id 排列，再交给加载器。
[[nodiscard]] std::vector<VisualDefinition> CommonBattleVisuals();
}
