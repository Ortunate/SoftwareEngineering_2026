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
namespace {
constexpr std::array<std::string_view,4> kBombSounds{"reference_6","bomb","reference_7","bomb_master"};
}

void Game::UpdateActors(float deltaTime) {
    std::vector<behavior::Spawn> children;
    bool defeatActors=false;
    actors_.ForEachActive([&](behavior::Actor& enemy) {
        UpdateActor(enemy,deltaTime);
        children.insert(children.end(),enemy.spawnEvents.begin(),enemy.spawnEvents.end());
        defeatActors|=enemy.defeatActors;
    });
    if(defeatActors)actors_.ForEachActive([](behavior::Actor& actor){actor.health=0;});
    SpawnBehaviorActors(children,deltaTime);
}

void Game::UpdateActor(behavior::Actor& enemy,float deltaTime) {
    enemy.SetRank(referenceRank_);
    enemy.phaseClockPaused=WorldTimeStopped();
    enemy.Update(deltaTime, player_.Position(), bullets_);
    // 普通演员的命名行为可交接击破/寿命回调；不在行为 Update 内重入协程所有权操作。
    if(enemy.pendingCallback)enemy.Transition();
    for(int id:enemy.soundEvents)assets_.PlaySoundEffect("reference_"+std::to_string(id));
    for(int count:enemy.dropEvents) drops_.SpawnReferenceBurst(enemy.position,count,player_.Power()==Player::kMaxPower);
    for(int item:enemy.itemEvents) drops_.SpawnNativeItem(enemy.position,item,player_.Power()==Player::kMaxPower);
    const auto& visual=assets_.GetVisual(enemy.visual);
    if(visual.archive) {
        auto selected=visual.animation;
        if(enemy.animationScript>=0)selected.script=enemy.animationScript;
        enemy.animation.Sync(*visual.archive,selected,enemy.animationAge>=0 ? enemy.animationAge : enemy.age,enemy.animationRevision,
            enemy.animationInterrupt ? &*enemy.animationInterrupt : nullptr);
        if(enemy.rotateAnimation)enemy.animation.player.SetRotationZ(enemy.motion.angle+std::numbers::pi_v<float>/2);
    }
    // 只处理本帧仍活动到失活的跃迁，快照不延长 Actor 的槽位寿命。
    if (!enemy.active && !enemy.retiring && enemy.visible)
        actorEffects_.Spawn(enemy.visual, enemy.position, enemy.hitRadius,
                            enemy.presentation, ActorExitReason::Retreat, &enemy.animation.player.Sample());
}
void Game::SpawnBehaviorActors(std::span<const behavior::Spawn> requests,float deltaTime) {
    // 请求按值复制，槽位复用不能使输入 span 或挂起协程引用失效；新演员在本内容步执行出生行为。
    std::vector<behavior::Spawn> pending(requests.begin(),requests.end());
    for(std::size_t index=0;index<pending.size();++index) {
        const auto request=pending[index];
        if(auto* actor=actors_.Spawn(request)) {
            UpdateActor(*actor,deltaTime);
            if(actor->defeatActors)actors_.ForEachActive([](behavior::Actor& target){target.health=0;});
            pending.insert(pending.end(),actor->spawnEvents.begin(),actor->spawnEvents.end());
        }
    }
}

void Game::UpdateBoss(float deltaTime) {
    if(boss_)boss_->SetChallengeRank(referenceRank_);
    if(boss_ && boss_->active) {
        boss_->SetPlayerBombActive(playerBomb_.Active());
        const int previousSpell=boss_->SpellCardNumber();
        const bool ended=boss_->Update(deltaTime,player_.Position(),bullets_);
        // 部分遭遇在同一阶段内先等待再宣告符卡；按真实宣告时刻记录及切入，不能提前到阶段交接。
        if(!ended&&previousSpell==0&&boss_->SpellCardNumber()>0) {
            spellStartMisses_=missCount_;spellStartBombs_=bombCount_;spellCutInTimer_=kSpellCutInDuration;
            BeginSpellRecord();assets_.PlaySoundEffect("spell");
        }
        for(int id:boss_->SoundEvents())assets_.PlaySoundEffect("reference_"+std::to_string(id));
        for(int count:boss_->DropEvents())drops_.SpawnReferenceBurst(boss_->position,count,
                                            player_.Power()==Player::kMaxPower);
        for(int item:boss_->ItemEvents())drops_.SpawnNativeItem(boss_->position,item,player_.Power()==Player::kMaxPower);
        if(boss_->DefeatActorsRequested())actors_.ForEachActive([](behavior::Actor& actor){actor.health=0;});
        SpawnBehaviorActors(boss_->SpawnEvents(),deltaTime);
        if(ended)AdvanceBossPhase();
    }
}

