// 本文件负责验证带参事件、连续出怪时序、异步依赖与第一面内容工厂。

#include "gameplay/actors/enemy.hpp"
#include "stage/catalog.hpp"
#include "stage/system.hpp"
#include "stages/stage1/content.hpp"
#include "stages/stage1/enemies.hpp"

#include <array>
#include <cassert>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace redmist;

/// @brief 验证非法事件表在绑定时被拒绝，避免运行中只执行部分波次。
bool RejectsEvents(std::span<const StageEvent> events) {
    try {
        StageDirector candidate(events);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

/// @brief 验证原有相对实际开始与实际结束的时间锚点不因参数类型迁移而改变。
void TestRelativeEvents() {
    constexpr std::array<StageEvent, 5> events{{
        {.delay = 1.0F, .action = ShowMessage{"1"}, .id = 10},
        {.delay = 2.0F, .action = StartDialogue{}, .id = 20},
        {.delay = 1.0F, .action = ShowMessage{"3"}},
        {.delay = 3.0F, .action = ShowMessage{"4"}, .afterEndOf = 20},
        {.delay = 1.0F, .action = ShowMessage{"5"}},
    }};
    StageDirector stage(events);
    std::vector<std::string_view> started;
    const auto record = [&](const StageEvent& event) {
        if (const auto* message = std::get_if<ShowMessage>(&event.action)) {
            started.push_back(message->text);
            return true;
        }
        started.push_back("2");
        return false;
    };
    stage.Update(5.0F, record);
    assert((started == std::vector<std::string_view>{"1"}));
    stage.Update(2.0F, record);
    assert((started == std::vector<std::string_view>{"1", "2"}));
    stage.Update(1.0F, record);
    assert((started == std::vector<std::string_view>{"1", "2", "3"}));
    assert(!stage.EndEvent(10));
    assert(stage.EndEvent(20));
    assert(!stage.EndEvent(20));
    stage.Update(2.5F, record);
    assert(started.size() == 3);
    stage.Update(0.5F, record);
    assert((started == std::vector<std::string_view>{"1", "2", "3", "4"}));
    stage.Update(1.0F, record);
    assert(started.back() == "5");
    stage.Reset();
    assert(stage.Elapsed() == 0.0F && !stage.EndEvent(20));
    started.clear();
    stage.Update(1.0F, record);
    assert((started == std::vector<std::string_view>{"1"}));
}

/// @brief 验证默认参数、跨帧补发、并行批次和最后一次生成的结束依赖。
void TestContinuousSpawns() {
    constexpr SpawnEnemies defaults{stage1::FairyParams{}};
    static_assert(defaults.count == 1 && defaults.interval == 0.5F);
    const std::array<StageEvent, 4> events{{
        {.action = SpawnEnemies{stage1::FairyParams{{1.0F, 2.0F, 8.0F}}, 4}, .id = 1},
        {.delay = 0.25F, .action = ShowMessage{"overlap"}},
        {.action = SpawnEnemies{stage1::OrbParams{}, 2, 0.25F}},
        {.delay = 0.25F, .action = ShowMessage{"done"}, .afterEndOf = 1},
    }};
    StageDirector stage(events);
    std::vector<float> fairyTimes;
    int orbs = 0;
    int messages = 0;
    const auto record = [&](const StageEvent& event) {
        if (const auto* spawn = std::get_if<SpawnEnemies>(&event.action)) {
            assert(spawn->count == 1);
            if (const auto* fairy = std::get_if<stage1::FairyParams>(&spawn->enemy)) {
                assert(fairy->position.x == 1.0F && fairy->position.y == 2.0F);
                fairyTimes.push_back(stage.Elapsed());
            } else {
                ++orbs;
            }
            // 批次结束属于调度器职责，不能由一次生成回调的返回值提前结束。
            return false;
        }
        ++messages;
        return true;
    };
    stage.Update(0.0F, record);
    assert((fairyTimes == std::vector<float>{0.0F}));
    assert(!stage.EndEvent(1));
    stage.Update(0.25F, record);
    assert(messages == 1 && orbs == 1 && fairyTimes.size() == 1);
    stage.Update(0.875F, record);
    assert((fairyTimes == std::vector<float>{0.0F, 1.125F, 1.125F}));
    assert(orbs == 2 && messages == 1);
    stage.Update(0.375F, record);
    assert(fairyTimes.size() == 4 && fairyTimes.back() == 1.5F);
    stage.Update(0.125F, record);
    assert(messages == 1);
    stage.Update(0.125F, record);
    assert(messages == 2 && !stage.EndEvent(1));
    stage.Update(100.0F, record);
    assert(fairyTimes.size() == 4 && orbs == 2);

    // 默认单只和零间隔批次都在启动帧完成；数量不是编队数量，不得截断奇数。
    const std::array<StageEvent, 3> immediate{{
        {.action = defaults},
        {.action = SpawnEnemies{stage1::DarkFairyParams{}, 3, 0.0F}, .id = 2},
        {.action = ShowMessage{"all"}, .afterEndOf = 2},
    }};
    StageDirector instant(immediate);
    int spawned = 0;
    int completed = 0;
    instant.Update(0.0F, [&](const StageEvent& event) {
        if (std::holds_alternative<SpawnEnemies>(event.action)) ++spawned;
        else ++completed;
        return true;
    });
    assert(spawned == 4 && completed == 1);

    // Reset 必须清掉尚未结束的批次进度，而不是仅重置下一个事件索引。
    stage.Reset();
    fairyTimes.clear();
    orbs = messages = 0;
    stage.Update(0.0F, record);
    stage.Reset();
    fairyTimes.clear();
    stage.Update(0.5F, record);
    assert((fairyTimes == std::vector<float>{0.5F}));
    stage.Update(0.5F, record);
    assert(fairyTimes.size() == 2);
}

/// @brief 验证清场取消进行中的出怪，未启动的后续波次仍可执行。
void TestSpawnCancellation() {
    const std::array<StageEvent, 3> events{{
        {.action = SpawnEnemies{stage1::FairyParams{}, 8}, .id = 1},
        {.delay = 0.25F, .action = StartBoss{}, .id = 2},
        {.action = SpawnEnemies{stage1::DarkFairyParams{}}, .afterEndOf = 2},
    }};
    StageDirector stage(events);
    int spawned = 0;
    const auto record = [&](const StageEvent& event) {
        if (std::holds_alternative<SpawnEnemies>(event.action)) {
            ++spawned;
            return true;
        }
        stage.CancelEnemySpawns();
        return false;
    };
    stage.Update(0.0F, record);
    stage.Update(0.25F, record);
    stage.Update(10.0F, record);
    assert(spawned == 1 && !stage.EndEvent(1));
    assert(stage.EndEvent(2));
    stage.Update(0.0F, record);
    assert(spawned == 2);
}

/// @brief 验证时间、批次数量、坐标、轨迹与异步 ID 的失败条件。
void TestInvalidEvents() {
    constexpr std::array<StageEvent, 2> badDependency{{
        {.id = 1}, {.afterEndOf = 2},
    }};
    constexpr std::array<StageEvent, 2> duplicateIds{{
        {.id = 1}, {.id = 1},
    }};
    assert(RejectsEvents(badDependency));
    assert(RejectsEvents(duplicateIds));
    const auto rejects = [](StageEvent event) {
        return RejectsEvents(std::span<const StageEvent>(&event, 1));
    };
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    for (float value : {-1.0F, infinity, nan}) {
        assert(rejects({.delay = value}));
        assert(rejects({.action = SpawnEnemies{stage1::FairyParams{}, 2, value}}));
        assert(rejects({.action = SpawnEnemies{stage1::DarkFairyParams{{}, value}}}));
        assert(rejects({.action = ShowMessage{"bad", value}}));
        if (value != -1.0F) {
            assert(rejects({.action = SpawnEnemies{stage1::FairyParams{{value, 0.0F, 0.0F}}}}));
        }
    }
    assert(rejects({.action = SpawnEnemies{stage1::FairyParams{}, 0}}));
    assert(rejects({.action = SpawnEnemies{stage1::FairyParams{}, -1}}));
    assert(rejects({.action = SpawnEnemies{stage1::FairyParams{}, 3,
                                         std::numeric_limits<float>::max()}}));
    assert(rejects({.action = SpawnEnemies{stage1::FairyParams{
        {}, static_cast<stage1::FairyParams::Movement>(999)}}}));
    assert(rejects({.action = StartBoss{}}));
    assert(rejects({.action = StartDialogue{}}));
    assert(rejects({.action = StartBoss{static_cast<BossEncounter>(999)}, .id = 1}));
    StageDirector empty;
    for (float value : {-1.0F, infinity, nan}) {
        bool rejected = false;
        try {
            empty.Update(value, [](const StageEvent&) { return true; });
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected && empty.Elapsed() == 0.0F);
    }
}

/// @brief 通过真实 StageContent 执行入口收集实例与演出请求，验证时间轴到工厂的完整链路。
struct EventProbe final : StageEventSink {
    const StageContent& content;
    std::vector<std::unique_ptr<Enemy>> enemies;
    int midbosses = 0;
    int finalBosses = 0;
    int dialogues = 0;
    int messages = 0;
    StageEventId midbossId = 0;
    StageEventId dialogueId = 0;

    explicit EventProbe(const StageContent& stage) : content(stage) {}
    void SpawnStageEnemy(const EnemySpec& spec) override {
        auto enemy = content.CreateEnemy(spec);
        assert(enemy && enemy->active && enemy->age == 0.0F);
        enemies.push_back(std::move(enemy));
    }
    void BeginStageBoss(bool final, StageEventId id) override {
        if (final) ++finalBosses;
        else { ++midbosses; midbossId = id; }
    }
    void BeginStageDialogue(StageEventId id, bool = false) override { ++dialogues; dialogueId = id; }
    void StageMessage(std::string_view text, float duration) override {
        assert(!text.empty() && duration > 0.0F);
        ++messages;
    }
};

/// @brief 验证 Stage1 波次人数、Boss/对话依赖、各带参敌人工厂与素材契约。
void TestStage1Content() {
    const auto content = CreateStageContent(StageId::Stage1);
    assert(content);
    StageDirector stage(content->Events());
    EventProbe probe(*content);
    const auto execute = [&](const StageEvent& event) {
        return content->HandleEvent(event, probe);
    };
    for (int frame=0;frame<1882;++frame) stage.Update(1.0F/60,execute);
    // 道中 Boss 的出生以 1882 帧为准；其结束不重置或暂停后续出生时间。
    stage.Update(1.0F/60,execute);
    assert(probe.midbosses==1 && probe.dialogues==0 && probe.finalBosses==0);
    assert(stage.EndEvent(probe.midbossId));
    for (int frame=1883;frame<5093;++frame) stage.Update(1.0F/60,execute);
    assert(probe.enemies.size()==222 && probe.dialogues==1 && probe.finalBosses==0);
    assert(stage.EndEvent(probe.dialogueId));
    stage.Update(0,execute);
    assert(probe.finalBosses==1);
    assert(stage.EndEvent(3));
    stage.Update(0,execute);
    assert(probe.dialogues==2); // 原作两句战后独白属于单独的异步对话。
    assert(content->Dialogue().size()==15 && content->Dialogue(true).size()==2);
    stage.Reset(); probe.enemies.clear();
    stage.Update(100,execute);
    assert(probe.enemies.size()==222 && probe.midbosses==2);

    const auto fairy = content->CreateEnemy(stage1::FairyParams{
        {1.0F, 2.0F, 5.0F}, stage1::FairyParams::Movement::Helix});
    assert(fairy && fairy->visual == stage1::kFairy && fairy->DropDistribution().size() == 2);
    assert(fairy->position.x == 1.0F && fairy->position.y == 2.0F && fairy->position.z == 5.0F);
    const auto dark = content->CreateEnemy(stage1::DarkFairyParams{});
    const auto orb = content->CreateEnemy(stage1::OrbParams{});
    assert(dark && dark->visual == stage1::kDarkFairy);
    assert(orb && orb->visual == stage1::kOrb);
    assert(!content->CreateEnemy(stage1::FairyParams{
        {}, static_cast<stage1::FairyParams::Movement>(999)}));
    assert(!content->CreateEnemy(stage1::OrbParams{
        {}, static_cast<stage1::OrbParams::Movement>(-1)}));
    assert(!content->CreateEnemy(stage1::DarkFairyParams{
        {0.0F, std::numeric_limits<float>::infinity(), 0.0F}}));
    const auto finalBoss = content->CreateBoss(true);
    assert(finalBoss && finalBoss->active && !finalBoss->IsMidboss());

    const auto& visuals = content->Assets().visuals;
    assert(visuals.size() == 13);
    assert(visuals[stage1::kFairy].path == "stages/stage1/fairy_modern.png");
    assert(visuals[stage1::kDarkFairy].path == "stages/stage1/dark_fairy_modern.png");
    assert(visuals[stage1::kRumia].path == "stages/stage1/rumia_cutout.png");
    assert(visuals[stage1::kGreenSphere].mode == VisualMode::Sphere);
    assert(content->PlayerBulletVisual() == stage1::kPlayerGoldCard);
    assert(content->PlayerHomingVisual() == stage1::kPlayerCyanCard);
    assert(content->PowerPickupVisual() == stage1::kPowerPoint);
    assert(content->ScorePickupVisual() == stage1::kScorePoint);
    assert(content->StarPickupVisual() == stage1::kStarPoint);
}
}  // namespace

/// @brief 执行关卡事件、连续出怪与内容契约测试；断言失败时终止进程。
void RunStageTests() {
    TestRelativeEvents();
    TestContinuousSpawns();
    TestSpawnCancellation();
    TestInvalidEvents();
    TestStage1Content();
}
