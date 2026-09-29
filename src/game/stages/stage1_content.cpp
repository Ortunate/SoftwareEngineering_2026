// 本文件负责定义第一面的素材清单、出怪时间轴、对话及事件处理。

#include "game/stages/stage1_content.hpp"

#include "game/stages/stage1_boss.hpp"
#include "game/stages/stage1_enemies.hpp"

#include <array>
#include <cmath>
#include <stdexcept>

namespace redmist::stage1 {
namespace {
constexpr std::uint16_t A(Action action) { return static_cast<std::uint16_t>(action); }
constexpr std::uint16_t E(EnemyType enemy) { return static_cast<std::uint16_t>(enemy); }
constexpr std::uint16_t F(Formation formation) {
    return static_cast<std::uint16_t>(formation);
}

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

constexpr std::array<VisualDefinition, 8> kVisuals{{
    {kBlueBullet, "bullet_pearl_runtime.png", VisualMode::Bullet, 1.0F,
     {0.5F, 0.5F}, 2.6F, true},
    {kRedBullet, "bullet_red_runtime.png", VisualMode::Bullet, 1.0F,
     {0.5F, 0.5F}, 2.6F, true},
    {kGreenSphere, {}, VisualMode::Sphere},
    {kFairy, "fairy_modern.png", VisualMode::Character, 1.8F,
     {0.51F, 0.40F}},
    {kDarkFairy, "dark_fairy_modern.png", VisualMode::Character, 1.8F,
     {0.50F, 0.45F}},
    {kOrb, "bullet_pearl_runtime.png", VisualMode::Character, 1.488F},
    {kRumia, "rumia_modern.png", VisualMode::Character, 3.2F,
     {0.50F, 0.36F}},
    {kReimuPortrait, "reimu_modern.png", VisualMode::Character},
}};
constexpr std::array<SoundDefinition, 0> kSounds{};
const StageAssetManifest kAssets{
    kVisuals, "audio/bgm/宛如鬼灯的鲜红之魂.mp3",
    "audio/bgm/妖魔夜行.mp3", kSounds};
constexpr std::array<DialogueLine, 6> kDialogue{{
    {"REIMU", "The mist is getting thicker...", false},
    {"REIMU", "Someone is hiding in the dark.", false},
    {"RUMIA", "You can see me from there?", true},
    {"REIMU", "Are you behind this strange night?", false},
    {"RUMIA", "Maybe. I like the darkness.", true},
    {"REIMU", "Then I'll clear a path through it!", false},
}};
}  // namespace

const StageAssetManifest& Stage1Content::Assets() const { return kAssets; }
std::span<const StageEvent> Stage1Content::Events() const { return kEvents; }
std::span<const DialogueLine> Stage1Content::Dialogue() const { return kDialogue; }

std::unique_ptr<Enemy> Stage1Content::CreateEnemy(
    std::uint16_t archetype, const EnemySpawn& spawn) const {
    if (spawn.formation > F(Formation::CrossingPair)) return nullptr;
    switch (static_cast<EnemyType>(archetype)) {
        case EnemyType::Fairy: return std::make_unique<FairyEnemy>(spawn);
        case EnemyType::DarkFairy: return std::make_unique<DarkFairyEnemy>(spawn);
        case EnemyType::Orb: return std::make_unique<OrbEnemy>(spawn);
    }
    return nullptr;
}

std::unique_ptr<Boss> Stage1Content::CreateBoss(bool final) const {
    auto boss = std::make_unique<RumiaBoss>();
    if (final) boss->StartFinalBoss();
    else boss->StartMidboss();
    return boss;
}

bool Stage1Content::HandleEvent(const StageEvent& event, StageEventSink& sink) const {
    const Action action = static_cast<Action>(event.action);
    if ((action == Action::StartMidboss || action == Action::StartDialogue ||
         action == Action::StartFinalBoss) && !event.id) {
        throw std::invalid_argument("Stage 1 asynchronous event requires an id");
    }
    switch (action) {
        case Action::SpawnLine: {
            const float middle = static_cast<float>(event.count - 1) * 0.5F;
            for (int index = 0; index < event.count; ++index) {
                const float offset = static_cast<float>(index) - middle;
                sink.SpawnStageEnemy(event.archetype,
                    {{event.center.x + offset * event.spacing,
                      event.center.y + std::sin(offset) * 0.8F,
                      event.center.z},
                     static_cast<float>(index) * 0.1F,
                     F(Formation::Line), index});
            }
            break;
        }
        case Action::SpawnStarFormation: {
            constexpr float goldenAngle = 2.39996322973F;
            for (int index = 0; index < event.count; ++index) {
                const float t = (static_cast<float>(index) + 0.5F) /
                                static_cast<float>(event.count);
                const float y = 1.0F - 2.0F * t;
                const float radial = std::sqrt(std::max(0.0F, 1.0F - y * y));
                const float azimuth = goldenAngle * static_cast<float>(index);
                sink.SpawnStageEnemy(event.archetype,
                    {{event.center.x + radial * std::cos(azimuth) * event.spacing,
                      event.center.y + y * event.spacing,
                      event.center.z + radial * std::sin(azimuth) * event.spacing * 0.35F},
                     index * 0.08F, F(Formation::Star), index});
            }
            break;
        }
        case Action::SpawnCrossingPair:
            for (int index = 0; index < event.count / 2; ++index) {
                const float y = -4.5F + static_cast<float>(index) * 2.6F;
                const float z = 7.8F - static_cast<float>(index) * 0.45F;
                sink.SpawnStageEnemy(event.archetype,
                    {{-9.0F, y, z}, index * 0.18F,
                     F(Formation::CrossingPair), index * 2});
                sink.SpawnStageEnemy(event.archetype,
                    {{9.0F, -y * 0.7F, z}, index * 0.18F,
                     F(Formation::CrossingPair), index * 2 + 1});
            }
            break;
        case Action::StartMidboss:
            sink.BeginStageBoss(false, *event.id);
            return false;
        case Action::StartDialogue:
            sink.BeginStageDialogue(*event.id);
            return false;
        case Action::StartFinalBoss:
            sink.BeginStageBoss(true, *event.id);
            return false;
        case Action::ShowMessage:
            sink.StageMessage(StartTitle(), 2.4F);
            break;
        default:
            throw std::invalid_argument("Unknown Stage 1 action");
    }
    return true;
}

}  // namespace redmist::stage1
