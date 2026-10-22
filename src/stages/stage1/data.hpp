// 本文件负责声明第一面时间轴、素材清单和对话数据。

#pragma once

#include "stage/content.hpp"

namespace koumakyou::stage1 {
/// @brief 返回第一面静态素材清单；引用在程序运行期间有效。
[[nodiscard]] const StageAssetManifest& Stage1Manifest();
/// @brief 返回第一面静态时间轴；事件按预期触发顺序排列，底层数据不转移所有权。
[[nodiscard]] std::span<const StageEvent> Stage1Timeline();
/// @brief 返回第一面静态对话；台词和说话人字符串在程序运行期间有效。
/// @brief 返回指定角色战前/战后静态台词；marisa=false 选择灵梦，返回值借用本模块常量。
[[nodiscard]] std::span<const DialogueLine> Stage1Dialogue(bool afterBattle = false, bool marisa=false);
}  // namespace koumakyou::stage1
