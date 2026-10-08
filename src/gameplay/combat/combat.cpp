// 本文件负责敌我碰撞、道具计分、满火力与 Bomb 场景效果及 Boss 阶段推进。

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
        // 只处理本帧仍活动到失活的跃迁，快照不延长 Enemy 的槽位寿命。
        if (!enemy.active)
            actorEffects_.Spawn(enemy.visual, enemy.position, enemy.hitRadius,
                                enemy.presentation, ActorExitReason::Retreat);
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
    if (state_ != RunState::Playing) return;

    enemies_.ForEachActive([this](Enemy& enemy) {
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
            actorEffects_.Spawn(enemy.visual, enemy.position, enemy.hitRadius,
                                enemy.presentation, ActorExitReason::Defeated);
            score_ += enemy.scoreValue;
            drops_.SpawnEnemyDrops(enemy.position, enemy.DropDistribution());
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

void Game::UpdatePickups(float deltaTime) {
    const PickupReward reward = drops_.Update(deltaTime, player_.Position());
    AdjustPlayerPower(reward.power);
    score_ += reward.score;
}

void Game::AdjustPlayerPower(int delta) {
    if (!player_.AdjustPower(delta)) return;
    drops_.ConvertEnemyBulletsToStars(bullets_);
    SetMessage("Full Power", 2.0F);
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
    const ActorExitReason exitReason = boss_->health <= 0.0F
        ? ActorExitReason::Defeated : ActorExitReason::Retreat;
    const bool hasNextPhase = boss_->AdvancePhase();
    if (!hasNextPhase)
        actorEffects_.Spawn(boss_->visual, boss_->position, boss_->hitRadius,
                            boss_->presentation, exitReason);
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
        // Boss 的专属概率规则尚未实现，暂时保留原有的固定大 P 点奖励。
        drops_.SpawnFixedPower(boss_->position, 8);
        SetMessage(stageContent_->MidbossEndMessage(), 1.6F);
    } else {
        score_ += 50000;
        messageTimer_ = 0.0F;
        EndStage(RunState::Won);
    }
}

}  // namespace redmist
