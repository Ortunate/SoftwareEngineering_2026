// 本文件负责游戏生命周期、主循环调度和关卡事件转发。

#include "app/game.hpp"
#include "app/constants.hpp"
#include "render/menu_layout.hpp"
#include "config/settings.hpp"
#include "stages/stage1/ids.hpp"

#include "rlgl.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <numbers>
#include <utility>

namespace koumakyou {
#ifdef KOUMAKYOU_DEBUG_FEATURES
namespace {
/// @brief 射击调试场地中的固定靶怪；不移动、不攻击，由 Game 在血量耗尽时复原。
class ShotTestEnemy final : public Enemy {
public:
    ShotTestEnemy() : Enemy(stage1::kDarkFairy, {0.0F, 0.0F, 4.0F}, {}) {
        health = maxHealth = 240.0F;
        hitRadius = 1.0F;
        scoreValue = 0;
    }

private:
    void Move(float) override {}
    void UpdateSkills(float, Vector3, BulletSystem&) override {}
};
}  // namespace
#endif

Game::Game()
    : assets_(std::filesystem::path(GetApplicationDirectory()) / "assets") {
#ifdef KOUMAKYOU_DEBUG_FEATURES
    debugInvincible_ = true;
#endif
    camera_.up = {0.0F, 1.0F, 0.0F};
    camera_.fovy = 72.0F;
    camera_.projection = CAMERA_PERSPECTIVE;
    assets_.LoadCommon();
    LoadSettings();
    Reset();
}

Game::~Game() = default;

void Game::Reset() {
    EnterMainMenu();
}

void Game::Start(bool captureCursor) {
    practiceMode_ = spellPractice_ = challengeMode_ = false;
    stageId_ = StageId::Stage1;
    StartRun(captureCursor);
}

void Game::StartStage(StageId id, bool captureCursor) {
    practiceMode_ = spellPractice_ = challengeMode_ = false;
    stageId_ = id;
    StartRun(captureCursor);
}

void Game::StartPreview() {
    practiceMode_ = spellPractice_ = challengeMode_ = false;
    StartRun(false, true);
    previewMode_ = true;
    player_.SetInvulnerability(1000.0F);
}

void Game::SetPreviewView(float yawDegrees, float pitchDegrees) {
    constexpr float degreesToRadians = std::numbers::pi_v<float> / 180.0F;
    player_.SetView(yawDegrees * degreesToRadians,
                    pitchDegrees * degreesToRadians);
    player_.SyncCamera(camera_);
}

void Game::OpenOptions() {
    if (state_ != RunState::MainMenu) EnterMainMenu();
    state_ = RunState::Options;
    optionsPage_ = OptionsPage::General;
    selectedOption_ = 0;
    selectedBinding_ = 0;
    waitingForBinding_ = false;
    EnableCursor();
}

void Game::LoadSettings() {
    const GameSettings settings = LoadOrCreateSettings(kSettingsFilename);
    bindings_ = settings.bindings;
    musicVolume_ = settings.musicVolume;
    mouseSensitivity_ = settings.mouseSensitivity;
    soundVolume_ = settings.soundVolume;
    initialLives_ = settings.initialLives;
    initialBombs_ = settings.initialBombs;
    bestScore_ = settings.bestScore;
    lastSelection_ = settings.lastSelection;
    selectedShot_ = lastSelection_.shot;
    challengeMode_ = lastSelection_.challenge;
    practicePhase_ = lastSelection_.practicePhase;
    assets_.SetMusicVolumePercent(musicVolume_);
    assets_.SetSoundVolumePercent(soundVolume_);

    const auto fpsLimit = std::find(kSupportedFpsLimits.begin(),
                                    kSupportedFpsLimits.end(),
                                    settings.fpsLimit);
    fpsLimitIndex_ = fpsLimit == kSupportedFpsLimits.end()
                         ? 1
                         : static_cast<int>(std::distance(kSupportedFpsLimits.begin(),
                                                          fpsLimit));
    SetTargetFPS(kSupportedFpsLimits[fpsLimitIndex_]);
}

void Game::SaveSettings() const {
    const GameSettings settings{bindings_, kSupportedFpsLimits[fpsLimitIndex_],
        musicVolume_, mouseSensitivity_, soundVolume_, initialLives_, initialBombs_, bestScore_, lastSelection_};
    if (!SaveSettingsFile(kSettingsFilename, settings)) {
        TraceLog(LOG_WARNING, "SETTINGS: Failed to save %s", kSettingsFilename);
    }
}

void Game::StartRun(bool captureCursor, bool synchronous) {
    previewMode_ = false;
    // 先移除所有借用旧关卡资源的对象，再取消旧任务并释放纹理及清单。
    actorEffects_.Clear();
    bullets_.Reset();
    enemies_.Clear();
    drops_.Clear();
    boss_.reset();
    assets_.UnloadStage();
    captureCursor_ = captureCursor;
    stage_ = StageDirector{};
    stageContent_ = CreateStageContent(stageId_);
    stage_ = StageDirector(stageContent_->Events());
    loadingSeconds_ = 0;
    loadingStarted_ = loadingFailed_ = false;
#ifdef KOUMAKYOU_DEBUG_FEATURES
    loadingShotTest_ = false;
#endif
    if (IsCursorHidden()) ShowCursor();
    if (synchronous) {
        assets_.LoadStage(stageContent_->Assets());
        CompleteStartRun();
    } else {
        state_ = RunState::Loading;
    }
}

void Game::UpdateLoading(float deltaTime) {
    loadingSeconds_ += std::max(0.0F, deltaTime);
    if (loadingFailed_) {
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE))
            EnterMainMenu();
        return;
    }
    try {
        if (!loadingStarted_) {
            assets_.BeginStageLoad(stageContent_->Assets());
            loadingStarted_ = true;
        }
        if (assets_.AdvanceStageLoad()) CompleteStartRun();
    } catch (const std::exception& error) {
        TraceLog(LOG_WARNING, "STAGE: %s", error.what());
        assets_.UnloadStage();
        loadingFailed_ = true;
    }
}

