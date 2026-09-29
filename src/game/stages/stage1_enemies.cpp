#include "game/stages/stage1_enemies.hpp"

#include "game/bullet_patterns.hpp"

#include <cmath>

namespace redmist::stage1 {
namespace {
constexpr Color kEnemyRed{255, 70, 105, 255};
constexpr Color kEnemyBlue{75, 145, 255, 255};

/// @brief 仅封装倒计时运算；每种技能的计时器由具体敌人自己持有。
bool SkillReady(float& timer, float interval, float deltaTime, float age) {
    timer -= deltaTime;
    if (age <= 0.55F || timer > 0.0F) return false;
    timer = interval;
    return true;
}

Formation SpawnFormation(const EnemySpawn& spawn) {
    return static_cast<Formation>(spawn.formation);
}
}  // namespace

FairyEnemy::FairyEnemy(const EnemySpawn& spawn)
    : Enemy(kFairy, spawn.position, spawn.delay),
      movement_(SpawnFormation(spawn) == Formation::CrossingPair
                    ? (spawn.position.x < 0.0F ? Movement::CrossFromLeft
                                               : Movement::CrossFromRight)
                    : (SpawnFormation(spawn) == Formation::Star ||
                       (spawn.slot % 2 != 0) ? Movement::Helix
                                            : Movement::Straight)),
      shotTimer_(0.8F + spawn.delay) {
    health = maxHealth = 10.0F;
    scoreValue = 500;
}

void FairyEnemy::Move(float) {
    constexpr float speed = 1.5F;
    switch (movement_) {
        case Movement::Straight:
            position = {origin.x, origin.y, origin.z - speed * age};
            break;
        case Movement::Helix: {
            const float angle = age * 2.1F + origin.x * 0.3F;
            position = {origin.x + std::cos(angle) * 1.25F,
                        origin.y + std::sin(angle) * 1.25F,
                        origin.z - speed * age};
            break;
        }
        case Movement::CrossFromLeft:
            position = {origin.x + speed * 0.9F * age,
                        origin.y + std::sin(age * 1.7F) * 0.7F,
                        origin.z - speed * 0.5F * age};
            break;
        case Movement::CrossFromRight:
            position = {origin.x - speed * 0.9F * age,
                        origin.y + std::cos(age * 1.7F) * 0.7F,
                        origin.z - speed * 0.5F * age};
            break;
    }
}

void FairyEnemy::UpdateSkills(float deltaTime, Vector3 playerPosition,
                              BulletSystem& bullets) {
    if (SkillReady(shotTimer_, 1.8F, deltaTime, age))
        FireAimedShot(bullets, playerPosition);
}

void FairyEnemy::FireAimedShot(BulletSystem& bullets, Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 direction = BulletSystem::DirectionTo(muzzle, playerPosition);
    bullets.Spawn(BulletOwner::Enemy, muzzle, pattern::Scale(direction, 3.3F),
                  0.16F, kEnemyRed, kRedBullet);
}

DarkFairyEnemy::DarkFairyEnemy(const EnemySpawn& spawn)
    : Enemy(kDarkFairy, spawn.position, spawn.delay),
      movement_(SpawnFormation(spawn) == Formation::CrossingPair
                    ? (spawn.position.x < 0.0F ? Movement::CrossFromLeft
                                               : Movement::CrossFromRight)
                    : (SpawnFormation(spawn) == Formation::Star ||
                       (spawn.slot % 2 != 0) ? Movement::Corkscrew
                                            : Movement::Drift)),
      shotTimer_(0.8F + spawn.delay) {
    health = maxHealth = 12.0F;
    scoreValue = 650;
}

void DarkFairyEnemy::Move(float) {
    constexpr float speed = 1.7F;
    switch (movement_) {
        case Movement::Drift:
            position = {origin.x + std::sin(age * 1.4F) * 0.3F,
                        origin.y + std::sin(age * 1.8F) * 0.35F,
                        origin.z - speed * age};
            break;
        case Movement::Corkscrew: {
            const float angle = age * 2.5F + origin.x * 0.35F;
            position = {origin.x + std::cos(angle) * 1.15F,
                        origin.y + std::sin(angle) * 1.45F,
                        origin.z - speed * age};
            break;
        }
        case Movement::CrossFromLeft:
            position = {origin.x + speed * age,
                        origin.y + std::sin(age * 2.0F) * 0.6F,
                        origin.z - speed * 0.55F * age};
            break;
        case Movement::CrossFromRight:
            position = {origin.x - speed * age,
                        origin.y + std::cos(age * 2.0F) * 0.6F,
                        origin.z - speed * 0.55F * age};
            break;
    }
}

void DarkFairyEnemy::UpdateSkills(float deltaTime, Vector3 playerPosition,
                                  BulletSystem& bullets) {
    if (SkillReady(shotTimer_, 1.55F, deltaTime, age))
        FireAimedShot(bullets, playerPosition);
}

void DarkFairyEnemy::FireAimedShot(BulletSystem& bullets,
                                   Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 direction = BulletSystem::DirectionTo(muzzle, playerPosition);
    bullets.Spawn(BulletOwner::Enemy, muzzle, pattern::Scale(direction, 3.5F),
                  0.17F, kEnemyRed, kRedBullet);
}

OrbEnemy::OrbEnemy(const EnemySpawn& spawn)
    : Enemy(kOrb, spawn.position, spawn.delay),
      movement_(SpawnFormation(spawn) == Formation::CrossingPair
                    ? Movement::CrossArena
                    : (SpawnFormation(spawn) == Formation::Star
                           ? Movement::OrbitForward : Movement::FloatForward)),
      shotTimer_(0.8F + spawn.delay) {
    health = maxHealth = 18.0F;
    scoreValue = 900;
    hitRadius = 0.62F;
}

void OrbEnemy::Move(float) {
    constexpr float speed = 1.15F;
    switch (movement_) {
        case Movement::FloatForward:
            position = {origin.x, origin.y + std::sin(age * 1.6F) * 0.45F,
                        origin.z - speed * age};
            break;
        case Movement::OrbitForward:
            position = {origin.x + std::cos(age * 1.3F) * 0.8F,
                        origin.y + std::sin(age * 1.3F) * 0.8F,
                        origin.z - speed * age};
            break;
        case Movement::CrossArena: {
            const float side = origin.x < 0.0F ? 1.0F : -1.0F;
            position = {origin.x + side * speed * age,
                        origin.y + std::sin(age * 1.6F) * 0.45F,
                        origin.z - speed * 0.45F * age};
            break;
        }
    }
}

void OrbEnemy::UpdateSkills(float deltaTime, Vector3 playerPosition,
                            BulletSystem& bullets) {
    if (SkillReady(shotTimer_, 1.5F, deltaTime, age))
        FireCone(bullets, playerPosition);
}

void OrbEnemy::FireCone(BulletSystem& bullets, Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 axis = BulletSystem::DirectionTo(muzzle, playerPosition);
    pattern::ForEachConeDirection(axis, 1, 5, 0.24F, age * 0.2F,
        [muzzle, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, muzzle,
                          pattern::Scale(direction, 3.0F), 0.16F, kEnemyBlue,
                          kBlueBullet);
        });
}

}  // namespace redmist::stage1
