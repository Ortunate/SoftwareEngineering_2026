// 本文件负责敌人掉落和清弹星点生成，并统一处理道具移动和拾取。

#include "gameplay/items/drop_system.hpp"

#include "gameplay/arena.hpp"
#include "gameplay/bullets/system.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace redmist {
namespace {
constexpr int kMaxCountPerKind = 32;
constexpr float kLifetime = 15.0F;
// 速度单位为世界单位/秒，加速度为世界单位/秒²；0.5 秒到达比生成点高 1 单位的顶点。
constexpr float kLaunchSpeed = 4.0F;
constexpr float kFallAcceleration = 8.0F;
constexpr float kFallSpeed = 2.5F;
constexpr float kAutoCollectSpeed = 20.0F;
constexpr float kCollectDistance = 0.6F;
// 战场 Z 范围为 [-kArenaHalfExtents.z, +kArenaHalfExtents.z]，靠近 +Z 边界的五分之一区域从 +0.6 倍半径开始。
constexpr float kAutoCollectZ = kArenaHalfExtents.z * 0.6F;
constexpr float kSpiralAngle = 2.3999632F;
}

DropSystem::DropSystem() : DropSystem(std::random_device{}()) {}

DropSystem::DropSystem(std::uint32_t seed) : random_(seed) {}

void DropSystem::Clear() {
    pickups_.Clear();
    stars_.clear();
}

void DropSystem::Spawn(PickupKind kind, Vector3 position, int value) {
    if (value <= 0) return;
    if (Pickup* pickup = pickups_.Acquire()) {
        pickup->kind = kind;
        pickup->position = position;
        pickup->value = value;
        pickup->velocityZ = kLaunchSpeed;
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
        constexpr Vector3 limit{kArenaHalfExtents.x - 0.2F,
                                kArenaHalfExtents.y - 0.2F,
                                kArenaHalfExtents.z - 0.2F};
        itemPosition.x = std::clamp(itemPosition.x, -limit.x, limit.x);
        itemPosition.y = std::clamp(itemPosition.y, -limit.y, limit.y);
        itemPosition.z = std::clamp(itemPosition.z, -limit.z, limit.z);
        if (index < powerCount)
            Spawn(PickupKind::Power, itemPosition, 1);
        else
            Spawn(PickupKind::Score, itemPosition, kScorePointValue);
    }
}

void DropSystem::SpawnFixedPower(Vector3 position, int power) {
    Spawn(PickupKind::Power, position, power);
}

void DropSystem::ConvertEnemyBulletsToStars(BulletSystem& bullets) {
    // 先预留空间，再停用敌弹，避免生成中途扩容；连续升满时保留上一批尚未吸收的星点。
    stars_.reserve(stars_.size() + bullets.Pool().ActiveCount());
    bullets.Pool().ForEachActive([this](Bullet& bullet) {
        if (bullet.owner != BulletOwner::Enemy) return;
        Pickup star{};
        star.active = true;
        star.homing = true;
        star.kind = PickupKind::Star;
        star.position = bullet.position;
        star.value = kStarPointValue;
        stars_.push_back(star);
        bullet.active = false;
    });
}

PickupReward DropSystem::Update(float deltaTime, Vector3 playerPosition) {
    PickupReward reward{};
    const auto updatePickup = [&](Pickup& pickup) {
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
            // 按达到速度上限的时刻拆分本帧，并精确积分加速段；跨越顶点或限速点时，
            // 位移仍须与分成多个小帧的结果一致，不能用帧末速度代替整帧速度。
            const float accelerationTime = std::min(
                deltaTime, (pickup.velocityZ + kFallSpeed) / kFallAcceleration);
            pickup.position.z += pickup.velocityZ * accelerationTime -
                                 0.5F * kFallAcceleration * accelerationTime * accelerationTime -
                                 kFallSpeed * (deltaTime - accelerationTime);
            pickup.velocityZ = std::max(
                -kFallSpeed, pickup.velocityZ - kFallAcceleration * accelerationTime);
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
    };
    pickups_.ForEachActive(updatePickup);
    for (Pickup& star : stars_) updatePickup(star);
    std::erase_if(stars_, [](const Pickup& star) { return !star.active; });
    return reward;
}

}  // namespace redmist