void Game::CompleteStartRun() {
    assets_.PlayStageMusic();
    state_ = RunState::Playing;
    player_.Reset();
    player_.ConfigureRun(initialLives_, initialBombs_, challengeMode_,
                         selectedShot_ == 0 ? ShotType::HomingAmulet : ShotType::Needle);
    missCount_ = bombCount_ = 0;
    spellStartMisses_=spellStartBombs_=0;
    shotSoundTimer_ = 0.0F;
    boss_.reset();
    bullets_.Reset();
    enemies_.Clear();
    drops_.Clear();
    stage_.Reset();
    score_ = 0;
    graze_ = pointItems_ = 0;
    referenceRank_=16; referenceSubrank_=0; rankTime_=0;
    screenFlashTimer_ = 0.0F;
    spellCutInTimer_ = 0.0F;
    spellBonusTimer_=0; spellBonus_=stageBonus_=0;
    dialogueTimer_ = 0.0F;
    dialogueAfterBattle_ = dialogueBossMusic_ = false;
    dialogueActive_ = false;
    dialogueMotion_.Clear();
    bossEventId_.reset();
    dialogueEventId_.reset();
    shouldQuit_ = false;
#ifdef KOUMAKYOU_DEBUG_FEATURES
    shotTestActive_ = false;
#endif
    player_.SyncCamera(camera_);
    if (captureCursor_) DisableCursor();
    messageTimer_=0;
    if (spellPractice_) {
        stage_ = StageDirector{};
        BeginStageBoss(true, 3);
        while (boss_->phase < practicePhase_) boss_->AdvancePhase();
        player_.AdjustPower(Player::kMaxPower);
        spellCutInTimer_ = kSpellCutInDuration;
    }
#ifdef KOUMAKYOU_DEBUG_FEATURES
    if (loadingShotTest_) {
        // 调试场景也等待同一加载流程完成，避免将固定靶清除或触发普通关卡事件。
        stage_ = StageDirector{};
        enemies_.Add(std::make_unique<ShotTestEnemy>());
        shotTestActive_ = true;
        loadingShotTest_ = false;
        SetMessage("SHOT TEST  F5: POWER -8  F6: POWER +8", 3.0F);
    }
#endif
}