void Game::ResolveCollisions() {
    bool playerWasHit = false;
    // 敌弹、普通敌人和 Boss 本体共同标记受击，所有接触都采样后只结算一次，避免复活位置影响同帧结果。
    bullets_.Pool().ForEachActive([this, &playerWasHit](Bullet& bullet) {
        if(WorldTimeStopped()&&bullet.owner==BulletOwner::Enemy)return;
        switch (player_.ContactEnemyBullet(bullet)) {
            case PlayerContact::Hit: {
                playerWasHit = true;
                break;
            }
            case PlayerContact::Graze:
                ++graze_;
                ++stageGraze_;
                AdjustReferenceRank(6);
                score_ += 100;
                assets_.PlaySoundEffect("graze");
                break;
            case PlayerContact::None:
                break;
        }
    });
    if(state_==RunState::Playing && !dialogueActive_) {
        // 对话人物及退场快照不在本体判定中；本体只让玩家受击，不给敌人扣血、生成掉落或产生擦弹。
        actors_.ForEachActive([&](const behavior::Actor& enemy) {
            if(enemy.CanContactPlayer() && player_.ContactEnemyBody(enemy.position,enemy.hitRadius))playerWasHit=true;
        });
        if(boss_ && boss_->CanContactPlayer() && player_.ContactEnemyBody(boss_->position,boss_->hitRadius))playerWasHit=true;
    }
#ifdef KOUMAKYOU_DEBUG_FEATURES
    if(debugInvincible_)playerWasHit=false;
#endif
    if (playerWasHit) HandlePlayerHit();
    if (state_ != RunState::Playing) return;

    const auto shotDamage=[this](const Bullet& bullet,DamageTarget target,const void* identity,bool damageable,bool spell) {
        const auto source=bullet.playerShotKind==PlayerShotKind::Laser ? DamageSource::Laser :
                          bullet.playerShotKind==PlayerShotKind::Explosion ? DamageSource::Explosion : DamageSource::Projectile;
        return ResolveDamage({source,target,player_.Shot(),bullet.ContactDamage(),damageable,spell,
                              identity,bullet.sourceShotRva,stage_.Elapsed(),bullet.damageSeconds,playerBomb_.Active()},
                             stageContent_->DamageRules()).amount;
    };
    const auto applyShotImpact=[this](Bullet& bullet) {
        if(bullet.playerShotKind==PlayerShotKind::Missile)bullets_.ExplodePlayerMissile(bullet);
        else if(bullet.playerShotKind==PlayerShotKind::Projectile)bullet.active=false;
        else if(bullet.playerShotKind==PlayerShotKind::Explosion)
            // 原作爆破每个持续接触的逻辑帧继续减半速度；按经过秒数换算，避免低 FPS 减速变弱。
            bullet.velocity=Scale(bullet.velocity,std::pow(0.5F,bullet.damageSeconds*60));
    };
    actors_.ForEachActive([this,&applyShotImpact,&shotDamage](behavior::Actor& enemy) {
        bullets_.Pool().ForEachActive([&enemy,&applyShotImpact,&shotDamage](Bullet& bullet) {
            if(!enemy.CanBeTargeted())return;
            if (bullet.owner != BulletOwner::Player || !bullet.Dangerous() || bullet.ContactDamage()<=0) return;
            const float collisionDistance = bullet.radius + enemy.hitRadius;
            if (bullet.DistanceSquaredTo(enemy.position) <=
                collisionDistance * collisionDistance) {
                enemy.health -= shotDamage(bullet,DamageTarget::Enemy,&enemy,enemy.CanTakeDamage(),false);
                applyShotImpact(bullet);
            }
        });
        if (enemy.health <= 0.0F && enemy.CanBeTargeted()) {
#ifdef KOUMAKYOU_DEBUG_FEATURES
            if (shotTestActive_) {
                enemy.health = enemy.maxHealth;
                return;
            }
#endif
            if(!enemy.Defeat())return;
            assets_.PlaySoundEffect("enemy_down");
            actorEffects_.Spawn(enemy.visual, enemy.position, enemy.hitRadius,
                                enemy.presentation, ActorExitReason::Defeated, &enemy.animation.player.Sample(), enemy.defeatVisual!=kInvalidVisualId ? enemy.defeatVisual : assets_.GetVisual(enemy.visual).defeatVisual);
            score_ += enemy.scoreValue;
            drops_.SpawnReferenceDrop(enemy.position,enemy.sourceItem,player_.Power()==Player::kMaxPower);
        }
    });

    if (boss_ && boss_->active) {
        const bool targetable=boss_->CanBeTargeted();
        bullets_.Pool().ForEachActive([this,&applyShotImpact,&shotDamage,targetable](Bullet& bullet) {
            if(!targetable)return;
            if (bullet.owner != BulletOwner::Player || !bullet.Dangerous() || bullet.ContactDamage()<=0) return;
            const float collisionDistance = bullet.radius + boss_->hitRadius;
            if (bullet.DistanceSquaredTo(boss_->position) <=
                collisionDistance * collisionDistance) {
                boss_->health -= shotDamage(bullet,DamageTarget::Boss,boss_.get(),boss_->CanTakeDamage(),boss_->SpellName()!=nullptr);
                if(bullet.damagePerSecond>0) {
                    const double points=bullet.hitScoreRemainder+600.0*bullet.damageSeconds;
                    const auto whole=static_cast<long long>(std::floor(points+0.00001));
                    score_+=whole;bullet.hitScoreRemainder=points-static_cast<double>(whole);
                } else score_+=10;
                applyShotImpact(bullet);
            }
        });
        if (boss_->ShouldAdvanceFromDamage()) AdvanceBossPhase();
    }
    // 持续弹可穿过多个目标，但同一份经过秒数只能结算一次，重复碰撞调用不能再次扣血。
    bullets_.Pool().ForEachActive([](Bullet& bullet){
        if(bullet.damagePerSecond>0)bullet.damageSeconds=0;
        if(bullet.pendingExpiration)bullet.active=false;
    });
}

