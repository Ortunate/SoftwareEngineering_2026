// 本文件为二面命名公共素材槽及独立的大妖精战斗槽；编号仅在二面清单内有效。
#pragma once
#include "stages/common/ids.hpp"
namespace koumakyou::stage2 {
using namespace stage_assets;
inline constexpr VisualId kCirno=kBoss;
// 琪露诺原包只有 face05a 的两个差分；face06 属于美铃，不能从关卡号推导人像文件编号。
inline constexpr std::array<VisualId,2> kCirnoPortraits{kBossPortraits[0],kBossPortraits[1]};
inline constexpr VisualId kDaiyousei=kCommonVisualCount;
inline constexpr std::size_t kVisualCount=kDaiyousei+1;
}