void Game::EnterMainMenu() {
    dialogueMotion_.Clear();
    actorEffects_.Clear();
    bullets_.Reset();
    enemies_.Clear();
    drops_.Clear();
    boss_.reset();
    assets_.UnloadStage();
    stage_ = StageDirector{};
    stageContent_.reset();
    assets_.PlayMenuMusic();
    state_ = RunState::MainMenu;
#ifdef KOUMAKYOU_DEBUG_FEATURES
    shotTestActive_ = false;
#endif
    selectedMenuItem_ = 0;
    messageTimer_ = 0.0F;
    dialogueActive_ = false;
    bossEventId_.reset();
    dialogueEventId_.reset();
    player_.ResetPose();
    player_.SyncCamera(camera_);
    // GLFW 后端的 ShowCursor 同时解除捕获，且不会像 EnableCursor 一样强制居中。
    // 已显示光标时不重复切换模式；更换窗口后端时需保持释放捕获但不移动指针的行为。
    if (IsCursorHidden()) ShowCursor();
}

void Game::EndStage(RunState result) {
    if (result == RunState::Won) {
        assets_.PlaySoundEffect("clear");
        if (!practiceMode_ && !challengeMode_ && !previewMode_ && score_ > bestScore_) {
            bestScore_ = static_cast<int>(std::min(score_, 2000000000LL));
            SaveSettings();
        }
    }
    bullets_.Reset();
    ClearEnemiesWithEffects();
    drops_.Clear();
    if (boss_ && boss_->active) {
        actorEffects_.Spawn(boss_->visual, boss_->position, boss_->hitRadius,
                            boss_->presentation, ActorExitReason::StageClear, &boss_->animation.player.Sample());
        boss_->active = false;
    }
    boss_.reset();
    dialogueActive_ = false;
    spellCutInTimer_ = messageTimer_ = 0.0F;
    finishResult_ = result;
    state_ = RunState::Finishing;
}

void Game::ClearEnemiesWithEffects() {
    enemies_.ForEachActive([this](Enemy& enemy) {
        actorEffects_.Spawn(enemy.visual, enemy.position, enemy.hitRadius,
                            enemy.presentation, ActorExitReason::StageClear, &enemy.animation.player.Sample());
        enemy.active = false;
    });
    enemies_.Clear();
}

void Game::TogglePause() {
    if (state_ == RunState::Playing || state_ == RunState::Finishing) {
        selectedPauseItem_ = 0;
        assets_.PlaySoundEffect("menu_cancel");
        pausedState_ = state_;
        state_ = RunState::Paused;
        assets_.PauseMusic();
        EnableCursor();
    } else if (state_ == RunState::Paused) {
        state_ = pausedState_;
        assets_.ResumeMusic();
        if (captureCursor_) DisableCursor();
    }
}

void Game::Update(float deltaTime) {
    assets_.UpdateAudio();
    UpdateState(deltaTime);
    if(state_==RunState::Playing || state_==RunState::Finishing)UpdateBattleAnimations();
    UpdateUiMotion(deltaTime);
}

