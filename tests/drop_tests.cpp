// 本文件负责验证敌人掉落表抽取、两类道具生成与拾取结算。

#include "gameplay/items/drop_system.hpp"

#include <array>
#include <cassert>

/// @brief 验证生成器按权重结果生成 P 点和分数点，并汇总拾取奖励。
void RunDropTests() {
    using redmist::DropOutcome;
    using redmist::DropSystem;
    using redmist::Pickup;
    using redmist::PickupKind;

    DropSystem drops(1234);
    constexpr std::array<DropOutcome, 2> outcomes{{
        {9, 9, 0}, {1, 2, 1},
    }};
    drops.SpawnEnemyDrops({}, outcomes);
    int powerCount = 0;
    int scoreCount = 0;
    drops.ForEachActive([&](const Pickup& pickup) {
        if (pickup.kind == PickupKind::Power) {
            ++powerCount;
            assert(pickup.value == 1);
        } else {
            ++scoreCount;
            assert(pickup.value == DropSystem::kScorePointValue);
        }
    });
    assert(powerCount == 1 && scoreCount == 2);
    const auto reward = drops.Update(1.0F, {});
    assert(reward.power == 1);
    assert(reward.score == 2 * DropSystem::kScorePointValue);

    constexpr std::array<DropOutcome, 2> alternatives{{
        {1, 0, 1}, {0, 1, 1},
    }};
    bool sawPower = false;
    bool sawScore = false;
    for (int trial = 0; trial < 100; ++trial) {
        drops.SpawnEnemyDrops({}, alternatives);
        const auto sampled = drops.Update(1.0F, {});
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
}
