#pragma once

#include "game/visual_assets.hpp"

#include <cstdint>

namespace redmist::stage1 {

// 编号仅在 Stage 1 内有效；更换 Stage 不要求保留相同含义。
constexpr VisualId kBlueBullet = 0;
constexpr VisualId kRedBullet = 1;
constexpr VisualId kGreenSphere = 2;
constexpr VisualId kFairy = 3;
constexpr VisualId kDarkFairy = 4;
constexpr VisualId kOrb = 5;
constexpr VisualId kElite = 6;
constexpr VisualId kRumia = 7;
constexpr VisualId kReimuPortrait = 8;

enum class EnemyType : std::uint16_t { Fairy, DarkFairy, Orb, Elite };
enum class Path : std::uint16_t { Straight, Helix, SweepLeft, SweepRight };
enum class Action : std::uint16_t {
    ShowMessage,
    SpawnLine,
    SpawnStarFormation,
    SpawnCrossingPair,
    SpawnElite,
    StartMidboss,
    StartDialogue,
    StartFinalBoss,
};

}  // namespace redmist::stage1