void Game::UpdateBattleAnimations() {
    const auto sync=[&](battle::Cursor& cursor,VisualId id,double age,int script=-1,std::uint32_t serial=0) {
        const auto& visual=assets_.GetVisual(id);
        if(!visual.archive)return;
        auto selected=visual.animation;if(script>=0)selected.script=script;
        cursor.Sync(*visual.archive,selected,std::max(0.0,age),serial);
    };
    actorEffects_.ForEachActive([&](ActorExitEffect& effect) {
        if(!effect.nativeDefeat)return;
        sync(effect.effectAnimation,effect.visual,effect.age);
        effect.animation=effect.effectAnimation.player.Sample();
        if(effect.effectAnimation.player.Finished())effect.active=false;
    });
    if(state_!=RunState::Playing)return;
    enemies_.ForEachActive([&](Enemy& enemy) {
        sync(enemy.animation,enemy.visual,enemy.animationAge>=0 ? enemy.animationAge : enemy.age,
             enemy.animationScript,enemy.animationRevision);
    });
    if(boss_ && boss_->active)
        sync(boss_->animation,boss_->visual,boss_->animationAge,boss_->animationScript,boss_->animationRevision);
    bullets_.Pool().ForEachActive([&](Bullet& bullet) {
        const auto& visual=assets_.GetVisual(bullet.visual);
        if(!visual.archive)return;
        auto selected=visual.animation;
        double age=bullet.age;
        if(bullet.nativeType>=0 && bullet.age<bullet.spawnVisualSeconds) {
            selected=battle::BulletSpawn(bullet.nativeType,bullet.nativeColor,static_cast<int>(std::round(bullet.spawnVisualSeconds*60)));
        } else if(bullet.nativeType>=0)age=std::max(0.0F,bullet.age-bullet.spawnVisualSeconds);
        const auto& atlas=assets_.LoadBattleAtlas(selected.atlas);
        bullet.animation.Sync(*atlas.archive,selected,age);
    });
    drops_.ForEachActive([&](Pickup& pickup) {
        const auto id=pickup.kind==PickupKind::Star ? stageContent_->StarPickupVisual() :
            pickup.kind==PickupKind::Score ? stageContent_->ScorePickupVisual() :
            pickup.value>=8 ? stageContent_->LargePowerPickupVisual() : stageContent_->PowerPickupVisual();
        sync(pickup.animation,id,pickup.age);
    });
}

void Game::UpdateUiMotion(float deltaTime) {
    if (!dialogueActive_) {
        dialogueMotion_.Clear();
    } else if (state_ == RunState::Playing && stageContent_) {
        const auto lines = CurrentDialogue();
        if (!lines.empty()) {
            const auto& line = lines[DialogueIndex()];
            dialogueMotion_.Advance(line.bossSpeaking, line.showBoss, deltaTime);
        }
    }
    int detail = 0, selection = -1;
    if (state_ != RunState::MainMenu) titleEntrance_.Complete();
    if (state_ == RunState::MainMenu) {
        selection = selectedMenuItem_;
        titleEntrance_.Advance(deltaTime);
    } else if (state_ == RunState::Options) {
        detail = optionsPage_ == OptionsPage::KeyBindings ? 1 : 0;
        selection = detail ? selectedBinding_ : selectedOption_;
    } else if (state_ == RunState::StartSetup) {
        detail = setupStep_;
        selection = setupStep_ == 1 ? selectedShot_ : setupStep_ == 2 ? static_cast<int>(challengeMode_)
            : setupStep_ == 3 ? (practicePhase_ == 2 ? 0 : 1) : 0;
    } else if (state_ == RunState::MusicRoom) selection = selectedTrack_;
    else if (state_ == RunState::Paused) selection = selectedPauseItem_;
    // 同一 RunState 内的难度/机体/模式和设置子页独立进场，选择编号只在当前页有效。
    menuMotion_.Advance(static_cast<int>(state_) * 16 + detail, selection, deltaTime);
}

