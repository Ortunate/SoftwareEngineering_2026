// 本文件负责验证带参事件、连续出怪时序、异步依赖及已注册关卡的内容工厂。

#include "boss_fixture.hpp"
#include "stages/stage1/boss_config.hpp"
#include "gameplay/bosses/rumia.hpp"
#include "gameplay/bosses/daiyousei.hpp"
#include "gameplay/bosses/cirno.hpp"
#include "gameplay/bosses/meiling.hpp"
#include "gameplay/bosses/koakuma.hpp"
#include "gameplay/bosses/patchouli.hpp"
#include "gameplay/bosses/sakuya.hpp"
#include "gameplay/bosses/remilia.hpp"
#include "actor_fixture.hpp"
#include "gameplay/actors/behavior.hpp"
#include "stage/catalog.hpp"
#include "stage/system.hpp"
#include "stages/stage1/content.hpp"
#include "stages/stage1/behaviors.hpp"
#include "app/spell_practice.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace koumakyou;

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
    constexpr SpawnActors defaults{testing::SpawnAt()};
    static_assert(defaults.count == 1 && defaults.interval == 0.5F);
    const std::array<StageEvent, 4> events{{
        {.action = SpawnActors{testing::SpawnAt({1.0F,2.0F,8.0F}), 4}, .id = 1},
        {.delay = 0.25F, .action = ShowMessage{"overlap"}},
        {.action = SpawnActors{testing::SpawnAt({},stage1::kOrb), 2, 0.25F}},
        {.delay = 0.25F, .action = ShowMessage{"done"}, .afterEndOf = 1},
    }};
    StageDirector stage(events);
    std::vector<float> fairyTimes;
    int orbs = 0;
    int messages = 0;
    const auto record = [&](const StageEvent& event) {
        if (const auto* spawn = std::get_if<SpawnActors>(&event.action)) {
            assert(spawn->count == 1);
            if (spawn->actor.visual==stage1::kFairy) {
                const auto* fairy=&spawn->actor;
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
        {.action = SpawnActors{testing::SpawnAt({},stage1::kDarkFairy), 3, 0.0F}, .id = 2},
        {.action = ShowMessage{"all"}, .afterEndOf = 2},
    }};
    StageDirector instant(immediate);
    int spawned = 0;
    int completed = 0;
    instant.Update(0.0F, [&](const StageEvent& event) {
        if (std::holds_alternative<SpawnActors>(event.action)) ++spawned;
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
        {.action = SpawnActors{testing::SpawnAt(), 8}, .id = 1},
        {.delay = 0.25F, .action = StartBoss{&stage1::kRumiaMidboss}, .id = 2},
        {.action = SpawnActors{testing::SpawnAt({},stage1::kDarkFairy)}, .afterEndOf = 2},
    }};
    StageDirector stage(events);
    int spawned = 0;
    const auto record = [&](const StageEvent& event) {
        if (std::holds_alternative<SpawnActors>(event.action)) {
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
        assert(rejects({.action = SpawnActors{testing::SpawnAt(), 2, value}}));
        assert(rejects({.action = SpawnActors{testing::SpawnAt({},stage1::kDarkFairy, value)}}));
        assert(rejects({.action = ShowMessage{"bad", value}}));
        if (value != -1.0F) {
            assert(rejects({.action = SpawnActors{testing::SpawnAt({value,0,0})}}));
        }
    }
    assert(rejects({.action = SpawnActors{testing::SpawnAt(), 0}}));
    assert(rejects({.action = SpawnActors{testing::SpawnAt(), -1}}));
    assert(rejects({.action = SpawnActors{testing::SpawnAt(), 3,
                                         std::numeric_limits<float>::max()}}));
    auto invalid=testing::SpawnAt();invalid.pattern.movement.count=5;
    assert(rejects({.action=SpawnActors{invalid}}));
    assert(rejects({.action = StartBoss{&stage1::kRumiaMidboss}}));
    assert(rejects({.action = StartDialogue{}}));
    assert(!rejects({.action=StartSceneryTransition{}}));
    assert(rejects({.action=WaitDialogueSignal{}}));
    assert(rejects({.action=WaitDialogueSignal{-1},.id=1}));
    assert(rejects({.action=StartDialogue{false,DialogueScene::FinalBoss,static_cast<DialogueClearPolicy>(99)},.id=20}));
    assert(rejects({.action=StartDialogue{false,static_cast<DialogueScene>(99)},.id=20}));
    assert(!rejects({.action=StartDialogue{false,DialogueScene::EncoreBoss,DialogueClearPolicy::PreserveWorld},.id=20}));
    assert(rejects({.action = StartBoss{nullptr}, .id = 1}));
    const bosses::Battle emptyBattle{};
    assert(rejects({.action = StartBoss{&emptyBattle}, .id = 1}));
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

/// @brief 通过真实 StageContent 执行入口收集实例与演出请求，验证时间轴到行为组件的完整链路。
struct EventProbe final : StageEventSink {
    const StageContent& content;
    std::vector<std::unique_ptr<behavior::Actor>> enemies;
    int midbosses = 0;
    int finalBosses = 0;
    int sceneryTransitions = 0;
    int dialogues = 0;
    int messages = 0;
    StageEventId midbossId = 0;
    StageEventId dialogueId = 0;

    explicit EventProbe(const StageContent& stage) : content(stage) {}
    void SpawnStageActor(const behavior::Spawn& spec) override {
        auto enemy = testing::MakeActor(spec);
        assert(enemy && enemy->active && enemy->age == 0.0F);
        enemies.push_back(std::move(enemy));
    }
    void BeginStageBoss(const bosses::Battle& battle, StageEventId id) override {
        if (&battle==&content.PracticeBoss()) ++finalBosses;
        else { ++midbosses; midbossId = id; }
    }
    void BeginStageDialogue(StageEventId id, bool = false,DialogueScene=DialogueScene::FinalBoss,
        DialogueClearPolicy=DialogueClearPolicy::ClearEnemiesAndBullets) override { ++dialogues; dialogueId = id; }
    void ResumeStageBoss() override {}
    void BeginStageSceneryTransition() override { ++sceneryTransitions; }
    bool AwaitStageDialogueSignal(StageEventId,int) override {return false;}
    void StageMessage(std::string_view text, float duration) override {
        assert(!text.empty() && duration > 0.0F);
        ++messages;
    }
};

/// @brief 独立核对源 ECL Sub3/4 的难度掩码参数；同一出生配置不得因其他实例修改。
void TestStage1DarkFairyDifficulty() {
    using D=native_combat::Difficulty;
    constexpr std::array difficulties{D::Easy,D::Normal,D::Hard,D::Lunatic};
    constexpr std::array counts{3,7,9,11};
    constexpr std::array layers{1,1,2,2};
    constexpr std::array spacing{0.7853982F,0.62831855F,0.3926991F,0.157079637F};
    auto spawn=testing::SpawnAt();spawn.pattern=stage1::kShootingDarkFairy;
    for(std::size_t i=0;i<difficulties.size();++i) {
        const stage1::Stage1Content content(difficulties[i]);
        const auto configured=content.ConfigureSpawn(spawn);
        const auto& shot=*configured.pattern.skill;
        assert(configured.difficulty==difficulties[i]&&shot.frame==70);
        assert(shot.shot.count==counts[i]&&shot.shot.layers==layers[i]);
        assert(std::abs(shot.shot.spacing-spacing[i])<1e-6F);
        assert(shot.shot.lastSpeed==(i>=2?1.33928573F:0));
        assert(behavior::Valid(configured));
    }
    assert(spawn.pattern.skill->shot.count==7);
    spawn.pattern=stage1::kPassingDarkFairy;
    for(auto difficulty:difficulties) {
        const auto configured=stage1::ConfigureSpawn(spawn,difficulty);
        if(difficulty==D::Lunatic) {
            assert(configured.pattern.skill&&configured.pattern.skill->shot.count==5);
            assert(configured.pattern.skill->frame==70);
        } else assert(!configured.pattern.skill);
    }
}
/// @brief Easy 首非符的死亡与超时均转到第二组；符卡编号随源难度独立绑定。
void TestRumiaDifficultyConfig() {
    using D=native_combat::Difficulty;using S=bosses::RumiaBoss::Behavior;
    const auto easy=stage1::RumiaFinalConfig(ShotType::HomingAmulet,D::Easy);
    assert(easy.difficulty==D::Easy&&easy.phases.size()==2);
    assert(easy.phases[0].skills.size()==1&&easy.phases[0].skills[0].id==S::FirstNonspell);
    bosses::RumiaBoss boss;
    const auto first=boss.DescribeSkill(S::FirstNonspell,ShotType::HomingAmulet,D::Easy);
    assert(first.NativeHealth()==7000&&first.reserveHealth==0&&first.timeoutFrames==2100);
    for(auto difficulty:{D::Easy,D::Normal,D::Hard,D::Lunatic}) {
        const int index=native_combat::DifficultyIndex(difficulty);
        assert(boss.DescribeSkill(S::Demarcation,ShotType::HomingAmulet,difficulty).spellCard==index+6);
        if(index>0)assert(boss.DescribeSkill(S::NightBird,ShotType::HomingAmulet,difficulty).spellCard==index+2);
    }
    bool rejected=false;
    try {boss.DescribeSkill(S::NightBird,ShotType::HomingAmulet,D::Easy);}catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
    for(auto difficulty:{D::Hard,D::Lunatic}) {
        const auto mid=stage1::RumiaMidbossConfig(ShotType::HomingAmulet,difficulty);
        assert(mid.phases.size()==1&&mid.phases[0].skills.size()==2);
        assert(mid.phases[0].skills[0].onTimeout.kind==bosses::Transition::Kind::Exit);
        assert(mid.phases[0].skills[1].onTimeout.kind==bosses::Transition::Kind::ExitDefeated);
        const auto firstMid=boss.DescribeSkill(S::Midboss,ShotType::HomingAmulet,difficulty);
        const auto spell=boss.DescribeSkill(S::MoonlightRay,ShotType::HomingAmulet,difficulty);
        assert(firstMid.NativeHealth()==6000&&firstMid.reserveHealth==500&&firstMid.timeoutFrames==1440);
        assert(spell.NativeHealth()==500&&spell.timeoutFrames==1320&&spell.spellCard==native_combat::DifficultyIndex(difficulty)-1);
    }
    // 原道中普通攻击阈值接符卡；符卡超时仍走 Sub7 击破式退场，但不视作捕获。
    auto mid=stage1::RumiaMidbossConfig(ShotType::HomingAmulet,D::Hard);
    auto bullets=std::make_unique<BulletSystem>();boss.StartBoss(mid);
    for(int i=0;i<61;++i)boss.Update(1.0F/60,{},*bullets);
    boss.health=499;assert(boss.Update(1.0F/60,{},*bullets));assert(boss.AdvanceSkill());
    assert(boss.attack==2&&boss.health==500&&!boss.CanTakeDamage());
    bool timedOut=false;
    for(int i=0;i<1330&&!timedOut;++i)timedOut=boss.Update(1.0F/60,{},*bullets);
    assert(timedOut&&!boss.SkillDefeated());assert(boss.AdvanceSkill()&&boss.Defeated());
    assert(boss.CompletionScore()==100000);
    for(int i=0;i<170&&boss.active;++i)if(boss.Update(1.0F/60,{},*bullets))boss.AdvanceSkill();
    assert(!boss.active);
}

/// @brief 验证 Stage1 波次人数、Boss/对话依赖、行为演员池与素材契约。
void TestStage1Content() {
    const auto content = CreateStageContent(StageId::Stage1);
    assert(content);
    int spatialBirths=0;bool upper=false,lower=false;
    for(const auto& event:content->Events())if(const auto* spawn=std::get_if<SpawnActors>(&event.action)) {
        ++spatialBirths;upper=upper||spawn->actor.position.y>0;lower=lower||spawn->actor.position.y<0;
        assert(spawn->actor.pattern.spatialShots);
        assert(spawn->actor.retirementHeights);
    }
    assert(spatialBirths==222&&upper&&lower);
    StageDirector stage(content->Events());
    EventProbe probe(*content);
    assert(content->HandleEvent(StageEvent{.action=StartSceneryTransition{}},probe));
    assert(probe.sceneryTransitions==1);
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

    behavior::ActorPool<1> pool;
    auto* actor=pool.Spawn(testing::SpawnAt({1,2,5}));
    assert(actor&&actor->active&&std::abs(actor->position.x-1)<0.0001F&&
           std::abs(actor->position.y-2)<0.0001F&&std::abs(actor->position.z-5)<0.0001F);
    assert(!pool.Spawn(testing::SpawnAt())&&pool.DroppedSpawns()==1);
    actor->active=false;assert(pool.Spawn(testing::SpawnAt({},stage1::kDarkFairy)));
    pool.Clear();assert(pool.DroppedSpawns()==0);
    const auto finalBoss = content->CreateBoss(testing::BattleAt(*content,1));
    assert(finalBoss && finalBoss->active && finalBoss->DisplayName()==std::string_view("Rumia"));

    const auto& visuals = content->Assets().visuals;
    assert(visuals.size() == stage1::kVisualCount);
    assert(visuals[stage1::kFairy].animation.atlas == battle::Atlas::Stage1Enemies);
    assert(visuals[stage1::kDarkFairy].animation.script == 3);
    assert(visuals[stage1::kRumia].animation.atlas == battle::Atlas::Rumia);
    for (int side=0;side<2;++side) {
        const auto id=content->PlayerOptionVisual(side);
        assert(id==(side==0 ? stage1::kPlayerOptionLeft : stage1::kPlayerOptionRight));
        assert(visuals[id].id==id && visuals[id].animation.atlas==battle::Atlas::Reimu);
        assert(visuals[id].animation.script==128+side && visuals[id].worldHeight==1.0F);
    }
    assert(content->PlayerOptionVisual(-1)==kInvalidVisualId && content->PlayerOptionVisual(2)==kInvalidVisualId);
    assert(content->BossPortraitVisual()!=stage1::kRumia);
    const std::array<std::string_view,6> reimuFiles{
        "face00a_0.png", "face00a_1.png", "face00b_0.png", "face00b_1.png", "face00c_0.png", "face00c_1.png"};
    const std::array<std::string_view,4> rumiaFiles{
        "face03a_0.png", "face03a_1.png", "face03b_0.png", "face03b_1.png"};
    for(int index=0;index<6;++index) {
        const auto id=content->HeroPortraitVisual(index);
        assert(id==stage1::kReimuPortraits[index]);
        assert(visuals[id].id==id && visuals[id].path.ends_with(reimuFiles[index]));
    }
    for(int index=0;index<4;++index) {
        const auto id=content->BossPortraitVisual(index);
        assert(id==stage1::kRumiaPortraits[index]);
        assert(visuals[id].id==id && visuals[id].path.ends_with(rumiaFiles[index]));
    }
    assert(content->HeroPortraitVisual(-1)==content->HeroPortraitVisual());
    assert(content->HeroPortraitVisual(6)==content->HeroPortraitVisual());
    assert(content->BossPortraitVisual(-1)==content->BossPortraitVisual());
    assert(content->BossPortraitVisual(4)==content->BossPortraitVisual());
    assert(content->SpellPortraitVisual()==content->BossPortraitVisual(2));
    // 台词交替时，未发言的一方保留最近一次表情；独白和越界查询不继承上一场对话状态。
    const auto before=content->Dialogue();
    assert((DialogueExpressionsAt(before,5)==std::array<int,2>{3,1}));
    assert((DialogueExpressionsAt(before,6)==std::array<int,2>{5,1}));
    assert((DialogueExpressionsAt(before,9)==std::array<int,2>{0,3}));
    assert((DialogueExpressionsAt(before,100)==std::array<int,2>{3,2}));
    assert((DialogueExpressionsAt(content->Dialogue(true),0)==std::array<int,2>{2,0}));
    assert((DialogueExpressionsAt(before,2,19.0F/60)==std::array<int,2>{0,0}));
    assert((DialogueExpressionsAt(before,2,20.0F/60)==std::array<int,2>{5,0}));
    // 来自 msg1 的实际展开帧，专门覆盖相邻 wait 共用计数及末尾等待，不能按句长重新估算。
    const std::array<int,15> reimuFrames{361,361,321,151,240,311,301,301,301,301,301,301,301,301,530};
    for(std::size_t index=0;index<before.size();++index)
        assert(std::abs(before[index].duration*60-reimuFrames[index])<0.001F);
    assert(before[0].secondLineReplacesFirst && before[0].textDelay==1);
    assert(content->Dialogue(true)[0].duration==6 && content->Dialogue(true)[1].duration==16);
    const auto reimuCues=content->DialogueCues(false,ShotType::HomingAmulet);
    assert(!DialogueFlagAt(reimuCues,DialogueCueKind::InputEnabled,1745.0F/60,true));
    assert(DialogueFlagAt(reimuCues,DialogueCueKind::InputEnabled,1775.0F/60,true));
    assert(!DialogueFlagAt(reimuCues,DialogueCueKind::BossMusic,3851.0F/60));
    assert(DialogueFlagAt(reimuCues,DialogueCueKind::BossMusic,3852.0F/60));
    assert((DialogueExpressionsAt({},0)==std::array<int,2>{0,0}));
    // 逐行采样必须兼容旧的延迟出现及同槽覆盖；没有独立说话人时仍共用原名牌。
    const auto hiddenRows=DialogueTextRowsAt(before[0],.5F);
    assert(hiddenRows[0].text[0]=='\0'&&hiddenRows[1].text[0]=='\0');
    auto delayedReplacement=before[0];delayedReplacement.line2Delay=1.0F/60;
    const auto firstRows=DialogueTextRowsAt(delayedReplacement,delayedReplacement.textDelay);
    assert(std::string_view(firstRows[0].text)==before[0].text&&firstRows[1].text[0]=='\0');
    const auto replacedRows=DialogueTextRowsAt(before[0],before[0].textDelay+before[0].line2Delay);
    assert(std::string_view(replacedRows[0].text)==before[0].secondLine&&replacedRows[1].text[0]=='\0');
    assert(std::string_view(replacedRows[0].speaker)==before[0].speaker);
    assert(visuals[stage1::kGreenSphere].mode == VisualMode::Bullet);
    assert(content->PlayerBulletVisual() == stage1::kPlayerGoldCard);
    assert(content->PlayerHomingVisual() == stage1::kPlayerCyanCard);
    assert(content->PlayerNeedleVisual() == stage1::kPlayerNeedle);
    assert(visuals[stage1::kPlayerNeedle].animation.atlas == battle::Atlas::Reimu);
    assert(visuals[stage1::kPlayerNeedle].animation.script == 66);
    for(auto shot:{ShotType::MagicMissile,ShotType::IllusionLaser}) {
        const auto player=content->PlayerVisuals(shot);
        assert(player.primary==stage1::kMarisaPrimary && visuals[player.primary].animation.atlas==battle::Atlas::Marisa);
        assert(content->PlayerOptionVisual(0,shot)==stage1::kMarisaOptionLeft && content->PlayerOptionVisual(1,shot)==stage1::kMarisaOptionRight);
        assert(content->HeroPortraitVisual(0,shot)==stage1::kMarisaPortraits[0]);
        const auto dialogue=content->Dialogue(false,shot);
        assert(dialogue.size()==10 && content->Dialogue(true,shot).size()==2);
        const std::array<int,10> marisaFrames{361,301,361,340,201,301,301,301,301,430};
        for(std::size_t index=0;index<dialogue.size();++index)
            assert(std::abs(dialogue[index].duration*60-marisaFrames[index])<0.001F);
        const auto cues=content->DialogueCues(false,shot);
        assert(!DialogueFlagAt(cues,DialogueCueKind::BossMusic,2466.0F/60));
        assert(DialogueFlagAt(cues,DialogueCueKind::BossMusic,2467.0F/60));
        assert((DialogueExpressionsAt(content->Dialogue(true,shot),0)==std::array<int,2>{1,0}));
        for(int index=0;index<6;++index) {
            const auto id=content->HeroPortraitVisual(index,shot);
            assert(visuals[id].path.ends_with(std::array<std::string_view,6>{"face01a_0.png","face01a_1.png","face01b_0.png","face01b_1.png","face01c_0.png","face01c_1.png"}[index]));
            assert(visuals[id].dialogueCrop.height==0.6F);
        }
        for(const auto& line:dialogue)if(!line.bossSpeaking)assert(std::string_view(line.speaker)=="魔理沙");
    }
    assert(content->PowerPickupVisual() == stage1::kPowerPoint);
    assert(content->ScorePickupVisual() == stage1::kScorePoint);
    assert(content->StarPickupVisual() == stage1::kStarPoint);
}
/// @brief 六关必须同时可选、可构造且按闯关顺序衔接；非法编号不得别名到已有内容。
void TestPlayableCatalog() {
    auto bullets=std::make_unique<BulletSystem>();
    // 独立目录编号必须实际到达同一符卡；覆盖道中入口、Easy 缩短组合及四种御札。
    for(const auto& card:spell::Cards())if(spell::Playable(&card,0))for(int shot=0;shot<4;++shot) {
        const auto difficulty=card.stage==6?native_combat::Difficulty::Normal:*native_combat::DifficultyFromIndex(card.difficulty);
        const auto content=CreateStageContent(*SpellPracticeStage(card.stage),difficulty,StageEntry::SpellPractice);
        auto boss=content->CreateBoss(content->PracticeBoss(card.number),static_cast<ShotType>(shot));
        if(boss->attack==0)boss->ResumeCombat();
        for(int skipped=1;skipped<card.phase;++skipped)assert(boss->AdvanceSkill());
        assert(boss->attack==card.phase);
        for(int frame=0;frame<180&&!boss->SpellName();++frame)boss->Update(1.0F/60,{},*bullets);
        assert(boss->SpellCardNumber()==card.number&&std::string_view(boss->SpellName())==card.name);
        assert(!bullets->Pool().DroppedSpawns());bullets->Reset();
        bool rejectedCard=false;
        try {(void)content->PracticeBoss(132);}catch(const std::invalid_argument&){rejectedCard=true;}
        assert(rejectedCard);
    }
    constexpr std::array<StageId,6> expected{StageId::Stage1,StageId::Stage2,StageId::Stage3,
        StageId::Stage4,StageId::Stage5,StageId::Stage6};
    for(std::size_t index=0;index<expected.size();++index) {
        const auto id=expected[index];
        assert(kPracticeStages[index]==id && StageNumber(id)==static_cast<int>(index)+1);
        assert(NextStage(id)==(index+1<expected.size() ? std::optional{expected[index+1]} : std::nullopt));
        const auto content=CreateStageContent(id);
        assert(content && !content->Events().empty() && !content->Assets().visuals.empty());
        StageDirector director(content->Events()); // 同时验证所有生产事件表的依赖和参数约束。
        const auto mid=content->CreateBoss(testing::BattleAt(*content,0)),final=content->CreateBoss(testing::BattleAt(*content,1));
        // 原 ECL 103 的 X/Y 全宽，受弹球取短边的一半；道中咲夜在对白开战时才从 56 改为 40。
        constexpr std::array<float,6> midWidths{48,45,56,40,56,56},finalWidths{48,48,56,48,56,56};
        assert(mid&&final);
        switch(index) {
            case 0:assert(dynamic_cast<bosses::RumiaBoss*>(mid.get())&&dynamic_cast<bosses::RumiaBoss*>(final.get()));break;
            case 1:assert(dynamic_cast<bosses::DaiyouseiBoss*>(mid.get())&&dynamic_cast<bosses::CirnoBoss*>(final.get()));break;
            case 2:assert(dynamic_cast<bosses::MeilingBoss*>(mid.get())&&dynamic_cast<bosses::MeilingBoss*>(final.get()));break;
            case 3:assert(dynamic_cast<bosses::KoakumaBoss*>(mid.get())&&dynamic_cast<bosses::PatchouliBoss*>(final.get()));break;
            case 4:assert(dynamic_cast<bosses::SakuyaBoss*>(mid.get())&&dynamic_cast<bosses::SakuyaBoss*>(final.get()));break;
            case 5:assert(dynamic_cast<bosses::SakuyaBoss*>(mid.get())&&dynamic_cast<bosses::RemiliaBoss*>(final.get()));break;
        }
        assert(std::abs(mid->hitRadius-midWidths[index]*0.5F*(20.0F/448))<1e-6F);
        assert(std::abs(final->hitRadius-finalWidths[index]*0.5F*(20.0F/448))<1e-6F);
        if(index>=4) {
            mid->ResumeCombat();
            assert(std::abs(mid->hitRadius-20*(20.0F/448))<1e-6F);
        }
        for(const auto& event:content->Events())if(const auto* spawn=std::get_if<SpawnActors>(&event.action)) {
            const float radius=spawn->actor.hitRadius;
            // 二面有全宽 16/22 的小演员，其他当前时间轴演员均为全宽 28。
            assert(std::abs(radius-14*(20.0F/448))<1e-6F || (index==1&&
                (std::abs(radius-8*(20.0F/448))<1e-6F||std::abs(radius-11*(20.0F/448))<1e-6F)));
        }
    }
    const auto invalid=static_cast<StageId>(255);
    assert(StageNumber(invalid)==0 && !NextStage(invalid));
    bool rejected=false;
    try { (void)CreateStageContent(invalid); } catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
}
/// @brief 测试角色把血量绑定在自己的技能目录中，公共执行器只接收引用及显式血条外观。
class ConfigTestBoss final : public bosses::ConfiguredBoss {
public:
    enum class Behavior {First,Second,Third,Timed,Survival,ThresholdOnly,ZeroHealth,NegativeHealth,Exit};
    const char* DisplayName() const override {return "Config test";}
protected:
    bosses::Attack ResolveBehavior(bosses::BehaviorId id) const override {
        if(!id.Is<Behavior>())throw std::invalid_argument("Foreign test behavior");
        return bosses::Run(static_cast<Behavior>(id.value)==Behavior::Exit ? Leave : Wait);
    }
    bosses::SkillDefinition ResolveSkill(bosses::BehaviorId id,ShotType) const override {
        if(!id.Is<Behavior>())throw std::invalid_argument("Foreign test skill");
        bosses::SkillDefinition p;p.behavior=ResolveBehavior(id);p.timeoutFrames=6;
        switch(static_cast<Behavior>(id.value)) {
            case Behavior::First:p.health=6;p.reserveHealth=4;break;
            case Behavior::Second:p.health=4;p.timeoutFrames=12;break;
            case Behavior::Third:p.health=20;p.timeoutFrames=18;break;
            case Behavior::Timed:p.health=10;p.timeoutArmFrame=2;break;
            case Behavior::Survival:p.health=10;p.survival=true;p.spellName="生存测试";break;
            case Behavior::ThresholdOnly:p.health=0;p.reserveHealth=1000;break;
            case Behavior::ZeroHealth:p.health=0;break;
            case Behavior::NegativeHealth:p.health=-1;p.reserveHealth=1000;break;
            default:throw std::invalid_argument("Not a test skill");
        }
        return p;
    }
    bosses::SkillDefinition ResolveSkillForDifficulty(bosses::BehaviorId id,ShotType shot,native_combat::Difficulty difficulty) const override {
        auto result=ResolveSkill(id,shot);
        if(difficulty==native_combat::Difficulty::Hard)result.health+=100;
        else if(difficulty!=native_combat::Difficulty::Normal)throw std::invalid_argument("Unsupported test difficulty");
        return result;
    }
private:
    static behavior::Routine Wait(behavior::Actor& actor) {actor.damageable=true;co_await actor.At(100000);}
    static behavior::Routine Leave(behavior::Actor& actor) {co_await actor.At(4);actor.finished=true;}
};
/// @brief 验证一阶段多技能、与 HP 无关的比例/颜色、可变段数、复制、跳转和原子交接。
void TestBossStartupConfig() {
    using namespace bosses;using S=ConfigTestBoss::Behavior;
    auto config=stage1::RumiaMidbossConfig();config.initialContact=false;
    config.phases={{{S::First,S::Second},{{{1,0.75F,GREEN},{0,0.25F,BLUE}}}},{{S::Third},{{{0,1,RED}}}}};
    config.intro={};config.defeated=config.retreat=S::Exit;config.defeatScore=77;config.retreatScore=11;
    config.phases[0].healthBar.background=BLACK;
    ConfigTestBoss restartBoss;
    restartBoss.StartBoss(config);assert(restartBoss.health==10);
    config.difficulty=native_combat::Difficulty::Hard;restartBoss.StartBoss(config);assert(restartBoss.health==110);
    config.difficulty=native_combat::Difficulty::Normal;restartBoss.StartBoss(config);assert(restartBoss.health==10);
    ConfigTestBoss boss;boss.StartBoss(config);auto bullets=std::make_unique<BulletSystem>();
    config.phases[0].healthBar.segments[0].ratio=0.1F;config.phases.clear(); // 容器复制后不再借用调用方。
    auto bar=boss.HealthBar();assert(ColorToInt(bar.background)==ColorToInt(BLACK));assert(boss.health==10&&boss.phase==1&&boss.skill==1&&boss.attack==1);
    assert(bar.count==2&&bar.remainingBars==1&&bar.segments[0].capacity==0.75F&&ColorToInt(bar.segments[0].color)==ColorToInt(GREEN));
    boss.health=7;bar=boss.HealthBar();assert(bar.segments[0].remaining==0.75F&&bar.segments[1].remaining==0.125F);
    boss.Update(1.0F/120,{},*bullets);boss.health=3;
    assert(boss.ShouldAdvanceFromDamage()&&boss.AdvanceSkill()&&boss.phase==1&&boss.skill==2&&boss.attack==2&&boss.health==4);
    bar=boss.HealthBar();assert(bar.segments[1].remaining==0&&bar.segments[0].remaining==0.75F);
    assert(!boss.CanTakeDamage()&&!boss.ShouldAdvanceFromDamage());
    boss.Update(1.0F/240,{},*bullets);assert(!boss.CanTakeDamage());boss.Update(1.0F/240,{},*bullets);assert(boss.CanTakeDamage());
    boss.health=0;assert(boss.AdvanceSkill()&&boss.phase==2&&boss.skill==1&&boss.attack==3&&boss.health==20);
    boss.Update(1.0F/120,{},*bullets);boss.health=0;assert(boss.AdvanceSkill()&&boss.active);
    bool finished=false;for(int i=0;i<10&&!finished;++i)if(boss.Update(1.0F/120,{},*bullets))finished=!boss.AdvanceSkill();
    assert(finished&&boss.Defeated()&&boss.CompletionScore()==77);
    // 自定义组合可以用阈值技能收尾；寄存器仍为正时也必须按实际技能血量判定整场击破。
    config.phases={{{S::First},{{{0,1,WHITE}}}}};boss.StartBoss(config);
    boss.Update(1.0F/120,{},*bullets);boss.health=3;
    boss.Update(1.0F/120,{},*bullets);assert(boss.health==4&&boss.AdvanceSkill()&&boss.Defeated());
    for(int i=0;i<12&&boss.active;++i)if(boss.Update(1.0F/120,{},*bullets))boss.AdvanceSkill();
    assert(!boss.active&&boss.CompletionScore()==77);
    // 同一技能出现四次，各段按本阶段的使用索引绑定，不能依赖固定三段或用技能 ID 去重。
    config.phases={{{S::Timed,S::Timed,S::Timed,S::Timed},{{{3,0.1F,RED},{2,0.2F,BLUE},{1,0.3F,GREEN},{0,0.4F,YELLOW}}}}};
    boss.StartBoss(config);assert(boss.HealthBar().count==4);boss.AdvanceSkill();boss.AdvanceSkill();
    assert(boss.phase==1&&boss.skill==3&&boss.HealthBar().segments[3].remaining==0&&boss.HealthBar().segments[0].remaining==0.1F);
    config.phases={{{S::Timed},{{{0,1,RED}}}}};
    for(int fps:{30,60,120,144,240}) {
        boss.StartBoss(config);double elapsed=0;
        while(boss.active&&elapsed<1) {if(boss.Update(1.0F/fps,{},*bullets))boss.AdvanceSkill();elapsed+=1.0/fps;}
        assert(!boss.active&&!boss.Defeated()&&boss.CompletionScore()==11);
        assert(std::abs(elapsed-12.0/60)<=2.0/fps+1.0/120);
    }
    // 超时也可按内容指定击破式退场和奖励；技能捕获仍依据真实超时，不跟随退场外观。
    config.phases[0].skills[0].onTimeout=Transition::ExitDefeated();boss.StartBoss(config);
    bool timedOut=false;
    for(int i=0;i<20&&!timedOut;++i)timedOut=boss.Update(1.0F/120,{},*bullets);
    assert(timedOut&&!boss.SkillDefeated());
    assert(boss.AdvanceSkill()&&boss.Defeated()&&boss.CompletionScore()==77);
    for(int i=0;i<20&&boss.active;++i)if(boss.Update(1.0F/120,{},*bullets))boss.AdvanceSkill();
    assert(!boss.active);
    config.phases={{{S::Timed,S::Second},{{{0,0.5F,RED},{1,0.5F,BLUE}}}},{{S::Third},{{{0,1,GREEN}}}}};
    config.phases[0].skills[0].onTimeout=Transition::Jump(1,0);boss.StartBoss(config);
    for(int i=0;i<20&&boss.phase==1;++i)if(boss.Update(1.0F/120,{},*bullets))boss.AdvanceSkill();
    assert(boss.phase==2&&boss.skill==1&&boss.attack==3);
    // 原生命打到零可跳过预留符卡；阈值及超时各自保留默认/显式路径，配置按值复制。
    auto split=config;split.phases[0].skills[0].id=S::First;
    split.phases[0].skills[0].onDefeat=Transition::Jump(1,0);
    split.phases[0].skills[0].onTimeout=Transition{};
    for(bool zero:{false,true}) {
        ConfigTestBoss routed;routed.StartBoss(split);routed.Update(1.0F/120,{},*bullets);routed.health=zero?0:3;
        assert(routed.AdvanceSkill()&&routed.attack==(zero?3:2));
    }
    // 指定击破式超时奖励不会改变真实完成原因；重开必须清除上次原因。
    auto forced=split;forced.phases={{{S::Timed},{{{0,1,WHITE}}}}};forced.phases[0].skills[0].onTimeout=Transition::ExitDefeated();
    ConfigTestBoss reason;reason.StartBoss(forced);
    for(int i=0;i<30;++i)if(reason.Update(1.0F/120,{},*bullets)) {assert(reason.AdvanceSkill());break;}
    assert(reason.Defeated()&&reason.RetirementFromTimeout());reason.StartBoss(split);assert(!reason.RetirementFromTimeout());
    const auto rejects=[&](StartConfig invalid) {try {boss.StartBoss(invalid);}catch(const std::invalid_argument&){return true;}return false;};
    auto invalid=config;invalid.phases.clear();assert(rejects(invalid));
    invalid=config;invalid.phases[0].skills[0].onTimeout=Transition::Jump(9,0);assert(rejects(invalid));
    invalid=config;invalid.phases[0].skills[0].onDefeat=Transition::Jump(9,0);assert(rejects(invalid));
    invalid=config;invalid.phases[0].skills[0].id=RumiaBoss::Behavior::Midboss;assert(rejects(invalid));
    invalid=config;invalid.phases[0].healthBar.segments[0].ratio=std::numeric_limits<float>::quiet_NaN();assert(rejects(invalid));
    invalid=config;invalid.phases[0].healthBar.segments[0].ratio=0.1F;assert(rejects(invalid));
    invalid=config;invalid.phases[0].healthBar.segments[0].skillIndex=9;assert(rejects(invalid));
    invalid=config;invalid.phases[0].healthBar.segments[0].skillIndex=1;assert(rejects(invalid));
    assert(boss.phase==2&&boss.attack==3); // 非法新配置不能破坏仍在运行的旧战斗。
    for(const auto id:{S::ZeroHealth,S::NegativeHealth}) {
        invalid=config;invalid.phases[0].skills[0].id=id;assert(rejects(invalid));
    }
    // 原寄存器仅剩阈值的过渡可以启动，但血条不能除以零；前后技能仍按显式比例和顺序交接。
    config.phases={{{S::First,S::ThresholdOnly,S::Second},{{{0,.4F,WHITE},{1,.1F,WHITE},{2,.5F,PINK}}}}};
    boss.StartBoss(config);assert(boss.HealthBar().segments[1].remaining==0);
    boss.AdvanceSkill();bar=boss.HealthBar();assert(boss.health==1000&&bar.segments[1].remaining==0&&bar.segments[2].remaining==.5F);
    assert(!boss.ShouldAdvanceFromDamage());boss.Update(1.0F/120,{},*bullets);boss.health=999;
    assert(boss.ShouldAdvanceFromDamage()&&boss.AdvanceSkill()&&boss.health==4&&boss.attack==3);
    // 准备项可显式不占段，后继仍显示完整血条；省略与实际段引用相矛盾必须拒绝。
    config.phases={{{S::ThresholdOnly,S::Second},{{{1,1,PINK}}}}};config.phases[0].skills[0].omitHealthBarSegment=true;
    boss.StartBoss(config);assert(boss.HealthBar().count==1&&boss.HealthBar().segments[0].remaining==1);
    auto badBar=config;badBar.phases[0].healthBar.segments[0].skillIndex=0;assert(rejects(badBar));
    badBar=config;badBar.phases[0].skills[0].omitHealthBarSegment=false;assert(rejects(badBar));
    // 生存成功条件与伤害完成分离；普通超时仍失败，清场差异只在实际生存超时交接时生效。
    for(auto id:{S::Survival,S::Second}) {
        config.phases={{{id,S::Second},{{{0,.5F,WHITE},{1,.5F,PINK}}}}};boss.StartBoss(config);
        assert(!boss.SpellSucceeded()&&boss.ClearsBulletsOnSkillEnd());
        bool ready=false;for(int tick=0;tick<30&&!ready;++tick)ready=boss.Update(1.0F/120,{},*bullets);
        assert(ready&&!boss.SkillDefeated()&&boss.SpellSucceeded()==(id==S::Survival));
        assert(boss.ClearsBulletsOnSkillEnd()==(id!=S::Survival));
        assert(boss.AdvanceSkill()&&boss.ClearsBulletsOnSkillEnd()&&!boss.SpellSucceeded());
    }
    config.phases={{{S::Survival,S::Second},{{{0,.5F,WHITE},{1,.5F,PINK}}}}};boss.StartBoss(config);
    boss.Update(1.0F/120,{},*bullets);boss.health=0;
    assert(boss.ShouldAdvanceFromDamage()&&boss.SkillDefeated()&&!boss.SpellSucceeded()&&boss.ClearsBulletsOnSkillEnd());
}
}  // namespace

/// @brief 执行关卡事件、连续出怪与内容契约测试；断言失败时终止进程。
void RunStageTests() {
    TestRelativeEvents();
    TestContinuousSpawns();
    TestSpawnCancellation();
    TestInvalidEvents();
    TestStage1Content();
    TestStage1DarkFairyDifficulty();
    TestRumiaDifficultyConfig();
    TestPlayableCatalog();
    TestBossStartupConfig();
}
