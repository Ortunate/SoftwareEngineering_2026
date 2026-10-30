// 本文件声明Extra含条件后继的完整显式时间轴；正式入口另经背景与目录验收后开放。
#pragma once
#include "stage/system.hpp"
namespace koumakyou::stage_extra {
/// @brief 借用353条静态事件，包含338次普通出生及道中/主战/追加战的实际退场与对白放行依赖。
/// 主战战后对白由内容根据本局成绩短路或进入关卡出口；只有短路路径继续追加战，不以预估时长代替等待。
[[nodiscard]] std::span<const StageEvent> ExtraMainTimeline();
}