void Game::UpdateState(float deltaTime) {
    if (state_ == RunState::Loading) {
        UpdateLoading(deltaTime);
        return; // 完成加载的这一帧也不更新战斗，下一帧从关卡时间零开始。
    }
    if (state_ == RunState::MainMenu) {
        UpdateMainMenu();
        return;
    }
    if (state_ == RunState::Options) {
        UpdateOptions();
        return;
    }
    if (state_ == RunState::StartSetup || state_ == RunState::MusicRoom ||
        state_ == RunState::Score || state_ == RunState::Credit) {
        UpdateMenuPage();
        return;
    }
#ifdef KOUMAKYOU_DEBUG_FEATURES
    if (IsKeyPressed(KEY_F3)) debugRender_ = !debugRender_;
#endif

    if (state_ == RunState::Won || state_ == RunState::GameOver) {
        if (IsKeyPressed(KEY_R)) {
            StartRun();
        } else if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_M)) {
            EnterMainMenu();
        }
        return;
    }

    if (state_ == RunState::Paused) {
        if (IsKeyPressed(KEY_ESCAPE) || BindingPressed(bindings_.pause)) { TogglePause(); return; }
        if (IsKeyPressed(KEY_M)) { EnterMainMenu(); return; }
        const int previous = selectedPauseItem_;
        if (IsKeyPressed(KEY_UP)) selectedPauseItem_ = (selectedPauseItem_ + menu::kPauseCount - 1) % menu::kPauseCount;
        if (IsKeyPressed(KEY_DOWN)) selectedPauseItem_ = (selectedPauseItem_ + 1) % menu::kPauseCount;
        if (selectedPauseItem_ != previous) assets_.PlaySoundEffect("menu_move");
        bool confirm = IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER);
        const menu::OverlayLayout layout(GetScreenWidth(),GetScreenHeight());
        for (int index = 0; index < menu::kPauseCount; ++index) {
            const auto row=layout.PauseRow(index);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), row)) {
                selectedPauseItem_ = index;
                confirm = true;
            }
        }
        if (confirm) {
            assets_.PlaySoundEffect("menu_confirm");
            switch (selectedPauseItem_) {
                case 0: TogglePause(); break;
                case 1: EnterMainMenu(); break;
                case 2: StartRun(); break;
            }
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) || BindingPressed(bindings_.pause)) {
        TogglePause();
    }
    if (state_ == RunState::Finishing) {
        screenFlashTimer_ = std::max(0.0F, screenFlashTimer_ - deltaTime);
        actorEffects_.Update(deltaTime);
        // ANM 删除指令也会结束效果，必须先执行再判断清空，保证本帧完成结算。
        UpdateBattleAnimations();
        if (actorEffects_.Pool().ActiveCount() == 0) {
            actorEffects_.Clear();
            assets_.UnloadStage();
            state_ = finishResult_;
            EnableCursor();
        }
        return;
    }
    if (state_ != RunState::Playing) {
        if (state_ == RunState::Paused && IsKeyPressed(KEY_M)) {
            EnterMainMenu();
        }
        return;
    }
#ifdef KOUMAKYOU_DEBUG_FEATURES
    if (IsKeyPressed(KEY_F4)) debugInvincible_ = !debugInvincible_;
    if (IsKeyPressed(KEY_F5)) AdjustPlayerPower(-8);
    if (IsKeyPressed(KEY_F6)) AdjustPlayerPower(8);
#endif

    messageTimer_ = std::max(0.0F, messageTimer_ - deltaTime);
    screenFlashTimer_ = std::max(0.0F, screenFlashTimer_ - deltaTime);
    spellCutInTimer_ = std::max(0.0F, spellCutInTimer_ - deltaTime);
    spellBonusTimer_=std::max(0.0F,spellBonusTimer_-deltaTime);
    // 先推进旧效果，本帧产生的快照下一帧开始计时，避免大 dt 吞掉退场首帧。
    actorEffects_.Update(deltaTime);

    const bool dialogueWasActive = dialogueActive_;
    stage_.Update(deltaTime,
                  [this](const StageEvent& event) {
                      return HandleStageEvent(event);
                  });
    // 本帧由时间轴刚启动的对话从下一帧计时，避免第一句少显示一个 deltaTime。
    if (dialogueWasActive && dialogueActive_) {
        dialogueTimer_ += deltaTime;
        float lineStart=0;
        const auto lines=CurrentDialogue();
        const auto& line=lines[DialogueIndex(&lineStart)];
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER))
            dialogueTimer_=lineStart+(line.duration>0 ? line.duration : stageContent_->DialogueLineDuration());
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
            dialogueTimer_=DialogueDuration();
        if (!dialogueAfterBattle_ && DialogueIndex()>=13 && !dialogueBossMusic_) {
            assets_.PlayBossMusic(); dialogueBossMusic_=true;
        }
        if (dialogueTimer_+0.00001F>=DialogueDuration()) {
            dialogueActive_=false;
            if (dialogueEventId_) { stage_.EndEvent(*dialogueEventId_); dialogueEventId_.reset(); }
            if (dialogueAfterBattle_) { EndStage(RunState::Won); return; }
        }
    }
    if(!dialogueActive_) {
        rankTime_+=deltaTime;
        const float period=std::max(4.0F,40.0F-std::max(0,player_.RemainingLives()-1)*4.0F);
        while(rankTime_>=period) { rankTime_-=period; AdjustReferenceRank(100); }
    }
    UpdatePlayer(deltaTime);
    UpdateEnemies(deltaTime);
    UpdateBoss(deltaTime);
    if (state_ != RunState::Playing) return;
    std::array<Vector3, kEnemyCapacity + 1> targets{};
    std::size_t targetCount = 0;
    enemies_.ForEachActive([&](const Enemy& enemy) {
        targets[targetCount++] = enemy.position;
    });
    if (boss_ && boss_->active) targets[targetCount++] = boss_->position;
    bullets_.Update(deltaTime, {targets.data(), targetCount}, player_.Position());
    UpdateBattleAnimations(); // 本帧击破快照读取已推进的动作，而不是上一帧的动画。
    ResolveCollisions();
    if (state_ == RunState::Playing) UpdatePickups(deltaTime);
}

