// 本文件负责敌我碰撞、玩家射击、炸弹和 Boss 阶段推进。

#include "app/game.hpp"
#include "app/game_constants.hpp"
#include "app/game_math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>

namespace redmist {
using namespace game_math;

void Game::UpdateEnemies(float deltaTime) {
    enemies_.ForEachActive([this, deltaTime](Enemy& enemy) {
        enemy.Update(deltaTime, player_.position, bullets_);
    });
}

void Game::UpdateBoss(float deltaTime) {
    if (boss_ && boss_->Update(deltaTime, player_.position, bullets_))
        AdvanceBossPhase();
}

void Game::ResolveCollisions() {
    bool playerWasHit = false;
    // 距离使用平方值比较，避免每颗弹幕都计算平方根。
    bullets_.Pool().ForEachActive([this, &playerWasHit](Bullet& bullet) {
        if (bullet.owner != BulletOwner::Enemy) return;
        const float distanceSquared = DistanceSquared(bullet.position, player_.position);
        const float hitDistance = bullet.radius + player_.hitRadius;
        if (distanceSquared <= hitDistance * hitDistance) {
            bullet.active = false;
            bool canTakeDamage = player_.invulnerableTimer <= 0.0F;
#ifdef RED_MIST_DEBUG_FEATURES
            canTakeDamage = canTakeDamage && !debugInvincible_;
#endif
            if (canTakeDamage)
                playerWasHit = true;
        } else {
            const float grazeDistance = bullet.radius + player_.grazeRadius;
            if (!bullet.grazed && distanceSquared <= grazeDistance * grazeDistance) {
                bullet.grazed = true;
                ++graze_;
                score_ += 100;
            }
        }
    });
    if (playerWasHit) DamagePlayer();

    enemies_.ForEachActive([this](Enemy& enemy) {
        // 延迟入场的敌人尚未显示，也不应提前承受玩家弹幕。
        if (enemy.age < 0.0F) return;
        bullets_.Pool().ForEachActive([&enemy](Bullet& bullet) {
            if (bullet.owner != BulletOwner::Player) return;
            const float collisionDistance = bullet.radius + enemy.hitRadius;
            if (DistanceSquared(bullet.position, enemy.position) <=
                collisionDistance * collisionDistance) {
                bullet.active = false;
                enemy.health -= 2.0F;
            }
        });
        if (enemy.health <= 0.0F) {
            enemy.active = false;
            score_ += enemy.scoreValue;
        }
    });

    if (boss_ && boss_->active) {
        bullets_.Pool().ForEachActive([this](Bullet& bullet) {
            if (bullet.owner != BulletOwner::Player) return;
            const float collisionDistance = bullet.radius + boss_->hitRadius;
            if (DistanceSquared(bullet.position, boss_->position) <=
                collisionDistance * collisionDistance) {
                bullet.active = false;
                boss_->health -= 1.6F;
                score_ += 10;
            }
        });
        if (boss_->health <= 0.0F) AdvanceBossPhase();
    }
}

void Game::FirePlayerShot() {
    const Vector3 forward = Forward();
    const Vector3 right = Right();
    const Vector3 muzzle = Add(player_.position, Scale(forward, 0.55F));
    const Vector3 velocity = Scale(forward, 20.0F);
    bullets_.Spawn(BulletOwner::Player, Add(muzzle, Scale(right, -0.11F)),
                   velocity, 0.09F, kPlayerBulletColor,
                   stageContent_->PlayerBulletVisual());
    bullets_.Spawn(BulletOwner::Player, Add(muzzle, Scale(right, 0.11F)),
                   velocity, 0.09F, kPlayerBulletColor,
                   stageContent_->PlayerBulletVisual());
}

void Game::UseBomb() {
    if (player_.bombs <= 0 || player_.bombCooldown > 0.0F) return;
    --player_.bombs;
    player_.bombCooldown = 1.0F;
    player_.invulnerableTimer = 2.6F;
    screenFlashTimer_ = 0.35F;
    bullets_.ClearEnemyBullets();
    enemies_.ForEachActive([](Enemy& enemy) { enemy.health -= 22.0F; });
    if (boss_ && boss_->active) boss_->health -= 35.0F;
    SetMessage("Spatial Bomb", 0.7F);
}

void Game::DamagePlayer() {
    --player_.lives;
    player_.bombs = std::max(player_.bombs, 2);
    player_.position = {0.0F, 0.0F, -6.5F};
    player_.yaw = 0.0F;
    player_.pitch = 0.0F;
    UpdateCamera();
    player_.invulnerableTimer = 2.8F;
    screenFlashTimer_ = 0.22F;
    bullets_.ClearEnemyBullets();
    if (player_.lives < 0) {
        EndStage(RunState::GameOver);
    } else {
        SetMessage("Miss  Camera hitbox restored", 0.9F);
    }
}

void Game::AdvanceBossPhase() {
    if (!boss_ || !boss_->active) return;
    bullets_.ClearEnemyBullets();
    const bool wasMidboss =
        boss_->IsMidboss();
    const int completedPhase = boss_->phase;
    const bool hasNextPhase = boss_->AdvancePhase();
    if (!hasNextPhase && bossEventId_) {
        stage_.EndEvent(*bossEventId_);
        bossEventId_.reset();
    }
    if (hasNextPhase) {
        score_ += completedPhase == 1 ? 15000 : 25000;
        spellCutInTimer_ = boss_->SpellName() != nullptr
                               ? kSpellCutInDuration : 0.0F;
        messageTimer_ = 0.0F;
    } else if (wasMidboss) {
        score_ += 5000;
        SetMessage(stageContent_->MidbossEndMessage(), 1.6F);
    } else {
        score_ += 50000;
        messageTimer_ = 0.0F;
        EndStage(RunState::Won);
    }
}

}  // namespace redmist
