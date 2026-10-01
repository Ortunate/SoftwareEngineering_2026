// 本文件负责验证敌人掉落表抽取、道具下落、自动吸取与拾取结算。

#include "gameplay/arena.hpp"
#include "gameplay/items/drop_system.hpp"

#include <array>
#include <cassert>
#include <cmath>

/// @brief 验证两类道具按权重生成、沿 -Z 下落，以及自动吸取状态的持续性。
void RunDropTests() {
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
        assert(std::abs(pickup.position.z - (original.z - 2.5F)) < 0.0001F);
    });
    assert(itemIndex == 3);
    const auto attracted = drops.Update(0.1F, {0.0F, 0.0F, autoCollectZ});
    assert(attracted.power == 0 && attracted.score == 0);
    itemIndex = 0;
    drops.ForEachActive([&](const Pickup& pickup) {
        assert(pickup.position.z > originalPositions[itemIndex++].z - 2.5F);
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

    // 离开触发区域后新生成的道具仍应下落，不继承旧道具的追踪状态。
    latchedDrops.SpawnFixedPower({5.0F, 0.0F, 0.0F}, 8);
    latchedDrops.Update(0.1F, {3.0F, 0.0F, 0.0F});
    int freshCount = 0;
    latchedDrops.ForEachActive([&](const Pickup& pickup) {
        if (pickup.value != 8) return;
        ++freshCount;
        assert(!pickup.homing);
        assert(pickup.position.x == 5.0F);
        assert(std::abs(pickup.position.z + 0.25F) < 0.0001F);
    });
    assert(freshCount == 1);
}
