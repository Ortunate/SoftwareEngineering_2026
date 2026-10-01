// 本文件负责第一面的敌人与 Boss 工厂及时间轴事件处理。

#include "stages/stage1/content.hpp"
#include "stages/stage1/data.hpp"

#include "stages/stage1/boss.hpp"
#include "stages/stage1/enemies.hpp"

#include <cmath>
#include <stdexcept>

namespace redmist::stage1 {
namespace {
constexpr std::uint16_t F(Formation formation) {
    return static_cast<std::uint16_t>(formation);
}

}  // namespace

const StageAssetManifest& Stage1Content::Assets() const { return Stage1Manifest(); }
std::span<const StageEvent> Stage1Content::Events() const { return Stage1Timeline(); }
std::span<const DialogueLine> Stage1Content::Dialogue() const { return Stage1Dialogue(); }

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
            // slot 的奇偶位分别留给左右入场者；敌人据此选择对应轨迹。
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
