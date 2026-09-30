// 本文件负责定义第一面局部使用的外观、敌人、编队和事件编号。

#pragma once

#include "resources/visual_assets.hpp"

#include <cstdint>

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

/// @brief 传给 Stage1Content::CreateEnemy 的第一面局部敌人原型编号。
enum class EnemyType : std::uint16_t { Fairy, DarkFairy, Orb };
/// @brief 关卡编队形状；具体敌人内部决定如何沿该编队移动。
enum class Formation : std::uint16_t { Line, Star, CrossingPair };
/// @brief Stage1Content::HandleEvent 解释的第一面局部动作编号。
enum class Action : std::uint16_t {
    ShowMessage,
    SpawnLine,
    SpawnStarFormation,
    SpawnCrossingPair,
    StartMidboss,
    StartDialogue,
    StartFinalBoss,
};

}  // namespace redmist::stage1
