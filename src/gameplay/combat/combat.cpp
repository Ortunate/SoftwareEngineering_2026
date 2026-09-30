// 本文件负责敌我碰撞结算、玩家 Bomb 的场景效果和 Boss 阶段推进。

#include "app/game.hpp"
#include "app/constants.hpp"
#include "app/math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>

namespace redmist {
using namespace game_math;

void Game::UpdateEnemies(float deltaTime) {
    enemies_.ForEachActive([this, deltaTime](Enemy& enemy) {
        enemy.Update(deltaTime, player_.Position(), bullets_);
    });
}

void Game::UpdateBoss(float deltaTime) {
    if (boss_ && boss_->Update(deltaTime, player_.Position(), bullets_))
        AdvanceBossPhase();
}

void Game::ResolveCollisions() {
    bool playerWasHit = false;
    // 本帧可能同时接触多颗敌弹；先统一标记受击，再只扣一次残机。
    bullets_.Pool().ForEachActive([this, &playerWasHit](Bullet& bullet) {
        switch (player_.ContactEnemyBullet(bullet)) {
            case PlayerContact::Hit: {
                bool canTakeDamage = true;
#ifdef RED_MIST_DEBUG_FEATURES
                canTakeDamage = !debugInvincible_;
#endif
                if (canTakeDamage) playerWasHit = true;
                break;
            }
            case PlayerContact::Graze:
                ++graze_;
                score_ += 100;
                break;
            case PlayerContact::None:
                break;
        }
    });
    if (playerWasHit) HandlePlayerHit();

    enemies_.ForEachActive([this](Enemy& enemy) {
        // 延迟入场的敌人尚未显示，也不应提前承受玩家弹幕。
        if (enemy.age < 0.0F) return;
        bullets_.Pool().ForEachActive([&enemy](Bullet& bullet) {
            if (bullet.owner != BulletOwner::Player) return;
            const float collisionDistance = bullet.radius + enemy.hitRadius;
            if (DistanceSquared(bullet.position, enemy.position) <=
                collisionDistance * collisionDistance) {
                bullet.active = false;
                enemy.health -= bullet.damage;
            }
        });
        if (enemy.health <= 0.0F) {
#ifdef RED_MIST_DEBUG_FEATURES
            if (shotTestActive_) {
                enemy.health = enemy.maxHealth;
                return;
            }
#endif
            enemy.active = false;
            score_ += enemy.scoreValue;
            SpawnPowerPickup(enemy.position, enemy.powerDrop);
        }
    });

    if (boss_ && boss_->active) {
        bullets_.Pool().ForEachActive([this](Bullet& bullet) {
            if (bullet.owner != BulletOwner::Player) return;
            const float collisionDistance = bullet.radius + boss_->hitRadius;
            if (DistanceSquared(bullet.position, boss_->position) <=
                collisionDistance * collisionDistance) {
                bullet.active = false;
                boss_->health -= bullet.damage * 0.8F;
                score_ += 10;
            }
        });
        if (boss_->health <= 0.0F) AdvanceBossPhase();
    }
}

void Game::SpawnPowerPickup(Vector3 position, int amount) {
    if (amount <= 0) return;
    if (PowerPickup* pickup = powerPickups_.Acquire()) {
        pickup->position = position;
        pickup->amount = amount;
    }
}

void Game::UpdatePowerPickups(float deltaTime) {
    const Vector3 playerPosition = player_.Position();
    powerPickups_.ForEachActive([this, deltaTime, playerPosition](PowerPickup& pickup) {
        pickup.age += deltaTime;
        if (pickup.age > 15.0F) {
            pickup.active = false;
            return;
        }
        const Vector3 toPlayer = Subtract(playerPosition, pickup.position);
        const float distance = std::sqrt(DistanceSquared(playerPosition,
                                                         pickup.position));
        // 掉落物始终缓慢靠近玩家，近处加速；步长不得越过玩家，否则低帧率下会来回穿梭。
        if (distance > 0.0001F) {
            const float speed = distance < 2.5F ? 8.0F : 2.5F;
            pickup.position = Add(pickup.position, Scale(toPlayer,
                std::min(speed * deltaTime, distance) / distance));
        }
        if (DistanceSquared(playerPosition, pickup.position) < 0.6F * 0.6F) {
            const int previousPower = player_.Power();
            player_.AdjustPower(pickup.amount);
            pickup.active = false;
            if (previousPower < Player::kMaxPower &&
                player_.Power() == Player::kMaxPower) {
                bullets_.ClearEnemyBullets();
            }
        }
    });
}

void Game::ApplyBombEffect() {
    screenFlashTimer_ = 0.35F;
    bullets_.ClearEnemyBullets();
    enemies_.ForEachActive([](Enemy& enemy) { enemy.health -= 22.0F; });
    if (boss_ && boss_->active) boss_->health -= 35.0F;
    SetMessage("Spatial Bomb", 0.7F);
}

void Game::HandlePlayerHit() {
    const bool survived = player_.TakeHit();
    player_.SyncCamera(camera_);
    screenFlashTimer_ = 0.22F;
    bullets_.ClearEnemyBullets();
    if (!survived) {
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
    // 仅整场遭遇结束时通知时间轴；阶段切换仍属于同一异步事件。
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
        SpawnPowerPickup(boss_->position, 8);
        SetMessage(stageContent_->MidbossEndMessage(), 1.6F);
    } else {
        score_ += 50000;
        messageTimer_ = 0.0F;
        EndStage(RunState::Won);
    }
}

}  // namespace redmist
