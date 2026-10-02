// 本文件负责验证敌人掉落表抽取、上抛与加速下落、自动吸取与拾取结算。

#include "gameplay/arena.hpp"
#include "gameplay/items/drop_system.hpp"

#include <array>
#include <cassert>
#include <cmath>

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
    assert(drops.Update(1.0F, {5.0F, 0.0F, 6.0F}).power == 8);
    drops.SpawnFixedPower({5.0F, 0.0F, 0.0F}, 1);
    const auto reused = SinglePickup(drops);
    assert(!reused.homing && reused.age == 0.0F && reused.velocityZ == 4.0F);
}
}  // namespace

/// @brief 验证两类道具按权重生成、上抛与限速下落，以及自动吸取状态的持续性。
void RunDropTests() {
    TestPickupMotion();
    using redmist::DropOutcome;
    using redmist::DropSystem;
    using redmist::Pickup;
    using redmist::PickupKind;
    constexpr float autoCollectZ = redmist::kArenaHalfExtent * 0.6F;

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
