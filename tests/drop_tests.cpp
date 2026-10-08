// 本文件负责验证敌人掉落、道具运动、满火力清弹星点与拾取结算。

#include "gameplay/arena.hpp"
#include "gameplay/items/drop_system.hpp"
#include "gameplay/bullets/system.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <memory>

namespace {
// 测试场景须恰有一个活动道具；返回副本，避免在后续推进时借用池内对象。
redmist::Pickup SinglePickup(const redmist::DropSystem& drops) {
    redmist::Pickup result{};
    int count = 0;
    drops.ForEachActive([&](const redmist::Pickup& pickup) {
        result = pickup;
        ++count;
    });
    assert(count == 1);
    return result;
}

void TestPickupMotion() {
    using redmist::DropSystem;
    constexpr Vector3 distantPlayer{-5.0F, 0.0F, 0.0F};
    DropSystem drops(1234);
    drops.SpawnFixedPower({5.0F, 0.0F, 0.0F}, 8);
    assert(SinglePickup(drops).velocityZ == 4.0F);
    drops.Update(0.0F, distantPlayer);
    assert(SinglePickup(drops).position.z == 0.0F);
    assert(SinglePickup(drops).velocityZ == 4.0F);

    drops.Update(0.25F, distantPlayer);
    assert(std::abs(SinglePickup(drops).position.z - 0.75F) < 0.0001F);
    assert(std::abs(SinglePickup(drops).velocityZ - 2.0F) < 0.0001F);
    drops.Update(0.25F, distantPlayer);
    assert(std::abs(SinglePickup(drops).position.z - 1.0F) < 0.0001F);
    assert(std::abs(SinglePickup(drops).velocityZ) < 0.0001F);
    drops.Update(0.25F, distantPlayer);
    assert(std::abs(SinglePickup(drops).position.z - 0.75F) < 0.0001F);
    assert(std::abs(SinglePickup(drops).velocityZ + 2.0F) < 0.0001F);
    drops.Update(0.0625F, distantPlayer);
    assert(std::abs(SinglePickup(drops).position.z - 0.609375F) < 0.0001F);
    assert(std::abs(SinglePickup(drops).velocityZ + 2.5F) < 0.0001F);
    for (int frame = 0; frame < 2; ++frame) {
        const float previousZ = SinglePickup(drops).position.z;
        drops.Update(0.2F, distantPlayer);
        assert(std::abs(SinglePickup(drops).position.z - previousZ + 0.5F) < 0.0001F);
        assert(std::abs(SinglePickup(drops).velocityZ + 2.5F) < 0.0001F);
    }

    // 一个大帧同时跨过上抛顶点和限速点，与常见帧率下推进相同时间的结果应一致。
    drops.Clear();
    drops.SpawnFixedPower({5.0F, 0.0F, 0.0F}, 8);
    drops.Update(2.0F, distantPlayer);
    const auto largeStep = SinglePickup(drops);
    for (int fps : {30, 60, 144}) {
        drops.Clear();
        drops.SpawnFixedPower({5.0F, 0.0F, 0.0F}, 8);
        for (int frame = 0; frame < fps * 2; ++frame)
            drops.Update(1.0F / static_cast<float>(fps), distantPlayer);
        const auto smallSteps = SinglePickup(drops);
        assert(std::abs(smallSteps.position.z - largeStep.position.z) < 0.0001F);
        assert(std::abs(smallSteps.velocityZ - largeStep.velocityZ) < 0.0001F);
    }

    // 从已下落的状态立即转入吸取；拾取后复用槽位须恢复上抛而不继承追踪。
    assert(drops.Update(1.0F, {5.0F, 0.0F, redmist::kArenaHalfExtents.z * 0.6F}).power == 8);
    drops.SpawnFixedPower({5.0F, 0.0F, 0.0F}, 1);
    const auto reused = SinglePickup(drops);
    assert(!reused.homing && reused.velocityZ == 4.0F);
}

// 在场内交替移动玩家，让追踪持续超过旧超时时间而不提前拾取。
void KeepHomingPickupUncollected(redmist::DropSystem& drops) {
    for (int frame = 0; frame < 1600; ++frame) {
        const float x = frame % 2 == 0 ? -9.0F : 9.0F;
        const auto reward = drops.Update(0.01F, {x, 9.0F, 0.0F});
        assert(reward.power == 0 && reward.score == 0);
        assert(SinglePickup(drops).homing);
    }
}

void TestPickupBoundary() {
    using redmist::DropSystem;
    constexpr float rearZ = -redmist::kArenaHalfExtents.z;
    constexpr Vector3 distantPlayer{-5.0F, 0.0F, 0.0F};
    constexpr std::array<redmist::DropOutcome, 1> outcome{{{1, 1, 1}}};
    DropSystem drops(1234);
    // 1.25 秒位移为 -0.484375；P 点和分数点都恰好落在边界时仍保留。
    drops.SpawnEnemyDrops({5.0F, 0.0F, rearZ + 0.484375F}, outcome);
    drops.Update(1.25F, distantPlayer);
    int remaining = 0;
    drops.ForEachActive([&](const redmist::Pickup& pickup) {
        ++remaining;
        assert(pickup.position.z == rearZ);
        assert(pickup.velocityZ == -2.5F);
    });
    assert(remaining == 2);
    // 越界后即使仍在玩家拾取半径内，也必须回收而不结算奖励。
    const auto expired = drops.Update(0.01F, {5.0F, 0.0F, rearZ + 0.35F});
    assert(expired.power == 0 && expired.score == 0);
    drops.ForEachActive([](const redmist::Pickup&) { assert(false); });
    drops.SpawnFixedPower({5.0F, 0.0F, 0.0F}, 8);
    assert(!SinglePickup(drops).homing && SinglePickup(drops).velocityZ == 4.0F);
    // 一个大帧跨出后边界也应回收。
    assert(drops.Update(10.0F, distantPlayer).power == 0);
    drops.ForEachActive([](const redmist::Pickup&) { assert(false); });

    // 上抛超过 +Z 边界不提前消失；消失边界只在 -Z 端。
    drops.SpawnFixedPower({5.0F, 0.0F, redmist::kArenaHalfExtents.z - 0.2F}, 1);
    drops.Update(0.25F, distantPlayer);
    assert(SinglePickup(drops).position.z > redmist::kArenaHalfExtents.z);
    drops.Clear();

    drops.SpawnFixedPower({}, 8);
    drops.Update(0.0F, {0.0F, 0.0F, redmist::kArenaHalfExtents.z * 0.6F});
    KeepHomingPickupUncollected(drops);
    assert(drops.Update(1.0F, {}).power == 8);
}

void TestBulletStars() {
    using redmist::BulletOwner;
    using redmist::BulletSystem;
    using redmist::DropSystem;
    using redmist::Pickup;
    using redmist::PickupKind;
    // 弹幕池较大，置于堆上，避免 Windows 默认栈容量不足。
    auto bullets = std::make_unique<BulletSystem>();
    DropSystem drops(1234);
    constexpr Vector3 origin{5.0F, 0.0F, 0.0F};
    constexpr Vector3 player{-5.0F, 0.0F, 0.0F};
    auto* enemy = bullets->Spawn(BulletOwner::Enemy, origin, {}, 0.1F, WHITE, 0);
    assert(enemy);
    enemy->age = 14.0F;
    auto* playerBullet = bullets->Spawn(BulletOwner::Player, {}, {}, 0.1F, WHITE, 0);
    assert(playerBullet);
    auto* inactive = bullets->Spawn(BulletOwner::Enemy, {}, {}, 0.1F, WHITE, 0);
    assert(inactive);
    inactive->active = false;
    drops.ConvertEnemyBulletsToStars(*bullets);
    assert(!enemy->active && playerBullet->active);
    assert(bullets->Pool().ActiveCount() == 1);
    const auto star = SinglePickup(drops);
    assert(star.kind == PickupKind::Star && star.homing);
    assert(star.position.x == origin.x && star.position.z == origin.z);
    assert(star.value == DropSystem::kStarPointValue);
    // 转换瞬间不计分；即使玩家在自动吸取线之外，仍以 20 世界单位/秒追踪。
    const auto moving = drops.Update(0.1F, player);
    assert(moving.power == 0 && moving.score == 0);
    assert(std::abs(SinglePickup(drops).position.x - 3.0F) < 0.0001F);
    drops.ConvertEnemyBulletsToStars(*bullets);
    assert(SinglePickup(drops).kind == PickupKind::Star);
    const auto collected = drops.Update(1.0F, {-5.0F, 2.0F, -6.5F});
    assert(collected.power == 0 && collected.score == DropSystem::kStarPointValue);
    assert(drops.Update(1.0F, player).score == 0);

    // 普通道具池和弹幕池同时满时，所有敌弹仍须产生星点；普通 P 点保留。
    drops.Clear();
    bullets->Reset();
    for (std::size_t index = 0; index < DropSystem::kCapacity; ++index)
        drops.SpawnFixedPower(origin, 1);
    for (std::size_t index = 0; index < BulletSystem::kCapacity; ++index)
        assert(bullets->Spawn(BulletOwner::Enemy, origin, {}, 0.1F, WHITE, 0));
    drops.ConvertEnemyBulletsToStars(*bullets);
    assert(bullets->Pool().ActiveCount() == 0);
    int starCount = 0;
    int powerCount = 0;
    drops.ForEachActive([&](const Pickup& pickup) {
        if (pickup.kind == PickupKind::Star) ++starCount;
        if (pickup.kind == PickupKind::Power) ++powerCount;
    });
    assert(starCount == static_cast<int>(BulletSystem::kCapacity));
    assert(powerCount == static_cast<int>(DropSystem::kCapacity));
    // 再次清弹不能覆盖还在飞行的星点，新敌弹只有转换后才产生额外得分。
    assert(bullets->Spawn(BulletOwner::Enemy, origin, {}, 0.1F, WHITE, 0));
    drops.ConvertEnemyBulletsToStars(*bullets);
    const auto crowdedReward = drops.Update(1.0F, player);
    assert(crowdedReward.power == 0);
    assert(crowdedReward.score ==
           static_cast<long long>(BulletSystem::kCapacity + 1) * DropSystem::kStarPointValue);

    drops.Clear();
    assert(bullets->Spawn(BulletOwner::Enemy, origin, {}, 0.1F, WHITE, 0));
    drops.ConvertEnemyBulletsToStars(*bullets);
    drops.Clear();
    assert(drops.Update(1.0F, player).score == 0);
    assert(bullets->Spawn(BulletOwner::Enemy, origin, {}, 0.1F, WHITE, 0));
    drops.ConvertEnemyBulletsToStars(*bullets);
    KeepHomingPickupUncollected(drops);
    assert(drops.Update(1.0F, player).score == DropSystem::kStarPointValue);
    int remaining = 0;
    drops.ForEachActive([&](const Pickup&) { ++remaining; });
    assert(remaining == 0);
}
}  // namespace