#ifdef KOUMAKYOU_DEBUG_FEATURES
void Game::StartShotTest() {
    stageId_ = StageId::Stage1;
    StartRun();
    loadingShotTest_ = true;
}
#endif

bool Game::HandleStageEvent(const StageEvent& event) {
    return stageContent_->HandleEvent(event, *this);
}

void Game::SpawnStageEnemy(const EnemySpec& spec) {
    // 新典道中 Boss 存在时，出生请求被跳过，但时间轴仍继续计时。
    if (boss_ && boss_->active) return;
    if (auto enemy = stageContent_->CreateEnemy(spec))
        enemies_.Add(std::move(enemy));
}

void Game::BeginStageBoss(bool final, StageEventId id) {
    // 只停止旧批次的后续出怪；现存小怪和弹幕继续更新，不能在 Boss 入场时清场。
    stage_.CancelEnemySpawns();
    bossEventId_ = id;
    boss_ = stageContent_->CreateBoss(final);
    if (final && !dialogueBossMusic_) assets_.PlayBossMusic();
    messageTimer_=0;
}

void Game::BeginStageDialogue(StageEventId id, bool afterBattle) {
    dialogueAfterBattle_=afterBattle;
    if(afterBattle) {
        // 一面沿用基础 Normal 结算公式；3D 拾取/擦弹数采用当前世界的实际结果。
        stageBonus_=(1000LL+graze_*10LL+player_.Power()*100LL)*pointItems_;
        score_+=stageBonus_;
    }
    dialogueBossMusic_=false;
    stage_.CancelEnemySpawns();
    ClearEnemiesWithEffects();
    bullets_.ClearEnemyBullets();
    dialogueActive_ = true;
    dialogueTimer_ = 0.0F;
    dialogueEventId_ = id;
    const auto lines = CurrentDialogue();
    if (!lines.empty()) dialogueMotion_.Reset(lines.front().bossSpeaking, lines.front().showBoss);
    messageTimer_ = 0.0F;
}

void Game::AdjustReferenceRank(int subrank) {
    referenceSubrank_+=subrank;
    while(referenceSubrank_>=100) { ++referenceRank_; referenceSubrank_-=100; }
    while(referenceSubrank_<0) { --referenceRank_; referenceSubrank_+=100; }
    referenceRank_=std::clamp(referenceRank_,10,32);
}
std::span<const DialogueLine> Game::CurrentDialogue() const { return stageContent_->Dialogue(dialogueAfterBattle_); }
int Game::DialogueIndex(float* lineStart) const {
    float start=0; const auto lines=CurrentDialogue();
    for (int index=0;index<static_cast<int>(lines.size());++index) {
        const float duration=lines[index].duration>0 ? lines[index].duration : stageContent_->DialogueLineDuration();
        if (dialogueTimer_<start+duration || index==static_cast<int>(lines.size())-1) {
            if(lineStart)*lineStart=start; return index;
        }
        start+=duration;
    }
    return 0;
}
float Game::DialogueDuration() const {
    float duration=0; for(const auto& line:CurrentDialogue()) duration+=line.duration>0 ? line.duration : stageContent_->DialogueLineDuration();
    return duration;
}
void Game::StageMessage(std::string_view message, float duration) {
    SetMessage(std::string(message), duration);
}

void Game::SetMessage(std::string message, float duration) {
    message_ = std::move(message);
    messageTimer_ = duration;
}

}  // namespace koumakyou
