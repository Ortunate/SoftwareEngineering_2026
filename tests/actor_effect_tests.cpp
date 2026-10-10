// 本文件负责验证角色表现不干扰战斗、预退场曲线和脱离对象的效果池生命周期。

#include "gameplay/effects/actor_effects.hpp"
#include "gameplay/arena.hpp"
#include "stages/stage1/enemies.hpp"
#include "stages/stage1/boss.hpp"

#include <cassert>
#include <cmath>
#include <memory>

namespace {
/// @brief Fairy 及带出生轨道偏移的 Orb 在各帧率下完成入场后都应完全显示。
void TestEnemyEntryOpacity() {
    using namespace koumakyou;
    using Movement = stage1::FairyParams::Movement;
    auto bullets = std::make_unique<BulletSystem>();
    for (const int fps : {30, 60, 120, 240}) {
        const float dt = 1.0F / static_cast<float>(fps);
        for (const stage1::FairyParams params : {
                 stage1::FairyParams{{0, -0.5F, 8.8F}, Movement::Helix},
                 stage1::FairyParams{{9, 3.15F, 7.8F}, Movement::CrossFromRight},
                 stage1::FairyParams{{-9, -4.5F, 7.8F}, Movement::CrossFromLeft},
                 stage1::FairyParams{{0, 1.2F, 8.8F}, Movement::Straight}}) {
            stage1::FairyEnemy fairy(params);
            for (int frame = 0; frame < fps; ++frame) {
                fairy.Update(dt, {}, *bullets);
                if (fairy.age >= 0.3F)
                    assert(fairy.presentation.Sample().alpha == 1.0F);
                assert(fairy.active);
            }
        }
        stage1::OrbEnemy orb({{0, 0, 8.8F}, stage1::OrbParams::Movement::OrbitForward});
        for (int frame = 0; frame < fps; ++frame) {
            orb.Update(dt, {}, *bullets);
            if (orb.age >= 0.3F)
                assert(orb.presentation.Sample().alpha == 1.0F);
            assert(orb.active);
        }
    }
}

/// @brief 贴近边界但始终在场内的螺旋轨道，转向后不能保留外推产生的淡出。
void TestBoundaryPredictionRecovers() {
    using namespace koumakyou;
    auto bullets = std::make_unique<BulletSystem>();
    stage1::FairyEnemy fairy({{8.7F, 0, 8.8F}, stage1::FairyParams::Movement::Helix});
    bool sawPreview = false;
    for (int frame = 0; frame < 180; ++frame) {
        fairy.Update(1.0F / 60.0F, {}, *bullets);
        assert(fairy.active);
        assert(std::abs(fairy.position.x) < kArenaHalfExtents.x);
        if (fairy.age > 0.3F && fairy.presentation.Sample().alpha < 0.99F)
            sawPreview = true;
    }
    assert(sawPreview);
    assert(fairy.presentation.Sample().alpha == 1.0F);

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
    TestEnemyEntryOpacity();
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

    auto bullets = std::make_unique<BulletSystem>();
    stage1::FairyEnemy fairy({{0, 0, -11.2F}});
    fairy.Update(0.1F, {}, *bullets);
    assert(fairy.active);
    assert(fairy.presentation.Sample().alpha < 0.5F);
    fairy.Update(0.1F, {}, *bullets);
    assert(fairy.active);  // 到达场地表面，保留原有 +2 单位的回收容差。
    assert(fairy.presentation.Sample().alpha < 0.0001F);
    fairy.Update(1.4F, {}, *bullets);
    assert(!fairy.active);
    assert(bullets->Pool().ActiveCount() == 0);

    constexpr Vector3 start{0, 0, 8.5F};
    const float approach = 2.0F * kArenaHalfExtents.z / 5.0F / 1.7F;
    stage1::DarkFairyEnemy dark({start, 1.0F});
    dark.Update(2.0F * approach + 1.0F - 0.125F, {}, *bullets);
    assert(dark.active);
    assert(std::abs(dark.presentation.Sample().alpha - 0.5F) < 0.0001F);
    assert(bullets->Pool().ActiveCount() == 18);
    dark.Update(0.125F, {}, *bullets);
    assert(!dark.active);
    assert(dark.position.z == start.z);
    assert(dark.presentation.Sample().alpha == 0.0F);

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
    EnemyPool<1> pool;
    auto old = std::make_unique<stage1::FairyEnemy>(stage1::FairyParams{{1, 2, 3}});
    effects.Spawn(old->visual, old->position, old->hitRadius,
                  old->presentation, ActorExitReason::Defeated);
    old->active = false;
    assert(pool.Add(std::move(old)));
    assert(pool.Add(std::make_unique<stage1::OrbEnemy>(stage1::OrbParams{{4, 5, 6}})));
    effects.Pool().ForEachActive([](const ActorExitEffect& effect) {
        assert(effect.position.x == 1 && effect.position.y == 2 && effect.position.z == 3);
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
