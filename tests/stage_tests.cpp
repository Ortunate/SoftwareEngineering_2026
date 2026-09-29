// 本文件负责验证相对时间轴、异步事件与第一面内容工厂。

#include "gameplay/bullets/system.hpp"
#include "gameplay/bullets/patterns.hpp"
#include "gameplay/actors/boss.hpp"
#include "gameplay/actors/enemy.hpp"
#include "config/settings.hpp"
#include "stage/system.hpp"
#include "stage/catalog.hpp"
#include "stages/stage1/content.hpp"
#include "stages/stage1/enemies.hpp"
#include "stages/stage1/boss.hpp"

#include <cassert>
#include <array>
#include <cmath>
#include <filesystem>
#include <string_view>
#include <numbers>
#include <stdexcept>
#include <system_error>
#include <vector>

void RunStageTests() {
    constexpr auto line = static_cast<std::uint16_t>(redmist::stage1::Formation::Line);
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
            event.action == static_cast<std::uint16_t>(redmist::stage1::Action::SpawnCrossingPair)) ++enemyEvents;
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
    assert(visuals.size() == 8);
    assert(visuals[redmist::stage1::kFairy].path == "stages/stage1/fairy_modern.png");
    assert(visuals[redmist::stage1::kDarkFairy].path == "stages/stage1/dark_fairy_modern.png");
    assert(visuals[redmist::stage1::kRumia].path == "stages/stage1/rumia_modern.png");
    assert(visuals[redmist::stage1::kGreenSphere].mode == redmist::VisualMode::Sphere);
    const auto newFairy = stageFromCatalog->CreateEnemy(
        static_cast<std::uint16_t>(redmist::stage1::EnemyType::Fairy),
        {{}, 0.0F, line, 1});
    assert(newFairy && newFairy->visual == redmist::stage1::kFairy);
    assert(!stageFromCatalog->CreateEnemy(999, {{}, 0.0F, line, 0}));
    assert(!stageFromCatalog->CreateEnemy(
        static_cast<std::uint16_t>(redmist::stage1::EnemyType::Fairy),
        {{}, 0.0F, 999, 0}));
    const auto finalBoss = stageFromCatalog->CreateBoss(true);
    assert(finalBoss && finalBoss->active && !finalBoss->IsMidboss());

    struct EventProbe final : redmist::StageEventSink {
        int spawned = 0;
        int bosses = 0;
        int dialogues = 0;
        int messages = 0;
        void SpawnStageEnemy(std::uint16_t,
                             const redmist::EnemySpawn& spawn) override {
            assert(spawn.formation == static_cast<std::uint16_t>(
                redmist::stage1::Formation::Line));
            assert(spawn.slot == spawned);
            ++spawned;
        }
        void BeginStageBoss(bool, redmist::StageEventId) override { ++bosses; }
        void BeginStageDialogue(redmist::StageEventId) override { ++dialogues; }
        void StageMessage(std::string_view, float) override { ++messages; }
    } probe;
    assert(stageFromCatalog->HandleEvent(stageFromCatalog->Events()[1], probe));
    assert(probe.spawned == 8);
    assert(!stageFromCatalog->HandleEvent(stageFromCatalog->Events()[9], probe));
    assert(probe.bosses == 1);
}
