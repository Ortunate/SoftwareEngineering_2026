// 本文件负责敌我碰撞、道具计分、满火力与 Bomb 场景效果及 Boss 阶段推进。

#include "app/game.hpp"
#include "app/constants.hpp"
#include "app/math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>

namespace koumakyou {
using namespace game_math;

void Game::UpdateEnemies(float deltaTime) {
    enemies_.ForEachActive([this, deltaTime](Enemy& enemy) {
        enemy.SetChallengeRank(referenceRank_);
        enemy.Update(deltaTime, player_.Position(), bullets_);
        for(int id:enemy.SoundEvents())assets_.PlaySoundEffect("reference_"+std::to_string(id));
        const auto& visual=assets_.GetVisual(enemy.visual);
        if(visual.archive) {
            auto selected=visual.animation;
            if(enemy.animationScript>=0)selected.script=enemy.animationScript;
            enemy.animation.Sync(*visual.archive,selected,enemy.animationAge>=0 ? enemy.animationAge : enemy.age,enemy.animationRevision);
        }
        // 只处理本帧仍活动到失活的跃迁，快照不延长 Enemy 的槽位寿命。
        if (!enemy.active)
            actorEffects_.Spawn(enemy.visual, enemy.position, enemy.hitRadius,
                                enemy.presentation, ActorExitReason::Retreat, &enemy.animation.player.Sample());
    });
}

void Game::UpdateBoss(float deltaTime) {
    if(boss_)boss_->SetChallengeRank(referenceRank_);
    if(boss_ && boss_->active) {
        const bool ended=boss_->Update(deltaTime,player_.Position(),bullets_);
        for(int id:boss_->SoundEvents())assets_.PlaySoundEffect("reference_"+std::to_string(id));
        for(int count:boss_->DropEvents())drops_.SpawnReferenceBurst(boss_->position,count,
                                            player_.Power()==Player::kMaxPower);
        if(ended)AdvanceBossPhase();
    }
}

void Game::ResolveCollisions() {
    bool playerWasHit = false;
    // 本帧可能同时接触多颗敌弹；先统一标记受击，再只扣一次残机。
    bullets_.Pool().ForEachActive([this, &playerWasHit](Bullet& bullet) {
        switch (player_.ContactEnemyBullet(bullet)) {
            case PlayerContact::Hit: {
                bool canTakeDamage = true;
#ifdef KOUMAKYOU_DEBUG_FEATURES
                canTakeDamage = !debugInvincible_;
#endif
                if (canTakeDamage) playerWasHit = true;
                break;
            }
            case PlayerContact::Graze:
                ++graze_;
                AdjustReferenceRank(6);
                score_ += 100;
                assets_.PlaySoundEffect("graze");
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
#ifdef KOUMAKYOU_DEBUG_FEATURES
            if (shotTestActive_) {
                enemy.health = enemy.maxHealth;
                return;
            }
#endif
            enemy.active = false;
            assets_.PlaySoundEffect("enemy_down");
            actorEffects_.Spawn(enemy.visual, enemy.position, enemy.hitRadius,
                                enemy.presentation, ActorExitReason::Defeated, &enemy.animation.player.Sample(), enemy.defeatVisual!=kInvalidVisualId ? enemy.defeatVisual : assets_.GetVisual(enemy.visual).defeatVisual);
            score_ += enemy.scoreValue;
            if(enemy.ReferenceDropKind()==-3) drops_.SpawnEnemyDrops(enemy.position,enemy.DropDistribution());
            else drops_.SpawnReferenceDrop(enemy.position,enemy.ReferenceDropKind(),player_.Power()==Player::kMaxPower);
        }
    });

    if (boss_ && boss_->active) {
        bullets_.Pool().ForEachActive([this](Bullet& bullet) {
            if (bullet.owner != BulletOwner::Player) return;
            const float collisionDistance = bullet.radius + boss_->hitRadius;
            if (DistanceSquared(bullet.position, boss_->position) <=
                collisionDistance * collisionDistance) {
                bullet.active = false;
                // 原作符卡将普通射击伤害缩小至约七分之一；HP 阈值本身仍采用原始单位。
                if (boss_->CanTakeDamage()) boss_->health -= bullet.damage * 0.8F / (boss_->SpellName() ? 7.0F : 1.0F);
                score_ += 10;
            }
        });
        if (boss_->ShouldAdvanceFromDamage()) AdvanceBossPhase();
    }
}

void Game::UpdatePickups(float deltaTime) {
    const PickupReward reward = drops_.Update(deltaTime, player_.Position());
    AdjustPlayerPower(reward.power);
    score_ += reward.score;
    pointItems_ += reward.pointItems;
    AdjustReferenceRank(reward.pointItems*3 + reward.power*3);
    if (reward.power != 0 || reward.score != 0) assets_.PlaySoundEffect("pickup");
}

void Game::AdjustPlayerPower(int delta) {
    if (!player_.AdjustPower(delta)) return;
    drops_.ConvertEnemyBulletsToStars(bullets_);
    SetMessage("Full Power", 2.0F);
    assets_.PlaySoundEffect("full_power");
}

void Game::ApplyBombEffect() {
    ++bombCount_;
    AdjustReferenceRank(-200);
    assets_.PlaySoundEffect("bomb");
    screenFlashTimer_ = 0.35F;
    bullets_.ClearEnemyBullets();
    enemies_.ForEachActive([](Enemy& enemy) { enemy.health -= 22.0F; });
    if (boss_ && boss_->active && boss_->CanTakeDamage()) boss_->health -= 35.0F / (boss_->SpellName() ? 3.0F : 1.0F);
    SetMessage("Spatial Bomb", 0.7F);
}

void Game::HandlePlayerHit() {
    AdjustReferenceRank(-1600);
    ++missCount_;
    assets_.PlaySoundEffect("hit");
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
    // 捕获奖励只在符卡击破时结算；退场回调仍保留 phase，必须用 SpellName 排除重复结算。
    if(boss_->SpellName() && boss_->health<=0 && missCount_==spellStartMisses_ && bombCount_==spellStartBombs_) {
        const int seconds=static_cast<int>(std::max(0.0F,boss_->TimeLimit()-boss_->phaseTimer));
        spellBonus_=200000+20000*seconds; score_+=spellBonus_;
        spellBonusTimer_=280.0F/60;
    }
    if (spellPractice_) {
        EndStage(RunState::Won);
        return;
    }
    bullets_.ClearEnemyBullets();
    const bool wasMidboss =
        boss_->IsMidboss();
    const int completedPhase = boss_->phase;
    const bool hasNextPhase = boss_->AdvancePhase();
    const ActorExitReason exitReason = boss_->Defeated()
        ? ActorExitReason::Defeated : ActorExitReason::Retreat;
    if (!hasNextPhase)
        actorEffects_.Spawn(boss_->visual, boss_->position, boss_->hitRadius,
                            boss_->presentation, exitReason, &boss_->animation.player.Sample(), boss_->defeatVisual!=kInvalidVisualId ? boss_->defeatVisual : assets_.GetVisual(boss_->visual).defeatVisual);
    // 仅整场遭遇结束时通知时间轴；阶段切换仍属于同一异步事件。
    if (!hasNextPhase && bossEventId_) {
        stage_.EndEvent(*bossEventId_);
        bossEventId_.reset();
    }
    if (hasNextPhase && boss_->phase != completedPhase) {
        spellStartMisses_=missCount_; spellStartBombs_=bombCount_;
        spellCutInTimer_ = boss_->SpellName() != nullptr
                               ? kSpellCutInDuration : 0.0F;
        messageTimer_ = 0.0F;
        assets_.PlaySoundEffect(boss_->SpellName() ? "spell" : "enemy_down");
    } else if (!hasNextPhase && wasMidboss) {
        if(boss_->Defeated())score_+=100000;
        messageTimer_=0;
    } else if (!hasNextPhase) {
        score_ += 200000;
        messageTimer_ = 0.0F;
        // 战斗异步事件已结束，下一帧时间轴启动原作战后独白；独白完毕才结算。
    }
}

}  // namespace koumakyou
