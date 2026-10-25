// 本文件负责验证角色表现不干扰战斗、预退场曲线和脱离对象的效果池生命周期。

#include "actor_fixture.hpp"
#include "gameplay/effects/actor_effects.hpp"
#include "gameplay/arena.hpp"
#include "stages/stage1/behaviors.hpp"
#include "stages/stage1/boss.hpp"

#include <cassert>
#include <cmath>
#include <memory>

namespace {
/// @brief 直接行为演员在各帧率下完成入场后完全可见，表现不改变五种配方的战斗状态。
void TestActorEntryOpacity() {
    using namespace koumakyou;
    auto bullets=std::make_unique<BulletSystem>();
    for(int fps:{30,60,120,240})for(const auto& pattern:std::array{stage1::kTurningFairy,stage1::kGentleTurningFairy,
            stage1::kLateTurningFairy,stage1::kShootingDarkFairy,stage1::kPassingDarkFairy}) {
        auto spawn=testing::SpawnAt({0,0,5});spawn.pattern=pattern;
        behavior::Actor actor;actor.Activate(spawn);
        for(int frame=0;frame<fps;++frame) {
            actor.Update(1.0F/fps,{},*bullets);
            if(actor.age>=0.3F)assert(actor.presentation.Sample().alpha==1);
            assert(actor.active);
        }
        bullets->Reset();
    }
}
/// @brief 边界预测在下一帧恢复；明确退场淡出单独保留，不绑定已删除的旧路径类。
void TestBoundaryPredictionRecovers() {
    using namespace koumakyou;
    ActorPresentation presentation;
    presentation.Update(1.0F);
    presentation.PreviewBoundaryExit(0.125F);
    assert(presentation.Sample().alpha == 0.5F);
    presentation.Update(0.01F);
    assert(presentation.Sample().alpha == 1.0F);
    presentation.PrepareExit(0.125F);
    presentation.PreviewBoundaryExit(0.0F);
    assert(presentation.Sample().alpha == 0.0F);
    presentation.Update(0.01F);
    assert(presentation.Sample().alpha == 0.5F);  // 明确退场的进度不能被预测重置。
}
}

/// @brief 验证公共角色视觉状态、自然退场、Boss 阶段和效果池，不需要窗口或资源。
void RunActorEffectTests() {
    using namespace koumakyou;
    TestActorEntryOpacity();
    TestBoundaryPredictionRecovers();
    ActorPresentation presentation;
    assert(presentation.Sample().alpha == 0.0F);
    assert(presentation.Sample().ringAlpha > 0.0F);
    presentation.Update(0.125F);
    assert(std::abs(presentation.Sample().alpha - 0.5F) < 0.0001F);
    presentation.Update(0.125F);
    assert(presentation.Sample().alpha == 1.0F);
    presentation.PrepareExit(0.125F);
    assert(std::abs(presentation.Sample().alpha - 0.5F) < 0.0001F);
    presentation.PrepareExit(2.0F);
    assert(std::abs(presentation.Sample().alpha - 0.5F) < 0.0001F);
    presentation.PrepareExit(0.0F);
    assert(presentation.Sample().alpha == 0.0F);

    ActorEffectSystem effects;
    assert(effects.Spawn(3, {1, 2, 3}, 0.55F, presentation, ActorExitReason::Retreat));
    effects.Pool().ForEachActive([](const ActorExitEffect& effect) {
        // 预淡出完成后不能因快照重新出现身体或光环。
        assert(effect.Sample().alpha == 0.0F);
        assert(effect.Sample().ringAlpha == 0.0F);
    });
    effects.Update(0.25F);
    assert(effects.Pool().ActiveCount() == 0);
    presentation.Reset();
    assert(effects.Spawn(3, {1, 2, 3}, 0.55F, presentation, ActorExitReason::Defeated));
    effects.Pool().ForEachActive([](const ActorExitEffect& effect) {
        assert(effect.Sample().alpha > 0.0F);
        assert(effect.Sample().burstAlpha == 1.0F);
    });
    effects.Update(0.15F);
    effects.Pool().ForEachActive([](const ActorExitEffect& effect) {
        assert(effect.Sample().burst == 0.5F);
        assert(effect.Sample().scale < effect.initial.scale);
    });
    effects.Update(0.15F);
    assert(effects.Pool().ActiveCount() == 0);

    auto bullets=std::make_unique<BulletSystem>();
    auto spawn=testing::SpawnAt({0,0,5});spawn.pattern.endFrame=5;
    behavior::Actor actor;actor.Activate(spawn);actor.Update(6.0F/60,{},*bullets);
    assert(!actor.active&&actor.finished&&bullets->Pool().ActiveCount()==0);

    stage1::RumiaBoss boss;
    boss.StartFinalBoss();
    boss.Update(0.3F, {}, *bullets);
    const float alpha = boss.presentation.Sample().alpha;
    assert(std::abs(alpha - 0.5F) < 0.0001F);
    assert(boss.AdvancePhase());
    assert(boss.presentation.Sample().alpha == alpha);
    assert(effects.Spawn(boss.visual, boss.position, boss.hitRadius,
                         boss.presentation, ActorExitReason::Defeated));
    effects.Update(0.5F);
    assert(effects.Pool().ActiveCount() == 1);
    effects.Update(0.3F);
    assert(effects.Pool().ActiveCount() == 0);

    // 快照持有值，角色被销毁及原槽位复用后不能变成下一只角色的外观或位置。
    behavior::ActorPool<1> pool;
    auto* old=pool.Spawn(testing::SpawnAt({1,2,3}));assert(old);
    const auto originalPosition=old->position;
    effects.Spawn(old->visual,old->position,old->hitRadius,old->presentation,ActorExitReason::Defeated);
    old->active=false;
    assert(pool.Spawn(testing::SpawnAt({4,5,6},stage1::kOrb)));
    effects.Pool().ForEachActive([originalPosition](const ActorExitEffect& effect) {
        assert(effect.position.x==originalPosition.x&&effect.position.y==originalPosition.y&&effect.position.z==originalPosition.z);
    });
    effects.Clear();
    for (int i = 0; i < 512; ++i)
        assert(effects.Spawn(0, {}, 1.0F, presentation, ActorExitReason::StageClear));
    assert(!effects.Spawn(0, {}, 1.0F, presentation, ActorExitReason::StageClear));
    assert(effects.Pool().DroppedSpawns() == 1);
    effects.Update(1.0F);
    assert(effects.Pool().ActiveCount() == 0);
    effects.Clear();
    assert(effects.Pool().DroppedSpawns() == 0);
}
