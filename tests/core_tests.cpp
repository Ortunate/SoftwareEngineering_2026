#include "game/bullet_system.hpp"
#include "game/bullet_patterns.hpp"
#include "game/boss.hpp"
#include "game/enemy.hpp"
#include "game/settings.hpp"
#include "game/stage_system.hpp"
#include "game/stage_catalog.hpp"
#include "game/stages/stage1_content.hpp"
#include "game/stages/stage1_enemies.hpp"
#include "game/stages/stage1_boss.hpp"

#include <cassert>
#include <array>
#include <cmath>
#include <filesystem>
#include <string_view>
#include <numbers>
#include <stdexcept>
#include <system_error>
#include <vector>

int main() {
    using redmist::BulletOwner;
    using redmist::BulletSystem;
    using redmist::GameSettings;
    using redmist::KeyBindings;

    const KeyBindings defaultBindings;
    assert(defaultBindings.descend == KEY_LEFT_SHIFT);
    assert(defaultBindings.focus == KEY_C);

    const std::filesystem::path settingsPath =
        std::filesystem::temp_directory_path() /
        "red_mist_settings_core_test.cfg";
    std::error_code settingsError;
    std::filesystem::remove(settingsPath, settingsError);
    const GameSettings createdSettings =
        redmist::LoadOrCreateSettings(settingsPath);
    assert(std::filesystem::exists(settingsPath));
    assert(createdSettings.fpsLimit == 60);
    assert(createdSettings.bindings.descend == KEY_LEFT_SHIFT);

    GameSettings changedSettings = createdSettings;
    changedSettings.fpsLimit = 144;
    changedSettings.bindings.fire = KEY_F1;
    assert(redmist::SaveSettingsFile(settingsPath, changedSettings));
    const GameSettings loadedSettings =
        redmist::LoadOrCreateSettings(settingsPath);
    assert(loadedSettings.fpsLimit == 144);
    assert(loadedSettings.bindings.fire == KEY_F1);
    std::filesystem::remove(settingsPath, settingsError);

    const Vector3 forward = BulletSystem::DirectionFromYawPitch(0.0F, 0.0F);
    assert(std::abs(forward.x) < 0.0001F);
    assert(std::abs(forward.y) < 0.0001F);
    assert(std::abs(forward.z - 1.0F) < 0.0001F);

    const Vector3 right = BulletSystem::DirectionFromYawPitch(
        std::numbers::pi_v<float> * 0.5F, 0.0F);
    assert(std::abs(right.x - 1.0F) < 0.0001F);
    assert(std::abs(right.z) < 0.0001F);

    const Vector3 aimed = BulletSystem::DirectionTo(
        {0.0F, 0.0F, 0.0F}, {0.0F, 3.0F, 4.0F});
    assert(std::abs(aimed.y - 0.6F) < 0.0001F);
    assert(std::abs(aimed.z - 0.8F) < 0.0001F);

    BulletSystem bullets;
    redmist::pattern::ForEachSphereDirection(64, 0.0F,
        [&bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, {},
                          redmist::pattern::Scale(direction, 2.5F), 0.16F, RED,
                          redmist::stage1::kRedBullet);
        });
    assert(bullets.Pool().ActiveCount() == 64);

    bool hasPositiveX = false;
    bool hasNegativeX = false;
    bool hasPositiveY = false;
    bool hasNegativeY = false;
    bool hasPositiveZ = false;
    bool hasNegativeZ = false;
    bullets.Pool().ForEachActive([&](const redmist::Bullet& bullet) {
        assert(bullet.visual == redmist::stage1::kRedBullet);
        assert(std::abs(BulletSystem::Length(bullet.velocity) - 2.5F) < 0.0002F);
        hasPositiveX = hasPositiveX || bullet.velocity.x > 0.2F;
        hasNegativeX = hasNegativeX || bullet.velocity.x < -0.2F;
        hasPositiveY = hasPositiveY || bullet.velocity.y > 0.2F;
        hasNegativeY = hasNegativeY || bullet.velocity.y < -0.2F;
        hasPositiveZ = hasPositiveZ || bullet.velocity.z > 0.2F;
        hasNegativeZ = hasNegativeZ || bullet.velocity.z < -0.2F;
    });
    assert(hasPositiveX && hasNegativeX && hasPositiveY && hasNegativeY &&
           hasPositiveZ && hasNegativeZ);

    bullets.Reset();
    redmist::pattern::ForEachConeDirection({0.0F, 0.0F, 1.0F},
        2, 6, 0.45F, 0.0F, [&bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, {},
                          redmist::pattern::Scale(direction, 3.0F), 0.16F, BLUE,
                          redmist::stage1::kBlueBullet);
        });
    assert(bullets.Pool().ActiveCount() == 13);
    bullets.Pool().ForEachActive([](const redmist::Bullet& bullet) {
        assert(bullet.visual == redmist::stage1::kBlueBullet);
    });

    bullets.ClearEnemyBullets();
    assert(bullets.Pool().ActiveCount() == 0);

    redmist::stage1::FairyEnemy fairy(redmist::stage1::Path::Straight, {0.0F, 0.0F, 5.0F});
    const Vector3 fairyMuzzle = fairy.MuzzlePosition({0.0F, 0.0F, 0.0F});
    assert(std::abs(fairyMuzzle.z - (5.0F - fairy.hitRadius * 0.55F)) <
           0.0001F);
    fairy.Update(0.9F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 1);
    bullets.Pool().ForEachActive([](const redmist::Bullet& bullet) {
        assert(bullet.visual == redmist::stage1::kRedBullet);
        assert(bullet.position.z < 5.0F);
    });
    bullets.Reset();

    redmist::stage1::DarkFairyEnemy darkFairy(redmist::stage1::Path::Straight,
                                       {0.0F, 0.0F, 5.0F});
    darkFairy.Update(0.9F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 1);
    bullets.Reset();

    redmist::stage1::OrbEnemy orb(redmist::stage1::Path::Straight, {0.0F, 0.0F, 5.0F});
    orb.Update(0.9F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 6);
    bullets.Reset();

    redmist::stage1::EliteEnemy elite(redmist::stage1::Path::Straight, {0.0F, 0.0F, 5.0F});
    elite.Update(0.9F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 42);
    bullets.Reset();

    redmist::stage1::RumiaBoss boss;
    boss.StartMidboss();
    assert(boss.SpellName() == nullptr);
    assert(std::abs(boss.position.x - 3.0F) < 0.0001F);
    assert(!boss.Update(1.0F, {}, bullets));
    assert(bullets.Pool().ActiveCount() == 22);
    bullets.Reset();
    assert(!boss.Update(6.6F, {}, bullets));
    assert(std::abs(boss.position.x + 3.0F) < 0.0001F);
    assert(boss.SpellName() == nullptr);
    assert(!boss.Update(9.4F, {}, bullets));
    assert(boss.Update(5.0F, {}, bullets));
    assert(!boss.AdvancePhase());
    assert(!boss.active);

    boss.StartFinalBoss();
    assert(boss.CurrentEncounter() == redmist::stage1::RumiaBoss::Encounter::Final);
    assert(boss.SpellName() == nullptr);
    assert(boss.AdvancePhase());
    assert(std::string_view(boss.SpellName()) ==
           "NIGHT SIGN  -  NIGHT BIRD");
    bullets.Reset();
    boss.Update(2.6F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 36);
    assert(boss.AdvancePhase());
    assert(boss.SpellName() == nullptr);
    assert(boss.AdvancePhase());
    assert(std::string_view(boss.SpellName()) ==
           "BOUNDARY SIGN  -  DEMARCATION");
    bullets.Reset();
    boss.Update(2.6F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 50);
    assert(!boss.AdvancePhase());
    assert(!boss.active);

    constexpr std::array<redmist::StageEvent, 5> relativeEvents{{
        {.time = 1.0F, .action = 0,
         .count = 1, .id = 10},
        {.time = 2.0F, .action = 0,
         .count = 2, .id = 20},
        {.time = 1.0F, .action = 0,
         .count = 3},
        {.time = 3.0F, .action = 0,
         .count = 4, .afterEndOf = 20},
        {.time = 1.0F, .action = 0,
         .count = 5},
    }};
    redmist::StageDirector relativeStage(relativeEvents);
    std::vector<int> startedEvents;
    auto recordRelative = [&](const redmist::StageEvent& event) {
        startedEvents.push_back(event.count);
        return event.count != 2;
    };
    relativeStage.Update(5.0F, recordRelative);
    assert((startedEvents == std::vector<int>{1}));
    relativeStage.Update(2.0F, recordRelative);
    assert((startedEvents == std::vector<int>{1, 2}));
    relativeStage.Update(1.0F, recordRelative);
    assert((startedEvents == std::vector<int>{1, 2, 3}));
    assert(!relativeStage.EndEvent(10));
    assert(relativeStage.EndEvent(20));
    assert(!relativeStage.EndEvent(20));
    relativeStage.Update(2.9F, recordRelative);
    assert(startedEvents.size() == 3);
    relativeStage.Update(0.2F, recordRelative);
    assert((startedEvents == std::vector<int>{1, 2, 3, 4}));
    relativeStage.Update(1.1F, recordRelative);
    assert((startedEvents == std::vector<int>{1, 2, 3, 4, 5}));
    relativeStage.Reset();
    assert(relativeStage.Elapsed() == 0.0F);
    assert(!relativeStage.EndEvent(20));
    startedEvents.clear();
    relativeStage.Update(1.0F, recordRelative);
    assert((startedEvents == std::vector<int>{1}));

    constexpr std::array<redmist::StageEvent, 2> invalidEvents{{
        {.time = 0.0F, .id = 1},
        {.time = 1.0F, .afterEndOf = 2},
    }};
    constexpr std::array<redmist::StageEvent, 2> duplicateIds{{
        {.time = 0.0F, .id = 1},
        {.time = 1.0F, .id = 1},
    }};
    constexpr std::array<redmist::StageEvent, 1> negativeDelay{{
        {.time = -1.0F},
    }};
    const auto rejectsEvents = [](const auto& events) {
        try {
            redmist::StageDirector candidate(events);
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    assert(rejectsEvents(invalidEvents));
    assert(rejectsEvents(duplicateIds));
    assert(rejectsEvents(negativeDelay));

    redmist::stage1::Stage1Content stageContent;
    redmist::StageDirector stage(stageContent.Events());
    int midbossEvents = 0;
    int dialogueEvents = 0;
    int finalBossEvents = 0;
    int enemyEvents = 0;
    redmist::StageEventId midbossId = 0;
    redmist::StageEventId dialogueId = 0;
    auto recordEvent = [&](const redmist::StageEvent& event) {
        if (event.action == static_cast<std::uint16_t>(redmist::stage1::Action::StartMidboss)) {
            ++midbossEvents;
            midbossId = *event.id;
        }
        if (event.action == static_cast<std::uint16_t>(redmist::stage1::Action::StartDialogue)) {
            ++dialogueEvents;
            dialogueId = *event.id;
        }
        if (event.action == static_cast<std::uint16_t>(redmist::stage1::Action::StartFinalBoss))
            ++finalBossEvents;
        if (event.action == static_cast<std::uint16_t>(redmist::stage1::Action::SpawnLine) ||
            event.action == static_cast<std::uint16_t>(redmist::stage1::Action::SpawnCrossingPair) ||
            event.action == static_cast<std::uint16_t>(redmist::stage1::Action::SpawnElite)) ++enemyEvents;
        return event.action != static_cast<std::uint16_t>(redmist::stage1::Action::StartMidboss) &&
               event.action != static_cast<std::uint16_t>(redmist::stage1::Action::StartDialogue) &&
               event.action != static_cast<std::uint16_t>(redmist::stage1::Action::StartFinalBoss);
    };
    for (int frame = 0; frame < 400; ++frame)
        stage.Update(0.1F, recordEvent);
    assert(midbossEvents == 1 && dialogueEvents == 0 && finalBossEvents == 0);
    assert(enemyEvents == 8);
    assert(stage.EndEvent(midbossId));
    for (int frame = 0; frame < 29; ++frame)
        stage.Update(0.1F, recordEvent);
    assert(enemyEvents == 8);
    for (int frame = 0; frame < 2; ++frame)
        stage.Update(0.1F, recordEvent);
    assert(enemyEvents == 9);
    for (int frame = 0; frame < 600; ++frame)
        stage.Update(0.1F, recordEvent);
    assert(dialogueEvents == 1 && finalBossEvents == 0);
    assert(enemyEvents == 17);
    assert(stage.EndEvent(dialogueId));
    stage.Update(0.0F, recordEvent);
    assert(finalBossEvents == 1);
    stage.Update(100.0F, recordEvent);
    assert(finalBossEvents == 1);

    stage.Reset();
    midbossEvents = dialogueEvents = finalBossEvents = enemyEvents = 0;
    for (int frame = 0; frame < 600; ++frame)
        stage.Update(0.1F, recordEvent);
    assert(midbossEvents == 1 && enemyEvents == 8);
    assert(dialogueEvents == 0 && finalBossEvents == 0);
    assert(stage.EndEvent(midbossId));
    for (int frame = 0; frame < 31; ++frame)
        stage.Update(0.1F, recordEvent);
    assert(enemyEvents == 9);
    // 关卡内容拥有自己的外观 ID、敌人工厂与事件解释；通用调度器不识别这些编号。
    const auto stageFromCatalog = redmist::CreateStageContent(redmist::StageId::Stage1);
    assert(stageFromCatalog);
    const auto& visuals = stageFromCatalog->Assets().visuals;
    assert(visuals[redmist::stage1::kFairy].path == "fairy_modern.png");
    assert(visuals[redmist::stage1::kDarkFairy].path == "dark_fairy_modern.png");
    assert(visuals[redmist::stage1::kGreenSphere].mode == redmist::VisualMode::Sphere);
    const auto newFairy = stageFromCatalog->CreateEnemy(
        static_cast<std::uint16_t>(redmist::stage1::EnemyType::Fairy),
        static_cast<std::uint16_t>(redmist::stage1::Path::Helix), {}, 0.0F);
    assert(newFairy && newFairy->visual == redmist::stage1::kFairy);
    assert(!stageFromCatalog->CreateEnemy(999, 0, {}, 0.0F));
    const auto finalBoss = stageFromCatalog->CreateBoss(true);
    assert(finalBoss && finalBoss->active && !finalBoss->IsMidboss());

    struct EventProbe final : redmist::StageEventSink {
        int spawned = 0;
        int bosses = 0;
        int dialogues = 0;
        int messages = 0;
        void SpawnStageEnemy(std::uint16_t, std::uint16_t,
                             Vector3, float) override { ++spawned; }
        void BeginStageBoss(bool, redmist::StageEventId) override { ++bosses; }
        void BeginStageDialogue(redmist::StageEventId) override { ++dialogues; }
        void StageMessage(std::string_view, float) override { ++messages; }
    } probe;
    assert(stageFromCatalog->HandleEvent(stageFromCatalog->Events()[1], probe));
    assert(probe.spawned == 8);
    assert(!stageFromCatalog->HandleEvent(stageFromCatalog->Events()[9], probe));
    assert(probe.bosses == 1);
    return 0;
}