/// @brief 验证普通道具掉落与运动、吸取状态持续性，以及清弹星点的转换和计分。
void RunDropTests() {
    TestPickupMotion();
    TestPickupBoundary();
    TestBulletStars();
    using redmist::DropOutcome;
    using redmist::DropSystem;
    using redmist::Pickup;
    using redmist::PickupKind;
    constexpr float autoCollectZ = redmist::kArenaHalfExtents.z * 0.6F;

    DropSystem drops(1234);
    constexpr std::array<DropOutcome, 2> outcomes{{
        {9, 9, 0}, {1, 2, 1},
    }};
    drops.SpawnEnemyDrops({}, outcomes);
    int powerCount = 0;
    int scoreCount = 0;
    std::array<Vector3, 3> originalPositions{};
    int itemIndex = 0;
    drops.ForEachActive([&](const Pickup& pickup) {
        assert(itemIndex < static_cast<int>(originalPositions.size()));
        originalPositions[itemIndex++] = pickup.position;
        if (pickup.kind == PickupKind::Power) {
            ++powerCount;
            assert(pickup.value == 1);
        } else {
            ++scoreCount;
            assert(pickup.value == DropSystem::kScorePointValue);
        }
        assert(pickup.velocityZ > 0.0F);
    });
    assert(powerCount == 1 && scoreCount == 2);
    assert(itemIndex == 3);
    // 旧吸取线 z=6 已移出前端五分之一区域，停在这里不能提前启动追踪。
    drops.Update(0.0F, {0.0F, 0.0F, 6.0F});
    drops.ForEachActive([](const Pickup& pickup) { assert(!pickup.homing); });
    const auto falling = drops.Update(1.0F, {0.0F, 0.0F, autoCollectZ - 0.01F});
    assert(falling.power == 0 && falling.score == 0);
    itemIndex = 0;
    drops.ForEachActive([&](const Pickup& pickup) {
        const Vector3 original = originalPositions[itemIndex++];
        assert(std::abs(pickup.position.x - original.x) < 0.0001F);
        assert(std::abs(pickup.position.y - original.y) < 0.0001F);
        assert(std::abs(pickup.position.z - (original.z + 0.140625F)) < 0.0001F);
        assert(std::abs(pickup.velocityZ + 2.5F) < 0.0001F);
    });
    assert(itemIndex == 3);
    const auto attracted = drops.Update(0.1F, {0.0F, 0.0F, autoCollectZ});
    assert(attracted.power == 0 && attracted.score == 0);
    itemIndex = 0;
    drops.ForEachActive([&](const Pickup& pickup) {
        assert(pickup.position.z > originalPositions[itemIndex++].z + 0.140625F);
        assert(pickup.position.z < autoCollectZ);
    });
    assert(itemIndex == 3);
    const auto reward = drops.Update(1.0F, {0.0F, 0.0F, autoCollectZ});
    assert(reward.power == 1);
    assert(reward.score == 2 * DropSystem::kScorePointValue);

    // 超出 X/Y 的掉落会被钳制；Z 新增区间中的掉落保留原深度。
    constexpr std::array<DropOutcome, 1> cornerOutcome{{{1, 0, 1}}};
    for (float sign : {-1.0F, 1.0F}) {
        drops.SpawnEnemyDrops({sign * 20.0F, sign * 20.0F, sign * 11.0F},
                              cornerOutcome);
        const Pickup corner = SinglePickup(drops);
        assert(std::abs(corner.position.x - sign * 9.8F) < 0.0001F);
        assert(std::abs(corner.position.y - sign * 9.8F) < 0.0001F);
        assert(std::abs(corner.position.z - sign * 11.0F) < 0.0001F);
        drops.Clear();
        drops.SpawnEnemyDrops({0.0F, 0.0F, sign * 20.0F}, cornerOutcome);
        assert(std::abs(SinglePickup(drops).position.z - sign * 11.3F) < 0.0001F);
        drops.Clear();
    }

    constexpr std::array<DropOutcome, 2> alternatives{{
        {1, 0, 1}, {0, 1, 1},
    }};
    bool sawPower = false;
    bool sawScore = false;
    for (int trial = 0; trial < 100; ++trial) {
        drops.SpawnEnemyDrops({}, alternatives);
        const auto sampled = drops.Update(1.0F, {0.0F, 0.0F, autoCollectZ});
        assert((sampled.power == 1 && sampled.score == 0) ||
               (sampled.power == 0 &&
                sampled.score == DropSystem::kScorePointValue));
        sawPower |= sampled.power == 1;
        sawScore |= sampled.score > 0;
    }
    assert(sawPower && sawScore);

    drops.SpawnEnemyDrops({}, {});
    assert(drops.Update(0.0F, {}).power == 0);
    drops.SpawnFixedPower({}, 8);
    assert(drops.Update(0.0F, {}).power == 8);
    drops.SpawnFixedPower({5.0F, 0.0F, 0.0F}, 1);
    drops.Clear();
    assert(drops.Update(0.0F, {}).power == 0);

    DropSystem latchedDrops(4321);
    latchedDrops.SpawnFixedPower({}, 1);
    latchedDrops.Update(0.1F, {0.0F, 0.0F, autoCollectZ});
    const auto afterLeaving = latchedDrops.Update(0.05F, {3.0F, 0.0F, 0.0F});
    assert(afterLeaving.power == 0);
    int homingCount = 0;
    latchedDrops.ForEachActive([&](const Pickup& pickup) {
        ++homingCount;
        assert(pickup.homing);
        assert(pickup.position.x > 0.0F);
    });
    assert(homingCount == 1);

    // 离开触发区域后新生成的道具仍从上抛开始，不继承旧道具的追踪状态。
    latchedDrops.SpawnFixedPower({5.0F, 0.0F, 0.0F}, 8);
    latchedDrops.Update(0.1F, {3.0F, 0.0F, 0.0F});
    int freshCount = 0;
    latchedDrops.ForEachActive([&](const Pickup& pickup) {
        if (pickup.value != 8) return;
        ++freshCount;
        assert(!pickup.homing);
        assert(pickup.position.x == 5.0F);
        assert(std::abs(pickup.position.z - 0.36F) < 0.0001F);
        assert(std::abs(pickup.velocityZ - 3.2F) < 0.0001F);
    });
    assert(freshCount == 1);
}
