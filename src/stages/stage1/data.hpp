// 本文件负责声明第一面时间轴、素材清单和对话数据。

#pragma once

#include "stage/content.hpp"

namespace redmist::stage1 {
/// @brief 返回第一面静态素材清单；引用在程序运行期间有效。
[[nodiscard]] const StageAssetManifest& Stage1Manifest();
/// @brief 返回第一面静态时间轴；事件按预期触发顺序排列，底层数据不转移所有权。
[[nodiscard]] std::span<const StageEvent> Stage1Timeline();
/// @brief 返回第一面静态对话；台词和说话人字符串在程序运行期间有效。
[[nodiscard]] std::span<const DialogueLine> Stage1Dialogue(bool afterBattle = false);
}  // namespace redmist::stage1
