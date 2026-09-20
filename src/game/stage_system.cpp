#include "game/stage_system.hpp"

#include <array>

namespace redmist {
namespace {

// Original timing and formations inspired by the pacing of an early-stage
// Normal route. No commercial game data is included.
constexpr std::array<StageEvent, 16> kNormalStageOneEvents{{
    {0.2F, StageEventKind::ShowMessage},
    {1.0F, StageEventKind::SpawnLine, EnemyKind::Fairy, 5,
     {0.0F, 0.0F, 8.8F}, 2.1F},
    {4.5F, StageEventKind::SpawnStarFormation, EnemyKind::Fairy, 7,
     {0.0F, 1.5F, 8.5F}, 2.2F},
    {8.0F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 6},
    {11.5F, StageEventKind::SpawnLine, EnemyKind::Orb, 5,
     {0.0F, 2.6F, 8.4F}, 1.8F},
    {15.5F, StageEventKind::SpawnStarFormation, EnemyKind::Fairy, 9,
     {0.0F, 0.0F, 8.7F}, 2.8F},
    {19.5F, StageEventKind::SpawnCrossingPair, EnemyKind::Orb, 8},
    {23.0F, StageEventKind::ShowMessage},
    {23.7F, StageEventKind::SpawnElite, EnemyKind::Elite, 1,
     {0.0F, 2.5F, 7.8F}},
    {28.5F, StageEventKind::SpawnLine, EnemyKind::Fairy, 7,
     {0.0F, -2.5F, 8.8F}, 2.0F},
    {31.5F, StageEventKind::SpawnStarFormation, EnemyKind::Orb, 7,
     {0.0F, 1.0F, 8.5F}, 3.0F},
    {34.5F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 8},
    {38.0F, StageEventKind::SpawnLine, EnemyKind::Orb, 5,
     {0.0F, 3.5F, 8.5F}, 2.4F},
    {41.0F, StageEventKind::ShowMessage},
    {42.0F, StageEventKind::StartBoss},
    {44.0F, StageEventKind::ShowMessage},
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
