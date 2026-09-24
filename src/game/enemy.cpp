#include "game/enemy.hpp"

#include "game/bullet_patterns.hpp"

#include <cmath>

namespace redmist {
namespace {
constexpr Color kEnemyRed{255, 70, 105, 255};
constexpr Color kEnemyBlue{75, 145, 255, 255};
constexpr Color kEnemyPurple{205, 95, 255, 255};
}

Enemy::Enemy(EnemyKind enemyKind, PathKind enemyPath, Vector3 start, float delay)
    : kind(enemyKind), path(enemyPath), position(start), origin(start),
      age(-delay), fireTimer(0.8F + delay) {}

void Enemy::Update(float deltaTime, Vector3 playerPosition, BulletSystem& bullets) {
    age += deltaTime;
    if (age < 0.0F) return;

    switch (path) {
        case PathKind::Straight:
            position = pattern::Add(origin, {0.0F, 0.0F, -speed * age});
            break;
        case PathKind::Helix: {
            const float angle = age * 2.1F + origin.x * 0.3F;
            position = {origin.x + std::cos(angle) * 1.25F,
                        origin.y + std::sin(angle) * 1.25F,
                        origin.z - speed * age};
            break;
        }
        case PathKind::SweepLeft:
            position = {origin.x + speed * 0.9F * age,
                        origin.y + std::sin(age * 1.7F) * 0.7F,
                        origin.z - speed * 0.5F * age};
            break;
        case PathKind::SweepRight:
            position = {origin.x - speed * 0.9F * age,
                        origin.y + std::cos(age * 1.7F) * 0.7F,
                        origin.z - speed * 0.5F * age};
            break;
    }

    fireTimer -= deltaTime;
    if (age > 0.55F && fireTimer <= 0.0F) {
        UseSkill(bullets, playerPosition);
        fireTimer = fireInterval;
    }

    constexpr float outside = kArenaHalfExtent + 2.0F;
    if (std::abs(position.x) > outside || std::abs(position.y) > outside ||
        std::abs(position.z) > outside) active = false;
}

FairyEnemy::FairyEnemy(PathKind path, Vector3 start, float delay)
    : Enemy(EnemyKind::Fairy, path, start, delay) {
    health = maxHealth = 10.0F;
    speed = 1.5F;
    fireInterval = 1.8F;
    scoreValue = 500;
}

void FairyEnemy::UseSkill(BulletSystem& bullets, Vector3 playerPosition) {
    const Vector3 direction = BulletSystem::DirectionTo(position, playerPosition);
    bullets.Spawn(BulletOwner::Enemy, position, pattern::Scale(direction, 3.3F),
                  0.16F, kEnemyRed);
}

OrbEnemy::OrbEnemy(PathKind path, Vector3 start, float delay)
    : Enemy(EnemyKind::Orb, path, start, delay) {
    health = maxHealth = 18.0F;
    speed = 1.15F;
    fireInterval = 1.5F;
    scoreValue = 900;
}

void OrbEnemy::UseSkill(BulletSystem& bullets, Vector3 playerPosition) {
    const Vector3 axis = BulletSystem::DirectionTo(position, playerPosition);
    pattern::ForEachConeDirection(axis, 1, 5, 0.24F, age * 0.2F,
        [this, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, position,
                          pattern::Scale(direction, 3.0F), 0.16F, kEnemyBlue);
        });
}

EliteEnemy::EliteEnemy(PathKind path, Vector3 start, float delay)
    : Enemy(EnemyKind::Elite, path, start, delay) {
    health = maxHealth = 135.0F;
    speed = 0.28F;
    fireInterval = 1.15F;
    scoreValue = 5000;
}

void EliteEnemy::UseSkill(BulletSystem& bullets, Vector3 playerPosition) {
    pattern::ForEachSphereDirection(34, age * 0.35F,
        [this, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, position,
                          pattern::Scale(direction, 2.45F), 0.16F,
                          kEnemyPurple);
        });
    const Vector3 axis = BulletSystem::DirectionTo(position, playerPosition);
    pattern::ForEachConeDirection(axis, 1, 7, 0.32F, age,
        [this, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, position,
                          pattern::Scale(direction, 3.5F), 0.16F, kEnemyRed);
        });
}

}  // namespace redmist
