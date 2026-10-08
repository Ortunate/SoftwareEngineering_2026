// 本文件负责验证弹幕方向、敌人行动和 Boss 阶段行为。

#include "gameplay/bullets/system.hpp"
#include "gameplay/bullets/patterns.hpp"
#include "gameplay/actors/boss.hpp"
#include "gameplay/actors/enemy.hpp"
#include "gameplay/actors/player.hpp"
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

/// @brief 验证弹幕方向、生成及敌人和 Boss 行为；断言失败时终止进程。
void RunGameplayTests() {
    using redmist::BulletOwner;
    using redmist::BulletSystem;
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

    // 回收仍保留原来的世界单位余量；Z 新增区间内的对象不能按旧立方体过早失活。
    for (int axis = 0; axis < 3; ++axis) {
        for (float sign : {-1.0F, 1.0F}) {
            const auto axisPosition = [axis, sign](float distance) -> Vector3 {
                if (axis == 0) return {sign * distance, 0.0F, 0.0F};
                if (axis == 1) return {0.0F, sign * distance, 0.0F};
                return {0.0F, 0.0F, sign * distance};
            };
            const float bulletLimit = axis == 2 ? 14.5F : 13.0F;
            auto* edgeBullet = bullets.Spawn(BulletOwner::Enemy,
                axisPosition(bulletLimit), {}, 0.1F, RED,
                redmist::stage1::kRedBullet);
            assert(edgeBullet);
            bullets.Update(0.0F);
            assert(edgeBullet->active);
            edgeBullet->position = axisPosition(bulletLimit + 0.01F);
            bullets.Update(0.0F);
            assert(!edgeBullet->active);

            const float enemyLimit = axis == 2 ? 13.5F : 12.0F;
            redmist::stage1::FairyEnemy edgeEnemy(
                {axisPosition(enemyLimit)});
            edgeEnemy.Update(0.0F, {}, bullets);
            assert(edgeEnemy.active);
            edgeEnemy.origin = axisPosition(enemyLimit + 0.01F);
            edgeEnemy.Update(0.0F, {}, bullets);
            assert(!edgeEnemy.active);
            bullets.Reset();
        }
    }
    redmist::stage1::FairyEnemy fairy({{0.0F, 0.0F, 5.0F}});
    const Vector3 fairyMuzzle = fairy.MuzzlePosition({0.0F, 0.0F, 0.0F});
    assert(std::abs(fairyMuzzle.z - (5.0F - fairy.hitRadius * 0.55F)) <
           0.0001F);
    fairy.Update(0.9F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 0);
    // 覆盖旧首次攻击和后续攻击时刻，避免仅延后首发而未移除攻击。
    for (int frame = 0; frame < 8; ++frame) fairy.Update(0.5F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 0);
    assert(fairy.health > 0.0F && fairy.health == fairy.maxHealth);
    // 使用真实玩家射击验证低火力直射弹与满火力追踪弹均能一发耗尽妖精血量。
    for (int power : {0, redmist::Player::kMaxPower}) {
        redmist::Player player;
        player.AdjustPower(power);
        player.Update(0.0F, {.fire = true}, bullets,
                      redmist::stage1::kPlayerGoldCard,
                      redmist::stage1::kPlayerCyanCard, true);
        assert(bullets.Pool().ActiveCount() > 0);
        bullets.Pool().ForEachActive([&fairy](const redmist::Bullet& bullet) {
            assert(bullet.damage >= fairy.health);
        });
        bullets.Reset();
    }
    bullets.Reset();

    redmist::stage1::DarkFairyEnemy darkFairy(
        {{0.0F, 0.0F, 5.0F}});
    darkFairy.Update(0.9F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 1);
    assert(darkFairy.position.x != fairy.position.x);
    bullets.Reset();

    redmist::stage1::FairyEnemy helixFairy(
        {{0.0F, 0.0F, 5.0F}, redmist::stage1::FairyParams::Movement::Helix});
    helixFairy.Update(0.9F, {}, bullets);
    assert(helixFairy.position.x != fairy.position.x);
    assert(bullets.Pool().ActiveCount() == 0);
    bullets.Reset();
    redmist::stage1::FairyEnemy crossingFairy(
        {{-9.0F, 0.0F, 5.0F}, redmist::stage1::FairyParams::Movement::CrossFromLeft});
    crossingFairy.Update(0.9F, {}, bullets);
    assert(crossingFairy.position.x > -9.0F);
    assert(crossingFairy.visual == fairy.visual);
    assert(bullets.Pool().ActiveCount() == 0);
    bullets.Reset();

    redmist::stage1::OrbEnemy orb({{0.0F, 0.0F, 5.0F}});
    orb.Update(0.9F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 6);
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

}
