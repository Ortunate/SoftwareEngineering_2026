#include "game/stage_system.hpp"

#include <array>

namespace redmist {
namespace {

// Approximate timings from the supplied Normal playthrough. These are
// editable stage data; frame-exact values still need a local video capture.
constexpr std::array<StageEvent, 18> kNormalStageOneEvents{{
    {0.2F, StageEventKind::ShowMessage},
    {1.1F, StageEventKind::SpawnLine, EnemyKind::Fairy, 4,
     {-3.0F, 1.0F, 8.8F}, 1.25F},
    {3.4F, StageEventKind::SpawnLine, EnemyKind::Fairy, 4,
     {3.0F, -0.8F, 8.8F}, 1.25F},
    {5.8F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 6},
    {8.2F, StageEventKind::SpawnLine, EnemyKind::Fairy, 5,
     {0.0F, 2.3F, 8.7F}, 1.4F},
    {10.6F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 6},
    {13.0F, StageEventKind::SpawnLine, EnemyKind::Fairy, 5,
     {0.0F, -1.8F, 8.7F}, 1.5F},
    {15.6F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 8},
    {18.2F, StageEventKind::SpawnLine, EnemyKind::Orb, 3,
     {0.0F, 2.2F, 8.5F}, 2.3F},
    {20.3F, StageEventKind::SpawnLine, EnemyKind::Fairy, 6,
     {0.0F, -1.0F, 8.6F}, 1.35F},
    {23.0F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 8},
    {26.0F, StageEventKind::SpawnElite, EnemyKind::Elite, 1,
     {0.0F, 2.3F, 7.8F}},
    {30.0F, StageEventKind::SpawnLine, EnemyKind::Fairy, 6,
     {0.0F, 0.0F, 8.8F}, 1.7F},
    {33.0F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 8},
    {36.0F, StageEventKind::SpawnLine, EnemyKind::Fairy, 5,
     {0.0F, 2.2F, 8.6F}, 1.8F},
    {39.0F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 6},
    {41.0F, StageEventKind::ShowMessage},
    {42.0F, StageEventKind::StartBoss},
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
