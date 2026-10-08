// 本文件负责实现敌人入场计时、边界检查和发弹点计算，并调用子类行为。

#include "gameplay/arena.hpp"
#include "gameplay/actors/enemy.hpp"

#include "gameplay/bullets/patterns.hpp"

#include <cmath>

namespace redmist {

Enemy::Enemy(VisualId appearance, Vector3 start, float delay,
             std::span<const DropOutcome> drops)
    : visual(appearance), position(start), origin(start), age(-delay),
      dropDistribution_(drops) {}

Vector3 Enemy::MuzzlePosition(Vector3 playerPosition) const {
    return pattern::Add(position,
        pattern::Scale(BulletSystem::DirectionTo(position, playerPosition),
                       hitRadius * 0.55F));
}

void Enemy::Update(float deltaTime, Vector3 playerPosition,
                   BulletSystem& bullets) {
    age += deltaTime;
    if (age < 0.0F) return;
    Move(deltaTime);
    UpdateSkills(deltaTime, playerPosition, bullets);
    constexpr float margin = 2.0F;
    if (std::abs(position.x) > kArenaHalfExtents.x + margin ||
        std::abs(position.y) > kArenaHalfExtents.y + margin ||
        std::abs(position.z) > kArenaHalfExtents.z + margin) active = false;
}

}  // namespace redmist
