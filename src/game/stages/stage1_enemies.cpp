#include "game/stages/stage1_enemies.hpp"
#include "game/stages/stage1_ids.hpp"

#include "game/bullet_patterns.hpp"

#include <cmath>

namespace redmist::stage1 {
namespace {
constexpr Color kEnemyRed{255, 70, 105, 255};
constexpr Color kEnemyBlue{75, 145, 255, 255};
constexpr Color kEnemyPurple{205, 95, 255, 255};
}

Stage1Enemy::Stage1Enemy(VisualId visual, Path path, Vector3 start, float delay)
    : Enemy(visual, start, delay), path_(path) {}

void Stage1Enemy::Move() {
    switch (path_) {
        case Path::Straight:
            position = pattern::Add(origin, {0.0F, 0.0F, -speed * age});
            break;
        case Path::Helix: {
            const float angle = age * 2.1F + origin.x * 0.3F;
            position = {origin.x + std::cos(angle) * 1.25F,
                        origin.y + std::sin(angle) * 1.25F,
                        origin.z - speed * age};
            break;
        }
        case Path::SweepLeft:
            position = {origin.x + speed * 0.9F * age,
                        origin.y + std::sin(age * 1.7F) * 0.7F,
                        origin.z - speed * 0.5F * age};
            break;
        case Path::SweepRight:
            position = {origin.x - speed * 0.9F * age,
                        origin.y + std::cos(age * 1.7F) * 0.7F,
                        origin.z - speed * 0.5F * age};
            break;
    }

}

FairyEnemy::FairyEnemy(Path path, Vector3 start, float delay)
    : Stage1Enemy(kFairy, path, start, delay) {
    health = maxHealth = 10.0F;
    speed = 1.5F;
    fireInterval = 1.8F;
    scoreValue = 500;
}

void FairyEnemy::UseSkill(BulletSystem& bullets, Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 direction = BulletSystem::DirectionTo(muzzle, playerPosition);
    bullets.Spawn(BulletOwner::Enemy, muzzle, pattern::Scale(direction, 3.3F),
                  0.16F, kEnemyRed, kRedBullet);
}

DarkFairyEnemy::DarkFairyEnemy(Path path, Vector3 start, float delay)
    : Stage1Enemy(kDarkFairy, path, start, delay) {
    health = maxHealth = 12.0F;
    speed = 1.7F;
    fireInterval = 1.55F;
    scoreValue = 650;
}

void DarkFairyEnemy::UseSkill(BulletSystem& bullets, Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 direction = BulletSystem::DirectionTo(muzzle, playerPosition);
    bullets.Spawn(BulletOwner::Enemy, muzzle, pattern::Scale(direction, 3.5F),
                  0.17F, kEnemyRed, kRedBullet);
}

OrbEnemy::OrbEnemy(Path path, Vector3 start, float delay)
    : Stage1Enemy(kOrb, path, start, delay) {
    health = maxHealth = 18.0F;
    speed = 1.15F;
    fireInterval = 1.5F;
    scoreValue = 900;
    hitRadius = 0.62F;
}

void OrbEnemy::UseSkill(BulletSystem& bullets, Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 axis = BulletSystem::DirectionTo(muzzle, playerPosition);
    pattern::ForEachConeDirection(axis, 1, 5, 0.24F, age * 0.2F,
        [muzzle, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, muzzle,
                          pattern::Scale(direction, 3.0F), 0.16F, kEnemyBlue,
                          kBlueBullet);
        });
}

EliteEnemy::EliteEnemy(Path path, Vector3 start, float delay)
    : Stage1Enemy(kElite, path, start, delay) {
    health = maxHealth = 135.0F;
    speed = 0.28F;
    fireInterval = 1.15F;
    scoreValue = 5000;
    hitRadius = 0.78F;
}

void EliteEnemy::UseSkill(BulletSystem& bullets, Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    pattern::ForEachSphereDirection(34, age * 0.35F,
        [muzzle, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, muzzle,
                          pattern::Scale(direction, 2.45F), 0.16F,
                          kEnemyPurple, kBlueBullet);
        });
    const Vector3 axis = BulletSystem::DirectionTo(muzzle, playerPosition);
    pattern::ForEachConeDirection(axis, 1, 7, 0.32F, age,
        [muzzle, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, muzzle,
                          pattern::Scale(direction, 3.5F), 0.16F, kEnemyRed,
                          kRedBullet);
        });
}

}  // namespace redmist::stage1