void Game::UpdatePickups(float deltaTime) {
    const PickupReward reward = drops_.Update(deltaTime, player_.Position());
    AdjustPlayerPower(reward.power);
    player_.CollectResources(reward.lives,reward.bombs);
    score_ += reward.score;
    pointItems_ += reward.pointItems;
    stagePointItems_ += reward.pointItems;
    AdjustReferenceRank(reward.pointItems*3 + reward.power*3);
    if (reward.power != 0 || reward.score != 0 || reward.bombs || reward.lives) assets_.PlaySoundEffect("pickup");
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
    ClearPlayerBomb();
    const auto visuals=stageContent_->PlayerBombVisuals(player_.Shot());
    const anm::Archive* archive=nullptr;
    if(visuals[0]!=kInvalidVisualId) {
        archive=assets_.GetVisual(visuals[0]).archive.get();
    }
    player_.SyncCamera(camera_);
    playerBomb_.Start(player_.Shot(),camera_,archive,static_cast<std::uint32_t>(bombCount_));
    drops_.MagnetAllItems();
    PlayPlayerBombSounds();
    screenFlashTimer_ = player_.Shot()==ShotType::Needle ? 0 : 0.12F;
    messageTimer_=0; // 独立 Bomb 宣告使用原名并由演出时钟驱动，避免与普通提示重复。
}
void Game::UpdatePlayerBomb(float deltaTime) {
    if(!playerBomb_.Active())return;
    if(WorldTimeStopped()) {
        // 原生玩家更新被跳过，已存在的 Bomb 区域仍能被敌人读取；区域冻结时不重复播放或执行清弹。
        frozenBombFrames_+=deltaTime*60;
        while(frozenBombFrames_+1e-6>=1) {frozenBombFrames_-=1;ResolvePlayerBombDamage();}
        return;
    }
    // 初始清弹也放在敌弹推进之后，覆盖释放本帧由敌人新生成的弹；frame=0 没有伤害通道。
    if(playerBomb_.Frame()==0)ResolvePlayerBombFrame();
    if(playerBombStartedThisFrame_)return;
    std::array<Vector3,kActorCapacity+1> targets{};std::size_t count=0;
    actors_.ForEachActive([&](const behavior::Actor& enemy) {
        if(enemy.health>0 && enemy.CanTakeDamage())targets[count++]=enemy.position;
    });
    if(boss_ && boss_->active && boss_->health>0 && boss_->CanTakeDamage())targets[count++]=boss_->position;
    playerBomb_.Update(deltaTime,camera_,{targets.data(),count},[this](PlayerBombEffect&) { ResolvePlayerBombFrame(); });
    PlayPlayerBombSounds();
}
void Game::ResolvePlayerBombFrame() {
    if(!playerBomb_.Active() || playerBombResolvedFrame_==playerBomb_.Frame())return;
    playerBombResolvedFrame_=playerBomb_.Frame();
    bullets_.Pool().ForEachActive([this](Bullet& bullet) {
        if(bullet.owner!=BulletOwner::Enemy)return;
        for(const auto& volume:playerBomb_.CancelVolumes()) {
            const float radius=bullet.radius+kBombCancelMargin;
            if(bullet.IsBeam() ? volume.IntersectsCapsule(bullet.position,bullet.BeamEnd(),radius) :
                                    volume.IntersectsSphere(bullet.position,radius)) { bullet.active=false;break; }
        }
    });
    ResolvePlayerBombDamage();
}
void Game::ResolvePlayerBombDamage() {
    const auto damage=[this](Vector3 position,float radius,DamageTarget target,const void* identity,bool spell) {
        const auto channels=playerBomb_.ContactDamage(position,radius);
        int total=0;
        for(int region=0;region<8;++region) { total+=channels[region];playerBomb_.ReportHit(region,channels[region]); }
        // 提供方只能调整真实接触后的 HP，不能在蓄力/跳帧或范围外凭空生成命中。
        if(total==0)return 0.0F;
        // 原作积分在 70 上限之后、符卡减伤之前；不能让四层恋札或 Z 扩展把单帧伤害重复乘倍。
        score_+=(std::min(total,70)/5)*10;
        return ResolveDamage({DamageSource::Bomb,target,playerBomb_.Shot(),static_cast<float>(total),true,spell,
                              identity,0,stage_.Elapsed(),1.0F/60,true,!spell || bombCount_>spellStartBombs_},
                             stageContent_->DamageRules()).amount;
    };
    actors_.ForEachActive([&](behavior::Actor& enemy) {
        if(enemy.health>0 && enemy.CanTakeDamage())enemy.health-=damage(enemy.position,enemy.hitRadius,DamageTarget::Enemy,&enemy,false);
    });
    if(boss_ && boss_->active && boss_->health>0 && boss_->CanTakeDamage())
        boss_->health-=damage(boss_->position,boss_->hitRadius,DamageTarget::Boss,boss_.get(),boss_->SpellName()!=nullptr);
}
void Game::PlayPlayerBombSounds() {
    std::array<bool,20> played{};
    for(const int id:playerBomb_.SoundEvents()) {
        if(id<0 || id>=static_cast<int>(played.size()) || played[id])continue;
        played[id]=true;
        const char* sound=id==6 ? "reference_6" : id==13 ? "bomb" : id==15 ? "reference_7" : id==19 ? "bomb_master" : nullptr;
        if(sound)assets_.PlaySoundEffect(sound);
    }
}
void Game::ClearPlayerBomb() {
    playerBomb_.Clear();playerBombSoundPaused_.fill(false);playerBombResolvedFrame_=-1;frozenBombFrames_=0;playerBombStartedThisFrame_=false;
    for(const auto name:kBombSounds)assets_.StopSoundEffect(name);
}
void Game::PausePlayerBombSounds(bool paused) {
    for(std::size_t i=0;i<kBombSounds.size();++i) {
        if(paused)playerBombSoundPaused_[i]=assets_.PauseSoundEffect(kBombSounds[i]);
        else if(playerBombSoundPaused_[i]) {
            assets_.ResumeSoundEffect(kBombSounds[i]);playerBombSoundPaused_[i]=false;
        }
    }
}

