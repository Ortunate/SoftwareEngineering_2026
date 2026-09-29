// 本文件负责第一面的出怪、道中 Boss、对话与关底 Boss 时间轴。

#include "stages/stage1/stage1_data.hpp"
#include "stages/stage1/stage1_ids.hpp"

#include <array>

namespace redmist::stage1 {
namespace {
constexpr std::uint16_t A(Action action) { return static_cast<std::uint16_t>(action); }
constexpr std::uint16_t E(EnemyType enemy) { return static_cast<std::uint16_t>(enemy); }
constexpr StageEventId kMidbossEventId = 1;
constexpr StageEventId kDialogueEventId = 2;
constexpr StageEventId kFinalBossEventId = 3;

// 普通波次相对上一事件启动；道中战及对话后的事件等待其真实结束。
constexpr std::array<StageEvent, 21> kEvents{{
    {0.2F, A(Action::ShowMessage)},
    {0.8F, A(Action::SpawnLine), E(EnemyType::Fairy), 8,
     {0.0F, 1.2F, 8.8F}, 1.15F},
    {3.5F, A(Action::SpawnCrossingPair), E(EnemyType::Fairy), 8},
    {3.5F, A(Action::SpawnLine), E(EnemyType::DarkFairy), 6,
     {0.0F, 2.0F, 8.8F}, 1.5F},
    {3.5F, A(Action::SpawnCrossingPair), E(EnemyType::DarkFairy), 6},
    {3.5F, A(Action::SpawnLine), E(EnemyType::Fairy), 8,
     {0.0F, -0.5F, 8.8F}, 1.2F},
    {3.5F, A(Action::SpawnCrossingPair), E(EnemyType::Fairy), 8},
    {3.5F, A(Action::SpawnLine), E(EnemyType::Fairy), 10,
     {0.0F, 1.5F, 8.8F}, 1.0F},
    {4.0F, A(Action::SpawnCrossingPair), E(EnemyType::Fairy), 10},
    {.time = 4.0F, .action = A(Action::StartMidboss), .id = kMidbossEventId},
    {.time = 3.0F, .action = A(Action::SpawnLine),
     .archetype = E(EnemyType::DarkFairy), .count = 6,
     .center = {0.0F, 1.2F, 8.7F}, .spacing = 1.5F,
     .afterEndOf = kMidbossEventId},
    {4.0F, A(Action::SpawnCrossingPair), E(EnemyType::DarkFairy), 8},
    {4.0F, A(Action::SpawnLine), E(EnemyType::DarkFairy), 7,
     {0.0F, -0.8F, 8.8F}, 1.25F},
    {4.0F, A(Action::SpawnCrossingPair), E(EnemyType::DarkFairy), 8},
    {5.0F, A(Action::SpawnLine), E(EnemyType::Fairy), 10,
     {0.0F, 1.2F, 8.8F}, 1.0F},
    {4.0F, A(Action::SpawnCrossingPair), E(EnemyType::Fairy), 10},
    {4.0F, A(Action::SpawnLine), E(EnemyType::Fairy), 10,
     {0.0F, -1.0F, 8.8F}, 1.0F},
    {4.0F, A(Action::SpawnCrossingPair), E(EnemyType::Fairy), 10},
    {4.0F, A(Action::SpawnLine), E(EnemyType::Fairy), 8,
     {0.0F, 1.2F, 8.8F}, 1.2F},
    {.time = 4.0F, .action = A(Action::StartDialogue), .id = kDialogueEventId},
    {.time = 0.0F, .action = A(Action::StartFinalBoss),
     .id = kFinalBossEventId, .afterEndOf = kDialogueEventId},
}};

 }  // namespace

std::span<const StageEvent> Stage1Timeline() { return kEvents; }
}  // namespace redmist::stage1
