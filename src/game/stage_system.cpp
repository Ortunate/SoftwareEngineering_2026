#include "game/stage_system.hpp"

#include <array>

namespace redmist {
namespace {

// 时间以开始战斗为零点，参考 187 秒 Normal 录像近似编排；录像开头的选人时间不计入。
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
    // 一次更新可能跨越多个事件时间点，按表中顺序补发所有到期事件。
    while (nextEvent_ < kNormalStageOneEvents.size() &&
           kNormalStageOneEvents[nextEvent_].time <= elapsed_) {
        handler(kNormalStageOneEvents[nextEvent_]);
        ++nextEvent_;
    }
}

}  // namespace redmist
