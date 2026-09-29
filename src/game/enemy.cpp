#include "game/enemy.hpp"

#include "game/bullet_patterns.hpp"

#include <cmath>

namespace redmist {

Enemy::Enemy(VisualId appearance, Vector3 start, float delay)
    : visual(appearance), position(start), origin(start), age(-delay),
      fireTimer(0.8F + delay) {}

Vector3 Enemy::MuzzlePosition(Vector3 playerPosition) const {
    return pattern::Add(position,
        pattern::Scale(BulletSystem::DirectionTo(position, playerPosition),
                       hitRadius * 0.55F));
}

void Enemy::Update(float deltaTime, Vector3 playerPosition,
                   BulletSystem& bullets) {
    age += deltaTime;
    if (age < 0.0F) return;
    Move();
    fireTimer -= deltaTime;
    if (age > 0.55F && fireTimer <= 0.0F) {
        UseSkill(bullets, playerPosition);
        fireTimer = fireInterval;
    }
    constexpr float outside = kArenaHalfExtent + 2.0F;
    if (std::abs(position.x) > outside || std::abs(position.y) > outside ||
        std::abs(position.z) > outside) active = false;
}

}  // namespace redmist
