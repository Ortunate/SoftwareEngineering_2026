#pragma once

#include "game/bullet_system.hpp"
#include "game/game_types.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <utility>

namespace redmist {

class Enemy {
public:
    virtual ~Enemy() = default;
    virtual void Update(float deltaTime, Vector3 playerPosition,
                        BulletSystem& bullets);

    bool active = true;
    EnemyKind kind;
    PathKind path;
    Vector3 position;
    Vector3 origin;
    float age;
    float health = 10.0F;
    float maxHealth = 10.0F;
    float hitRadius = 0.55F;
    float speed = 1.0F;
    float fireTimer = 0.0F;
    float fireInterval = 1.5F;
    int scoreValue = 500;

    [[nodiscard]] Vector3 MuzzlePosition(Vector3 playerPosition) const;

protected:
    Enemy(EnemyKind kind, PathKind path, Vector3 position, float delay);
    virtual void UseSkill(BulletSystem& bullets, Vector3 playerPosition) = 0;
};

class FairyEnemy final : public Enemy {
public:
    FairyEnemy(PathKind path, Vector3 position, float delay = 0.0F);

private:
    void UseSkill(BulletSystem& bullets, Vector3 playerPosition) override;
};

class DarkFairyEnemy final : public Enemy {
public:
    DarkFairyEnemy(PathKind path, Vector3 position, float delay = 0.0F);

private:
    void UseSkill(BulletSystem& bullets, Vector3 playerPosition) override;
};

class OrbEnemy final : public Enemy {
public:
    OrbEnemy(PathKind path, Vector3 position, float delay = 0.0F);

private:
    void UseSkill(BulletSystem& bullets, Vector3 playerPosition) override;
};

class EliteEnemy final : public Enemy {
public:
    EliteEnemy(PathKind path, Vector3 position, float delay = 0.0F);

private:
    void UseSkill(BulletSystem& bullets, Vector3 playerPosition) override;
};

template <std::size_t Capacity>
class EnemyPool {
public:
    bool Add(std::unique_ptr<Enemy> enemy) {
        for (auto& slot : enemies_) {
            if (!slot || !slot->active) {
                slot = std::move(enemy);
                return true;
            }
        }
        ++droppedSpawns_;
        return false;
    }

    template <typename Function>
    void ForEachActive(Function&& function) {
        for (auto& enemy : enemies_) {
            if (enemy && enemy->active) function(*enemy);
        }
    }

    template <typename Function>
    void ForEachActive(Function&& function) const {
        for (const auto& enemy : enemies_) {
            if (enemy && enemy->active)
                function(static_cast<const Enemy&>(*enemy));
        }
    }

    void Clear() {
        for (auto& enemy : enemies_) enemy.reset();
        droppedSpawns_ = 0;
    }

    [[nodiscard]] std::size_t DroppedSpawns() const { return droppedSpawns_; }

private:
    std::array<std::unique_ptr<Enemy>, Capacity> enemies_{};
    std::size_t droppedSpawns_ = 0;
};

}  // namespace redmist
