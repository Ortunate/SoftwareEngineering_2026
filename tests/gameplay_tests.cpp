// 本文件负责验证弹幕方向、敌人行动和 Boss 阶段行为。

#include "gameplay/bullets/system.hpp"
#include "gameplay/bullets/patterns.hpp"
#include "gameplay/actors/boss.hpp"
#include "gameplay/actors/enemy.hpp"
#include "gameplay/actors/player.hpp"
#include "gameplay/arena.hpp"
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
#include <memory>
#include <string_view>
#include <numbers>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace {
/// @brief 验证暗色妖精的 Z 轴往返、单次稀疏球面散射以及跨帧阶段切换。
void TestDarkFairyBehavior() {
    using redmist::Bullet;
    using redmist::BulletOwner;
    using redmist::BulletSystem;
    using redmist::stage1::DarkFairyEnemy;
    const Vector3 start{2.0F, -1.0F, 8.0F};
    const float travelDistance = redmist::kArenaHalfExtents.z * 2.0F / 5.0F;
    const float approachTime = travelDistance / 1.7F;
    const Vector3 turn{start.x, start.y, start.z - travelDistance};
    auto storage = std::make_unique<BulletSystem>();
    BulletSystem& bullets = *storage;
    DarkFairyEnemy fairy({start});

    fairy.Update(approachTime - 0.01F, {}, bullets);
    assert(fairy.active && fairy.position.x == start.x && fairy.position.y == start.y);
    assert(fairy.position.z > turn.z && fairy.position.z < start.z);
    assert(bullets.Pool().ActiveCount() == 0);
    fairy.Update(0.02F, {}, bullets);
    assert(fairy.active && std::abs(fairy.position.z - turn.z) < 0.0001F);
    assert(bullets.Pool().ActiveCount() == 18);

    std::vector<Vector3> directions;
    Vector3 velocitySum{};
    std::array<bool, 6> axisSigns{};
    int planarDirections = 0;
    bullets.Pool().ForEachActive([&](const Bullet& bullet) {
        assert(bullet.owner == BulletOwner::Enemy && bullet.visual == redmist::stage1::kRedBullet);
        assert(bullet.position.x == turn.x && bullet.position.y == turn.y);
        assert(std::abs(bullet.position.z - turn.z) < 0.0001F);
        assert(std::abs(BulletSystem::Length(bullet.velocity) - 3.5F) < 0.0001F);
        const Vector3 direction = redmist::pattern::Normalize(bullet.velocity);
        // 图中疏散环的三维扩展须无重叠方向，不能退化为平面环或密集同向弹束。
        for (Vector3 previous : directions) {
            const float dot = direction.x * previous.x + direction.y * previous.y +
                              direction.z * previous.z;
            assert(dot <= std::cos(std::numbers::pi_v<float> / 4.0F) + 0.0001F);
        }
        directions.push_back(direction);
        if (std::abs(direction.z) < 0.0001F) ++planarDirections;
        velocitySum = redmist::pattern::Add(velocitySum, bullet.velocity);
        if (direction.x > 0.1F) axisSigns[0] = true;
        if (direction.x < -0.1F) axisSigns[1] = true;
        if (direction.y > 0.1F) axisSigns[2] = true;
        if (direction.y < -0.1F) axisSigns[3] = true;
        if (direction.z > 0.1F) axisSigns[4] = true;
        if (direction.z < -0.1F) axisSigns[5] = true;
    });
    for (bool populated : axisSigns) assert(populated);
    assert(planarDirections == 8);
    assert(BulletSystem::Length(velocitySum) < 0.0001F);

    fairy.Update(0.5F, {}, bullets);
    assert(fairy.position.z == turn.z && fairy.active);
    fairy.Update(0.5F, {}, bullets);
    assert(fairy.position.z > turn.z && fairy.position.z < start.z);
    assert(fairy.position.x == start.x && fairy.position.y == start.y);
    assert(bullets.Pool().ActiveCount() == 18);
    fairy.Update(approachTime, {}, bullets);
    assert(!fairy.active && fairy.position.x == start.x && fairy.position.y == start.y);
    assert(fairy.position.z == start.z && bullets.Pool().ActiveCount() == 18);

    // 单帧跨过前进、停留和返回时仍只发射一次，发弹点不能错误地落在已返回的位置。
    bullets.Reset();
    DarkFairyEnemy coarse({start});
    coarse.Update(approachTime * 2.0F + 2.0F, {9.0F, 9.0F, 9.0F}, bullets);
    assert(!coarse.active && coarse.position.z == start.z);
    assert(bullets.Pool().ActiveCount() == 18);
    std::size_t directionIndex = 0;
    bullets.Pool().ForEachActive([&](const Bullet& bullet) {
        assert(std::abs(bullet.position.z - turn.z) < 0.0001F);
        const Vector3 direction = redmist::pattern::Normalize(bullet.velocity);
        const Vector3 expected = directions[directionIndex++];
        assert(std::abs(direction.x - expected.x) < 0.0001F);
        assert(std::abs(direction.y - expected.y) < 0.0001F);
        assert(std::abs(direction.z - expected.z) < 0.0001F);
    });

    // 等待时间属于实例配置，零等待仍在转折点发射一次，然后立即开始返回。
    bullets.Reset();
    DarkFairyEnemy immediate({start, 0.0F});
    immediate.Update(approachTime + 0.5F, {}, bullets);
    assert(immediate.active && immediate.position.z > turn.z);
    assert(bullets.Pool().ActiveCount() == 18);
    immediate.Update(approachTime, {}, bullets);
    assert(!immediate.active && bullets.Pool().ActiveCount() == 18);
}
/// @brief 验证预警不伤人、光束中段与端点胶囊碰撞、持续擦弹及清除，不依赖图形窗口。
void TestBeamContacts() {
    using namespace redmist;
    auto pool = std::make_unique<BulletSystem>();
    Player player;
    player.SetInvulnerability(0);
    const Vector3 target = player.Position();
    Bullet* beam = pool->SpawnBeam({target.x - 4, target.y, target.z}, {2, 0, 0}, 8, 0.2F, 1, 0.6F, SKYBLUE);
    assert(beam && !beam->Dangerous());
    assert(player.ContactEnemyBullet(*beam) == PlayerContact::None);
    pool->Update(0.99F);
    assert(player.ContactEnemyBullet(*beam) == PlayerContact::None);
    pool->Update(0.02F);
    assert(player.ContactEnemyBullet(*beam) == PlayerContact::Hit && beam->active);
    player.SetInvulnerability(2);
    assert(player.ContactEnemyBullet(*beam) == PlayerContact::None && beam->active);
    beam->position.y += 0.6F;
    assert(player.ContactEnemyBullet(*beam) == PlayerContact::Graze);
    assert(player.ContactEnemyBullet(*beam) == PlayerContact::None);
    assert(std::abs(beam->DistanceSquaredTo({target.x + 4.3F, target.y + 0.6F, target.z}) - 0.09F) < 0.0001F);
    pool->Update(0.6F);
    assert(!beam->active);
    assert(!pool->SpawnBeam({}, {}, -1, 0.2F, 1, 1, WHITE));
    beam = pool->SpawnBeam({}, {0, 0, 1}, 5, 0.2F, 1, 1, WHITE);
    assert(beam);
    auto* own = pool->Spawn(BulletOwner::Player, {}, {}, 0.1F, WHITE, 0);
    pool->ClearEnemyBullets();
    assert(!beam->active && own->active);

    // 各层同向开口中没有采样点；高仰俯角也保持单位方向与正确张角。
    for (Vector3 axis : {Vector3{0, 0, 1}, Vector3{0, 1, 0}, Vector3{1, 2, 3}}) {
        const auto forward = pattern::Normalize(axis);
        int count = 0;
        pattern::ForEachOpenRingDirection(axis, 24, 0.6F, 0, 0.42F, [&](Vector3 direction) {
            assert(std::abs(BulletSystem::Length(direction) - 1) < 0.0001F);
            const float dot = forward.x * direction.x + forward.y * direction.y + forward.z * direction.z;
            assert(std::abs(dot - std::cos(0.6F)) < 0.0001F);
            ++count;
        });
        assert(count == 21);
    }
}

}  // namespace

/// @brief 验证弹幕方向、生成及敌人和 Boss 行为；断言失败时终止进程。
void RunGameplayTests() {
    TestBeamContacts();
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

    TestDarkFairyBehavior();

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
    bullets.Reset();
    boss.Update(0.8F, {0, 0, -6.5F}, bullets);
    const redmist::Bullet* fixedBeam = nullptr;
    bullets.Pool().ForEachActive([&](const redmist::Bullet& bullet) {
        if (bullet.beamLength > 0) fixedBeam = &bullet;
    });
    assert(fixedBeam && !fixedBeam->Dangerous());
    const Vector3 endpoint = fixedBeam->BeamEnd();
    boss.Update(0.5F, {9, 9, 0}, bullets);
    assert(fixedBeam->BeamEnd().x == endpoint.x && fixedBeam->BeamEnd().z == endpoint.z);
    assert(boss.AdvancePhase());
    assert(std::string_view(boss.SpellName()) ==
           "DARK SIGN  -  DEMARCATION");
    bullets.Reset();
    boss.Update(2.6F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 64);
    assert(!boss.AdvancePhase());
    assert(!boss.active);

}
