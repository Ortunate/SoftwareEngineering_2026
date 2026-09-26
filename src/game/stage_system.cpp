#include "game/stage_system.hpp"

#include <array>

namespace redmist {
namespace {

// Approximate gameplay times read from the local 187-second Normal capture.
// The first seconds contain character selection, so event times are relative
// to the start of play rather than the video timecode.
constexpr std::array<StageEvent, 21> kNormalStageOneEvents{{
    {0.2F, StageEventKind::ShowMessage},
    {1.0F, StageEventKind::SpawnLine, EnemyKind::Fairy, 8,
     {0.0F, 1.2F, 8.8F}, 1.15F},
    {4.5F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 8},
    {8.0F, StageEventKind::SpawnLine, EnemyKind::DarkFairy, 6,
     {0.0F, 2.0F, 8.8F}, 1.5F},
    {11.5F, StageEventKind::SpawnCrossingPair, EnemyKind::DarkFairy, 6},
    {15.0F, StageEventKind::SpawnLine, EnemyKind::Fairy, 8,
     {0.0F, -0.5F, 8.8F}, 1.2F},
    {18.5F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 8},
    {22.0F, StageEventKind::SpawnLine, EnemyKind::Fairy, 10,
     {0.0F, 1.5F, 8.8F}, 1.0F},
    {26.0F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 10},
    {30.0F, StageEventKind::StartMidboss},
    {55.0F, StageEventKind::SpawnLine, EnemyKind::DarkFairy, 6,
     {0.0F, 1.2F, 8.7F}, 1.5F},
    {59.0F, StageEventKind::SpawnCrossingPair, EnemyKind::DarkFairy, 8},
    {63.0F, StageEventKind::SpawnLine, EnemyKind::DarkFairy, 7,
     {0.0F, -0.8F, 8.8F}, 1.25F},
    {67.0F, StageEventKind::SpawnCrossingPair, EnemyKind::DarkFairy, 8},
    {72.0F, StageEventKind::SpawnLine, EnemyKind::Fairy, 10,
     {0.0F, 1.2F, 8.8F}, 1.0F},
    {76.0F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 10},
    {80.0F, StageEventKind::SpawnLine, EnemyKind::Fairy, 10,
     {0.0F, -1.0F, 8.8F}, 1.0F},
    {84.0F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 10},
    {88.0F, StageEventKind::SpawnLine, EnemyKind::Fairy, 8,
     {0.0F, 1.2F, 8.8F}, 1.2F},
    {92.0F, StageEventKind::StartDialogue},
    {107.0F, StageEventKind::StartFinalBoss},
}};

}  // namespace

void StageDirector::Reset() {
    elapsed_ = 0.0F;
    nextEvent_ = 0;
}

void StageDirector::Update(float deltaTime, const EventHandler& handler) {
    elapsed_ += deltaTime;
    while (nextEvent_ < kNormalStageOneEvents.size() &&
           kNormalStageOneEvents[nextEvent_].time <= elapsed_) {
        handler(kNormalStageOneEvents[nextEvent_]);
        ++nextEvent_;
    }
}

}  // namespace redmist
