// 本文件负责按敌人概率表生成 P 点与分数点，并处理道具移动和拾取。

#include "gameplay/items/drop_system.hpp"

#include "gameplay/arena.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace redmist {
namespace {
constexpr int kMaxCountPerKind = 32;
constexpr float kLifetime = 15.0F;
constexpr float kFallSpeed = 2.5F;
constexpr float kAutoCollectSpeed = 20.0F;
constexpr float kCollectDistance = 0.6F;
// 战场 Z 范围为 [-halfExtent, +halfExtent]，靠近 +Z 边界的五分之一区域从 +0.6 倍半径开始。
constexpr float kAutoCollectZ = kArenaHalfExtent * 0.6F;
constexpr float kSpiralAngle = 2.3999632F;
}

DropSystem::DropSystem() : DropSystem(std::random_device{}()) {}

DropSystem::DropSystem(std::uint32_t seed) : random_(seed) {}

void DropSystem::Clear() { pickups_.Clear(); }

void DropSystem::Spawn(PickupKind kind, Vector3 position, int value) {
    if (value <= 0) return;
    if (Pickup* pickup = pickups_.Acquire()) {
        pickup->kind = kind;
        pickup->position = position;
        pickup->value = value;
    }
}

void DropSystem::SpawnEnemyDrops(Vector3 position,
                                 std::span<const DropOutcome> distribution) {
    std::uint64_t totalWeight = 0;
    for (const DropOutcome& outcome : distribution)
        totalWeight += outcome.weight;
    if (totalWeight == 0) return;

    std::uniform_int_distribution<std::uint64_t> select(0, totalWeight - 1);
    std::uint64_t choice = select(random_);
    const DropOutcome* selected = nullptr;
    for (const DropOutcome& outcome : distribution) {
        if (choice < outcome.weight) {
            selected = &outcome;
            break;
        }
        choice -= outcome.weight;
    }
    if (!selected) return;

    const int powerCount = std::clamp(selected->powerCount, 0, kMaxCountPerKind);
    const int scoreCount = std::clamp(selected->scoreCount, 0, kMaxCountPerKind);
    if (powerCount + scoreCount == 0) return;
    std::uniform_real_distribution<float> phaseDistribution(
        0.0F, 2.0F * std::numbers::pi_v<float>);
    const float phase = phaseDistribution(random_);
    // 同时掉落的道具按黄金角错开，避免贴图完全重叠；位移保持在敌人附近。
    for (int index = 0; index < powerCount + scoreCount; ++index) {
        const float angle = phase + static_cast<float>(index) * kSpiralAngle;
        const float radius = 0.22F + 0.11F * std::sqrt(static_cast<float>(index));
        Vector3 itemPosition{
            position.x + std::cos(angle) * radius,
            position.y + std::sin(angle) * radius,
            position.z,
        };
        constexpr float limit = kArenaHalfExtent - 0.2F;
        itemPosition.x = std::clamp(itemPosition.x, -limit, limit);
        itemPosition.y = std::clamp(itemPosition.y, -limit, limit);
        itemPosition.z = std::clamp(itemPosition.z, -limit, limit);
        if (index < powerCount)
            Spawn(PickupKind::Power, itemPosition, 1);
        else
            Spawn(PickupKind::Score, itemPosition, kScorePointValue);
    }
}

void DropSystem::SpawnFixedPower(Vector3 position, int power) {
    Spawn(PickupKind::Power, position, power);
}

PickupReward DropSystem::Update(float deltaTime, Vector3 playerPosition) {
    PickupReward reward{};
    pickups_.ForEachActive([&](Pickup& pickup) {
        pickup.age += deltaTime;
        if (pickup.age > kLifetime) {
            pickup.active = false;
            return;
        }
        // 吸取状态属于单个道具；玩家离开触发区域不会取消已有道具的追踪。
        if (playerPosition.z >= kAutoCollectZ) pickup.homing = true;
        if (pickup.homing) {
            const Vector3 toPlayer{
                playerPosition.x - pickup.position.x,
                playerPosition.y - pickup.position.y,
                playerPosition.z - pickup.position.z,
            };
            const float distance = std::sqrt(toPlayer.x * toPlayer.x +
                                             toPlayer.y * toPlayer.y +
                                             toPlayer.z * toPlayer.z);
            if (distance > 0.0001F) {
                // 限制单帧位移，避免越过玩家后反复折返。
                const float ratio = std::min(kAutoCollectSpeed * deltaTime / distance, 1.0F);
                pickup.position.x += toPlayer.x * ratio;
                pickup.position.y += toPlayer.y * ratio;
                pickup.position.z += toPlayer.z * ratio;
            }
        } else {
            pickup.position.z -= kFallSpeed * deltaTime;
        }
        const float dx = playerPosition.x - pickup.position.x;
        const float dy = playerPosition.y - pickup.position.y;
        const float dz = playerPosition.z - pickup.position.z;
        if (dx * dx + dy * dy + dz * dz < kCollectDistance * kCollectDistance) {
            if (pickup.kind == PickupKind::Power)
                reward.power += pickup.value;
            else
                reward.score += pickup.value;
            pickup.active = false;
        }
    });
    return reward;
}

}  // namespace redmist
