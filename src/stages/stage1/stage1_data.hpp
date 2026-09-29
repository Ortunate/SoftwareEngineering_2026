// 本文件负责声明第一面时间轴、素材清单和对话数据。

#pragma once

#include "stage/stage_content.hpp"

namespace redmist::stage1 {
[[nodiscard]] const StageAssetManifest& Stage1Manifest();
[[nodiscard]] std::span<const StageEvent> Stage1Timeline();
[[nodiscard]] std::span<const DialogueLine> Stage1Dialogue();
}  // namespace redmist::stage1