void Game::HandlePlayerHit() {
    ClearPlayerBomb();
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
    BeginSpellRecord();
    const bool captured=boss_->SpellName() && boss_->PhaseDefeated() && missCount_==spellStartMisses_ && bombCount_==spellStartBombs_;
    // 捕获奖励只在符卡击破时结算；退场回调仍保留 phase，必须用 SpellName 排除重复结算。
    if(captured) {
        const int seconds=static_cast<int>(std::max(0.0F,boss_->TimeLimit()-boss_->phaseTimer));
        spellBonus_=200000+20000*seconds; score_+=spellBonus_;
        spellBonusTimer_=280.0F/60;
    }
    if(boss_->SpellName())FinishSpellRecord(captured);
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
        // 宣告符卡的本帧即记录遇见并落盘；即使紧接着暂停退出，也不能漏掉已经展示的符卡。
        BeginSpellRecord();
        messageTimer_ = 0.0F;
        assets_.PlaySoundEffect(boss_->SpellName() ? "spell" : "enemy_down");
    } else if (!hasNextPhase && wasMidboss) {
        if(boss_->Defeated())score_+=100000;
        messageTimer_=0;
    } else if (!hasNextPhase) {
        score_ += 200000;
        messageTimer_ = 0.0F;
        // 战斗事件已结束，下一帧时间轴开始战后独白，独白结束才展示关卡奖励。
    }
}

}  // namespace koumakyou
