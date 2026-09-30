// 本文件负责声明关卡编号和关卡内容工厂接口。

#pragma once

#include "stage/content.hpp"

#include <cstdint>
#include <memory>

namespace redmist {

/// @brief 可选择的关卡内容编号；注册新关卡只修改目录模块。
enum class StageId : std::uint8_t { Stage1 };

/// @brief 构造指定关卡的内容对象。
/// @return 调用方独占的关卡内容。
/// @throws std::invalid_argument 未注册的关卡编号。
[[nodiscard]] std::unique_ptr<StageContent> CreateStageContent(StageId id);

}  // namespace redmist
