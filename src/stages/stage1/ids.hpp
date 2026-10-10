// 本文件负责定义第一面局部使用的外观编号。

#pragma once

#include "resources/visual_assets.hpp"

namespace redmist::stage1 {

// 编号仅在 Stage 1 内有效；更换 Stage 不要求保留相同含义。
constexpr VisualId kBlueBullet = 0;
constexpr VisualId kRedBullet = 1;
constexpr VisualId kGreenSphere = 2;
constexpr VisualId kFairy = 3;
constexpr VisualId kDarkFairy = 4;
constexpr VisualId kOrb = 5;
constexpr VisualId kRumia = 6;
constexpr VisualId kReimuPortrait = 7;
/// @brief 玩家直射符卡与追踪符卡使用独立的关卡外观编号。
constexpr VisualId kPlayerGoldCard = 8;
constexpr VisualId kPlayerCyanCard = 9;
/// @brief Power 道具的公告板贴图，仅供第一面素材清单和场景绘制使用。
constexpr VisualId kPowerPoint = 10;
/// @brief 分数道具的公告板贴图编号。
constexpr VisualId kScorePoint = 11;
/// @brief 满火力清弹产生的星点公告板贴图编号。
constexpr VisualId kStarPoint = 12;

}  // namespace redmist::stage1
