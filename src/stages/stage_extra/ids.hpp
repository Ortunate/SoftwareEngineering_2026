// 本文件定义Extra同场素材槽，分离芙兰朵露、道中帕秋莉、立绘与背景；编号只属于本关清单。
#pragma once
#include "stages/common/ids.hpp"
namespace koumakyou::stage_extra {
using namespace stage_assets;
inline constexpr VisualId kFlandre=kBoss;
inline constexpr auto kFlandrePortraits=kBossPortraits;
inline constexpr VisualId kPatchouli=kCommonVisualCount;
inline constexpr std::array<VisualId,4> kPatchouliPortraits{kPatchouli+1,kPatchouli+2,kPatchouli+3,kPatchouli+4};
inline constexpr VisualId kBackground=kPatchouli+5;
}
