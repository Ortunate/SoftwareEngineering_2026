// 本文件负责游戏生命周期、主循环调度和关卡事件转发。

#include "app/game.hpp"
#include "app/constants.hpp"
#include "ui/components.hpp"
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
    // EnableCursor 会强制居中；设置页仅需显示光标，保留用户进入页面前的位置。
    if (IsCursorHidden()) ShowCursor();
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
    stageHighScores_ = settings.stageHighScores;
    scoreProfile_=score::Load(score::kFilename);
    lastSelection_ = settings.lastSelection;
    spellSelection_=settings.spellSelection;
    spellRecords_=settings.spellRecords;
    selectedShot_ = lastSelection_.shot;
    selectedCharacter_ = lastSelection_.character;
    challengeMode_ = lastSelection_.challenge;
    practiceAttack_ = lastSelection_.practicePhase;
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
        musicVolume_, mouseSensitivity_, soundVolume_, initialLives_, initialBombs_, bestScore_, lastSelection_, stageHighScores_, spellSelection_, spellRecords_};
    if (!SaveSettingsFile(kSettingsFilename, settings)) {
        TraceLog(LOG_WARNING, "SETTINGS: Failed to save %s", kSettingsFilename);
    }
}

void Game::StartRun(bool captureCursor, bool synchronous,bool continuingStage) {
    FinishSpellRecord(false);
    spellCaptured_=false;
    previewMode_ = false;
    continuingStage_=continuingStage;
    if(!continuingStage){clearedMainlineStages_=0;runContinues_=0;}
    pendingEnding_.reset();endingPlayer_=ending::Player{};assets_.SetMusicGain(1);
    // 先移除所有借用旧关卡资源的对象，再取消旧任务并释放纹理及清单。
    actorEffects_.Clear();
    bullets_.Reset();
    actors_.Clear();
    drops_.Clear();
    boss_.reset();
    playerOptionAnimations_ = {};
    ClearPlayerBomb();
    assets_.UnloadStage();
    captureCursor_ = captureCursor;
    stage_ = StageDirector{};
    stageContent_ = CreateStageContent(stageId_);
    stage_ = StageDirector(stageContent_->Events());
    scenerySeconds_=0;
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
        const auto input = ui::MenuInput::Read();
        if (input.confirm || input.click || input.back) EnterMainMenu();
        return;
    }
    try {
        if (!loadingStarted_) {
            assets_.BeginStageLoad(pendingEnding_?ending::Assets(*pendingEnding_):stageContent_->Assets());
            loadingStarted_ = true;
        }
        if (assets_.AdvanceStageLoad()) {
            if(pendingEnding_) {
                const auto id=*pendingEnding_;
                endingPlayer_.Start(id,scoreProfile_.endingsSeen[static_cast<std::size_t>(id)]);
                assets_.PlayStageMusic();state_=RunState::Ending;
            } else CompleteStartRun();
        }
    } catch (const std::exception& error) {
        TraceLog(LOG_WARNING, "STAGE: %s", error.what());
        playerOptionAnimations_ = {};
        ClearPlayerBomb();
        assets_.UnloadStage();
        loadingFailed_ = true;
    }
}

void Game::CompleteStartRun() {
    assets_.PlayStageMusic();
    state_ = RunState::Playing;
    playerOptionAnimations_ = {};
    if(!continuingStage_) {
        player_.Reset();
        player_.ConfigureRun(initialLives_, initialBombs_, challengeMode_,static_cast<ShotType>(selectedCharacter_*2+selectedShot_));
        missCount_=bombCount_=0;score_=0;graze_=pointItems_=0;referenceRank_=16;
    } else {
        // 自机不借用关卡素材；只重置姿态和射击周期，库存、火力与机体继续沿用这一局。
        player_.ResetPose();player_.SetInvulnerability(1.5F);
    }
    spellStartMisses_=spellStartBombs_=0;
    shotSoundTimer_ = 0.0F;
    boss_.reset();
    bullets_.Reset();
    actors_.Clear();
    drops_.Clear();
    stage_.Reset();
    battleTicks_=0;pendingPlayerInput_={};pendingDialogueConfirm_=false;
    stageGraze_=stagePointItems_=0;
    referenceSubrank_=0;rankTime_=0;
    continuingStage_=false;
    screenFlashTimer_ = 0.0F;
    spellCutInTimer_ = 0.0F;
    spellBonusTimer_=0; spellBonus_=stageBonus_=0;
    stageClearSummary_={}; stageClearSeconds_=0; stageClearShown_=false;
    dialogueTimer_ = 0.0F;
    dialogueAfterBattle_ = dialogueBossMusic_ = false;
    dialogueScene_=DialogueScene::FinalBoss;
    dialogueActive_ = false;
    dialogueMotion_.Clear();
    bossEventId_.reset();
    dialogueEventId_.reset();
    dialogueSignalWait_.reset();dialogueSignalsSeen_=0;
    shouldQuit_ = false;
#ifdef KOUMAKYOU_DEBUG_FEATURES
    shotTestActive_ = false;
#endif
    player_.SyncCamera(camera_);
    if (captureCursor_) DisableCursor();
    messageTimer_=0;
    if (spellPractice_) {
        stage_ = StageDirector{};
        BeginStageBoss(stageContent_->PracticeBoss(), 3);
        while (boss_->attack < practiceAttack_) boss_->AdvanceSkill();
        player_.AdjustPower(Player::kMaxPower);
        spellCutInTimer_ = kSpellCutInDuration;
    }
#ifdef KOUMAKYOU_DEBUG_FEATURES
    if (loadingShotTest_) {
        // 调试场景也等待同一加载流程完成，避免将固定靶清除或触发普通关卡事件。
        stage_ = StageDirector{};
        behavior::Spawn target;
        target.position={0,0,4};target.pattern.movement.speed=0;target.pattern.initialAnimation=3;target.pattern.endFrame=2147483647;
        target.sourceHealth=240;target.sourceScore=0;target.sourceItem=-2;
        target.visual=stage1::kDarkFairy;target.hitRadius=1;target.retireOutside=false;
        actors_.Spawn(target);
        shotTestActive_ = true;
        loadingShotTest_ = false;
        SetMessage("SHOT TEST  F5: POWER -8  F6: POWER +8", 3.0F);
    }
#endif
}

void Game::EnterMainMenu() {
    continuingStage_=false;
    pendingEnding_.reset();endingPlayer_=ending::Player{};clearedMainlineStages_=0;runContinues_=0;
    scorePageFromRun_=false;scoreHighlight_=-1;
    FinishSpellRecord(false);
    dialogueMotion_.Clear();
    actorEffects_.Clear();
    bullets_.Reset();
    actors_.Clear();
    drops_.Clear();
    boss_.reset();
    playerOptionAnimations_ = {};
    ClearPlayerBomb();
    assets_.UnloadStage();
    stage_ = StageDirector{};
    stageContent_.reset();
    assets_.SetMusicGain(1);
    assets_.PlayMenuMusic();
    state_ = RunState::MainMenu;
#ifdef KOUMAKYOU_DEBUG_FEATURES
    shotTestActive_ = false;
#endif
    messageTimer_ = 0.0F;
    dialogueActive_ = false;
    bossEventId_.reset();
    dialogueEventId_.reset();
    dialogueSignalWait_.reset();dialogueSignalsSeen_=0;
    player_.ResetPose();
    player_.SyncCamera(camera_);
    // GLFW 后端的 ShowCursor 同时解除捕获，且不会像 EnableCursor 一样强制居中。
    // 已显示光标时不重复切换模式；更换窗口后端时需保持释放捕获但不移动指针的行为。
    if (IsCursorHidden()) ShowCursor();
}

void Game::ReturnToPracticeStage() {
    if(!practiceMode_ || spellPractice_) { EnterMainMenu(); return; }
    const int stage=StageNumber(stageId_)-1;
    EnterMainMenu();
    selectedMenuItem_=2;
    selectedPracticeStage_=stage>=0 && stage<kStageScoreCount && kPracticeStages[stage]?stage:0;
    setupStep_=StartSetupStep::Stage;
    state_=RunState::StartSetup;
    // 返回战斗入口时直接显示完整选择背景，避免先闪回一次首页再淡入人物。
    startSetupMotion_.Advance(static_cast<int>(setupStep_),1);
}

void Game::BeginStageClear() {
    if(stageClearShown_)return;
    // 对话完整结束后再固定奖励；此入口可重入，但不能重复加分或重新启动倒计时。
    stageClearSummary_={std::max(1,StageNumber(stageId_)),player_.Power(),stageGraze_,stagePointItems_};
    stageBonus_=stageClearSummary_.Total();
    score_+=stageBonus_;
    stageClearShown_=true;
    RecordStageScore();
    bullets_.Reset(); drops_.Clear();ClearPlayerBomb();
    spellCutInTimer_=screenFlashTimer_=0;
    state_=RunState::StageClear;
    stageClearSeconds_=0;
    if(IsCursorHidden())ShowCursor();
}

void Game::ContinueStageClear() {
    if(state_!=RunState::StageClear)return;
    if(!practiceMode_&&!spellPractice_&&!previewMode_) {
        const int number=StageNumber(stageId_);
        // 只接纳当前完整前缀的下一关；从末关直接启动不能冒充整轮通关，重入也不会累计次数。
        if(number>=1 && number<=6 && clearedMainlineStages_==((1U<<(number-1))-1))
            clearedMainlineStages_|=1U<<(number-1);
        if(const auto next=NextStage(stageId_)) {
            stageId_=*next;StartRun(captureCursor_,false,true);return;
        }
    }
    pendingEnding_=ending::Select({static_cast<int>(player_.Shot()),1,runContinues_,missCount_,challengeMode_,
        !practiceMode_&&!spellPractice_&&!previewMode_ && clearedMainlineStages_==0x3f});
    EndStage(RunState::Won);
}

void Game::RecordStageScore() {
    // 分模式记录包含通关与失败的整关尝试；预览和符卡练习的计分口径不同，不能混入。
    bool recordStage=!previewMode_ && !spellPractice_;
#ifdef KOUMAKYOU_DEBUG_FEATURES
    recordStage=recordStage && !shotTestActive_;
#endif
    if(recordStage && stageHighScores_.Record(challengeMode_,StageNumber(stageId_),score_))
        SaveSettings();
}

void Game::EndStage(RunState result) {
    FinishSpellRecord(false);
    RecordStageScore();
    if (result == RunState::Won) {
        assets_.PlaySoundEffect("clear");
        if (!pendingEnding_ && !practiceMode_ && !challengeMode_ && !previewMode_ && score_ > bestScore_) {
            bestScore_ = static_cast<int>(std::min(score_, 2000000000LL));
            SaveSettings();
        }
    }
    bullets_.Reset();
    ClearActorsWithEffects();
    drops_.Clear();
    if (boss_ && boss_->active) {
        actorEffects_.Spawn(boss_->visual, boss_->position, boss_->hitRadius,
                            boss_->presentation, ActorExitReason::StageClear, &boss_->animation.player.Sample());
        boss_->active = false;
    }
    boss_.reset();
    dialogueActive_ = false;
    spellCutInTimer_ = messageTimer_ = 0.0F;
    dialogueSignalWait_.reset();dialogueSignalsSeen_=0;
    selectedResultItem_ = spellPractice_?2:0;
    ClearPlayerBomb();
    finishResult_ = result;
    state_ = RunState::Finishing;
}

void Game::ClearActorsWithEffects() {
    actors_.ForEachActive([this](behavior::Actor& enemy) {
        if(enemy.visible)actorEffects_.Spawn(enemy.visual, enemy.position, enemy.hitRadius,
                            enemy.presentation, ActorExitReason::StageClear, &enemy.animation.player.Sample());
        enemy.active = false;
    });
    actors_.Clear();
}

void Game::TogglePause() {
    pendingPlayerInput_={};pendingDialogueConfirm_=false;
    if (state_ == RunState::Playing || state_ == RunState::StageClear || state_ == RunState::Finishing) {
        selectedPauseItem_ = 0;
        assets_.PlaySoundEffect("menu_cancel");
        pausedState_ = state_;
        state_ = RunState::Paused;
        assets_.PauseMusic();
        PausePlayerBombSounds(true);
        EnableCursor();
    } else if (state_ == RunState::Paused) {
        state_ = pausedState_;
        assets_.ResumeMusic();
        PausePlayerBombSounds(false);
        if (captureCursor_ && state_!=RunState::StageClear) DisableCursor();
    }
}

void Game::Update(float deltaTime) {
    assets_.UpdateAudio();
    UpdateState(deltaTime);
    if(state_==RunState::Playing || state_==RunState::StageClear || state_==RunState::Finishing)UpdateBattleAnimations();
    UpdateUiMotion(deltaTime);
}

void Game::UpdateBattleAnimations() {
    const auto sync=[&](battle::Cursor& cursor,VisualId id,double age,int script=-1,std::uint32_t serial=0,const battle::AnimationInterrupt* interrupt=nullptr) {
        const auto& visual=assets_.GetVisual(id);
        if(!visual.archive)return;
        auto selected=visual.animation;if(script>=0)selected.script=script;
        cursor.Sync(*visual.archive,selected,std::max(0.0,age),serial,interrupt);
    };
    actorEffects_.ForEachActive([&](ActorExitEffect& effect) {
        if(!effect.nativeDefeat)return;
        sync(effect.effectAnimation,effect.visual,effect.age);
        effect.animation=effect.effectAnimation.player.Sample();
        if(effect.effectAnimation.player.Finished())effect.active=false;
    });
    if(state_!=RunState::Playing)return;
    if (stageContent_ && player_.OptionsVisible()) {
        for (int side = 0; side < 2; ++side) {
            const auto visual = stageContent_->PlayerOptionVisual(side,player_.Shot());
            if (visual != kInvalidVisualId) sync(playerOptionAnimations_[side], visual, player_.VisualAge());
        }
    }
    actors_.ForEachActive([&](behavior::Actor& enemy) {
        sync(enemy.animation,enemy.visual,enemy.animationAge>=0 ? enemy.animationAge : enemy.age,
             enemy.animationScript,enemy.animationRevision,enemy.animationInterrupt ? &*enemy.animationInterrupt : nullptr);
    });
    if(boss_ && boss_->active)
        sync(boss_->animation,boss_->visual,boss_->animationAge,boss_->animationScript,boss_->animationRevision,boss_->AnimationInterrupt());
    bullets_.Pool().ForEachActive([&](Bullet& bullet) {
        const auto& visual=assets_.GetVisual(bullet.visual);
        if(!visual.archive)return;
        auto selected=visual.animation;
        if(bullet.IsBeam()&&bullet.beamAnimationScript>=0)selected.script=bullet.beamAnimationScript;
        double age=bullet.age;
        if(bullet.nativeType>=0 && bullet.age<bullet.spawnVisualSeconds) {
            selected=battle::BulletSpawn(bullet.nativeType,bullet.nativeColor,static_cast<int>(std::round(bullet.spawnVisualSeconds*60)));
        } else if(bullet.nativeType>=0)age=std::max(0.0F,bullet.age-bullet.spawnVisualSeconds);
        const auto& atlas=assets_.LoadBattleAtlas(selected.atlas);
        if(bullet.playerShotKind==PlayerShotKind::Laser && !bullet.laserFadeStarted && age>=bullet.laserFadeAt) {
            // 先推进到中断发生点，再推进余下时间；低帧率跨过淡出边界时不能延迟或重复中断。
            bullet.animation.Sync(*atlas.archive,selected,bullet.laserFadeAt);
            bullet.animation.player.Interrupt(1);bullet.laserFadeStarted=true;
        }
        bullet.animation.Sync(*atlas.archive,selected,age);
    });
    drops_.ForEachActive([&](Pickup& pickup) {
        sync(pickup.animation,PickupVisual(pickup),pickup.age);
    });
}
VisualId Game::PickupVisual(const Pickup& pickup) const {
    switch(pickup.kind) {
        case PickupKind::Power:return pickup.value>=8 ? stageContent_->LargePowerPickupVisual() : stageContent_->PowerPickupVisual();
        case PickupKind::Score:return stageContent_->ScorePickupVisual();
        case PickupKind::Star:return stageContent_->StarPickupVisual();
        case PickupKind::Bomb:return stageContent_->ItemVisual(3);
        case PickupKind::FullPower:return stageContent_->ItemVisual(4);
        case PickupKind::Life:return stageContent_->ItemVisual(5);
    }
    return kInvalidVisualId;
}

void Game::UpdateUiMotion(float deltaTime) {
    if (!dialogueActive_) {
        dialogueMotion_.Clear();
    } else if (state_ == RunState::Playing && stageContent_) {
        const auto lines = CurrentDialogue();
        if (!lines.empty()) {
            const auto& line = lines[DialogueIndex()];
            const auto cues=stageContent_->SceneDialogueCues(dialogueScene_,dialogueAfterBattle_,player_.Shot());
            if(cues.empty())dialogueMotion_.Advance(line.bossSpeaking, line.showBoss, deltaTime);
            else dialogueMotion_.Seek(cues,dialogueTimer_);
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
        detail = static_cast<int>(setupStep_);
        selection = setupStep_ == StartSetupStep::Shot ? selectedShot_ : setupStep_ == StartSetupStep::Mode ? static_cast<int>(challengeMode_)
            : setupStep_ == StartSetupStep::Difficulty ? 1 : setupStep_ == StartSetupStep::Stage ? selectedPracticeStage_ : 0;
        if(spellPractice_) {
            detail=spellSelection_.page==spell::Page::Stage?8:9;
            selection=spellSelection_.page==spell::Page::Stage?spellSelection_.stage:
                spellSelection_.page==spell::Page::Group?spellSelection_.group:spellSelection_.difficulty;
        }
    } else if (state_ == RunState::MusicRoom) selection = selectedTrack_;
    else if (state_ == RunState::Paused) selection = selectedPauseItem_;
    else if (state_ == RunState::Won || state_ == RunState::GameOver) selection = selectedResultItem_;
    if (state_ == RunState::Score || state_ == RunState::Credit) selection = 0;
    // 同一 RunState 内的难度/机体/模式和设置子页独立进场，选择编号只在当前页有效。
    menuMotion_.Advance(static_cast<int>(state_) * 16 + detail, selection, deltaTime);
    startSetupMotion_.Advance(state_==RunState::MainMenu?-1:state_==RunState::StartSetup?static_cast<int>(setupStep_):-2,deltaTime);
    for(const auto step:{StartSetupStep::Shot,StartSetupStep::Mode,StartSetupStep::Stage}) {
        if(startSetupMotion_.Layer(step)<=0 && !(state_==RunState::StartSetup && setupStep_==step))continue;
        const int selected=step==StartSetupStep::Shot?selectedShot_:step==StartSetupStep::Mode?int(challengeMode_):
            selectedPracticeStage_;
        setupChoiceMotion_[static_cast<int>(step)].Advance(static_cast<int>(step),selected,deltaTime);
    }
    spellCharacterMotion_.Advance(0,spellSelection_.character,deltaTime);
    const bool spellColumns=state_==RunState::StartSetup && spellPractice_ && spellSelection_.page!=spell::Page::Stage;
    // 两列各自强调当前选项，焦点变化只改变强弱；统一的选卡页编号保证整页不随递进/返回闪烁。
    spellGroupMotion_.Advance(spellColumns?spellSelection_.stage:-1,spellColumns?spellSelection_.group:-1,deltaTime);
    spellDifficultyMotion_.Advance(spellColumns?spellSelection_.stage:-1,spellColumns?spellSelection_.difficulty:-1,deltaTime);
    startCharacterMotion_.Advance(0,selectedCharacter_,deltaTime);
    if(state_==RunState::MainMenu)titleMenuMotion_=menuMotion_;
}

void Game::UpdateState(float deltaTime) {
    if(state_==RunState::Ending){UpdateEnding(deltaTime);return;}
    if(state_==RunState::ScoreSave){UpdateScoreSave();return;}
    if(state_==RunState::ScoreName){UpdateScoreName();return;}
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

    if(state_==RunState::StageClear) {
        const auto input=ui::MenuInput::Read();
        if(input.back || (BindingPressed(bindings_.pause) && !input.confirm && !input.click)) {
            TogglePause(); return;
        }
        stageClearSeconds_=std::min(kStageClearSeconds,stageClearSeconds_+std::max(0.0F,deltaTime));
        actorEffects_.Update(std::max(0.0F,deltaTime));
        if(input.confirm || input.click || stageClearSeconds_>=kStageClearSeconds)ContinueStageClear();
        return; // 确认只关闭本层，下一帧才推进收尾，避免操作穿透到后续菜单。
    }

    if (state_ == RunState::Won || state_ == RunState::GameOver) {
        const auto input = ui::MenuInput::Read();
        if(spellPractice_) {
            if(input.back) {ReturnToSpellPractice();return;}
            if(IsKeyPressed(KEY_M)) {EnterMainMenu();return;}
            const menu::OverlayLayout layout(GetScreenWidth(),GetScreenHeight());
            std::array<ui::Item,3> items{};
            for(int index=0;index<3;++index)items[index]={index,layout.SpellResultRow(index),menu::kSpellResultAvailable[index]};
            const auto selection=ui::UpdateSelection(items,input,selectedResultItem_);
            if(selection.changed)assets_.PlaySoundEffect("menu_move");
            bool confirm=selection.confirmed;
            if(IsKeyPressed(KEY_R)) {selectedResultItem_=2;confirm=true;}
            if(confirm) {
                assets_.PlaySoundEffect("menu_confirm");
                if(selectedResultItem_==2)StartRun();else ReturnToSpellPractice();
            }
            return;
        }
        if (input.back) {
            if(practiceMode_ && !spellPractice_)ReturnToPracticeStage(); else EnterMainMenu();
            return;
        }
        if(IsKeyPressed(KEY_M)) { EnterMainMenu(); return; }
        const menu::OverlayLayout layout(GetScreenWidth(),GetScreenHeight());
        std::array<ui::Item,menu::kResultCount> items{};
        for (int index = 0; index < menu::kResultCount; ++index)
            items[index]={index,layout.ResultRow(index)};
        const auto selection=ui::UpdateSelection(items,input,selectedResultItem_);
        bool confirm=selection.confirmed;
        if(selection.changed)assets_.PlaySoundEffect("menu_move");
        if (IsKeyPressed(KEY_R)) { selectedResultItem_ = 0; confirm = true; }
        if (confirm) {
            assets_.PlaySoundEffect("menu_confirm");
            if (selectedResultItem_ == 0) StartRun();
            else if(practiceMode_ && !spellPractice_)ReturnToPracticeStage();
            else EnterMainMenu();
        }
        return;
    }

    if (state_ == RunState::Paused) {
        const auto input = ui::MenuInput::Read();
        // 自定义暂停键不能覆盖 UI 固定的导航和确认操作。
        if (input.back || (BindingPressed(bindings_.pause) && !input.confirm && !input.click && input.Direction()==0)) {
            TogglePause(); return;
        }
        if (IsKeyPressed(KEY_M)) { EnterMainMenu(); return; }
        const int count=static_cast<int>(menu::PauseLabels(practiceMode_ && !spellPractice_,spellPractice_).size());
        const menu::OverlayLayout layout(GetScreenWidth(),GetScreenHeight());
        std::array<ui::Item,4> items{};
        for (int index = 0; index < count; ++index)
            items[index]={index,layout.PauseRow(index)};
        const auto selection=ui::UpdateSelection(std::span(items).first(count),input,selectedPauseItem_);
        if(selection.changed)assets_.PlaySoundEffect("menu_move");
        if (selection.confirmed) {
            assets_.PlaySoundEffect("menu_confirm");
            switch (selectedPauseItem_) {
                case 0: TogglePause(); break;
                case 1: EnterMainMenu(); break;
                case 2: StartRun(); break;
                case 3: if(spellPractice_)ReturnToSpellPractice();else ReturnToPracticeStage(); break;
            }
        }
        return;
    }

    // 对话的返回操作打开暂停菜单；普通战斗右键仍是可配置的低速移动。
    const auto dialogueInput = ui::MenuInput::Read();
    const bool pauseBinding = BindingPressed(bindings_.pause) &&
        (!dialogueActive_ || (!dialogueInput.confirm && !dialogueInput.click));
    if (IsKeyPressed(KEY_ESCAPE) || pauseBinding || (dialogueActive_ && dialogueInput.back)) {
        TogglePause();
        return;
    }
    if (state_ == RunState::Finishing) {
        screenFlashTimer_ = std::max(0.0F, screenFlashTimer_ - deltaTime);
        actorEffects_.Update(deltaTime);
        // ANM 删除指令也会结束效果，必须先执行再判断清空，保证本帧完成结算。
        UpdateBattleAnimations();
        if (actorEffects_.Pool().ActiveCount() == 0) {
            actorEffects_.Clear();
            playerOptionAnimations_ = {};
            ClearPlayerBomb();
            assets_.UnloadStage();
            if(finishResult_==RunState::Won && practiceMode_ && !spellPractice_)ReturnToPracticeStage();
            else if(finishResult_==RunState::Won && pendingEnding_)BeginEnding();
            else { state_ = finishResult_; EnableCursor(); }
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

    // 发弹、弹池推进和碰撞共享固定步，低 FPS 补发的弹只推进各自实际经过的逻辑步。
    const PlayerInput latest=ReadPlayerInput();
    const Vector2 accumulatedMouse{pendingPlayerInput_.mouseDelta.x+latest.mouseDelta.x,
                                   pendingPlayerInput_.mouseDelta.y+latest.mouseDelta.y};
    const bool bomb=pendingPlayerInput_.bomb||latest.bomb;
    pendingPlayerInput_=latest;pendingPlayerInput_.mouseDelta=accumulatedMouse;pendingPlayerInput_.bomb=bomb;
    pendingDialogueConfirm_=pendingDialogueConfirm_||dialogueInput.confirm||dialogueInput.click;
    const bool fastForward=IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL);
    battleTicks_+=static_cast<double>(deltaTime)*120;
    while(battleTicks_+0.00001>=1&&state_==RunState::Playing) {
        battleTicks_-=1;
        const PlayerInput stepInput=pendingPlayerInput_;
        const bool confirm=std::exchange(pendingDialogueConfirm_,false);
        pendingPlayerInput_.mouseDelta={};pendingPlayerInput_.bomb=false;
        UpdateBattleStep(1.0F/120,stepInput,confirm,fastForward);
    }
}

void Game::UpdateBattleStep(float deltaTime,const PlayerInput& input,bool dialogueConfirm,bool fastForward) {
    messageTimer_ = std::max(0.0F, messageTimer_ - deltaTime);
    screenFlashTimer_ = std::max(0.0F, screenFlashTimer_ - deltaTime);
    spellCutInTimer_ = std::max(0.0F, spellCutInTimer_ - deltaTime);
    spellBonusTimer_=std::max(0.0F,spellBonusTimer_-deltaTime);
    // 先推进旧效果，本帧产生的快照下一帧开始计时，避免大 dt 吞掉退场首帧。
    actorEffects_.Update(deltaTime);

    BeginSpellRecord();
    const bool dialogueWasActive = dialogueActive_;
    stage_.Update(deltaTime,
                  [this](const StageEvent& event) {
                      return HandleStageEvent(event);
                  });
    if(state_!=RunState::Playing)return; // 事件改变状态时不能再推进本帧战斗。
    // 本步刚启动的对话从下一逻辑步计时，避免首句少显示一个固定步。
    if (dialogueWasActive && dialogueActive_) {
        dialogueTimer_ += deltaTime;
        float lineStart=0;
        const auto lines=CurrentDialogue();
        const auto cues=stageContent_->SceneDialogueCues(dialogueScene_,dialogueAfterBattle_,player_.Shot());
        // 无台词的 MSG 收尾只推进自身时长，不构造占位台词，也不允许确认键越过锁定的演出。
        if(!lines.empty()) {
            const auto& line=lines[DialogueIndex(&lineStart)];
            const bool inputAllowed=DialogueFlagAt(cues,DialogueCueKind::InputEnabled,dialogueTimer_,true) &&
                (fastForward || dialogueTimer_-lineStart+0.00001F>=line.confirmDelay);
            if (inputAllowed && (dialogueConfirm || fastForward))
                dialogueTimer_=lineStart+(line.advanceAfter>=0 ? line.advanceAfter :
                    line.duration>0 ? line.duration : stageContent_->DialogueLineDuration());
        }
        if (!dialogueAfterBattle_ && DialogueFlagAt(cues,DialogueCueKind::BossMusic,dialogueTimer_) && !dialogueBossMusic_) {
            assets_.PlayBossMusic(); dialogueBossMusic_=true;
        }
        if (dialogueTimer_+0.00001F>=DialogueDuration()) {
            dialogueActive_=false;
            if (dialogueEventId_) { stage_.EndEvent(*dialogueEventId_); dialogueEventId_.reset(); }
        }
        for(const auto& cue:cues)if(cue.kind==DialogueCueKind::StageSignal&&cue.frame<=dialogueTimer_*60+0.00001F)
            dialogueSignalsSeen_=std::max(dialogueSignalsSeen_,cue.value);
        if(dialogueSignalWait_&&(dialogueSignalWait_->second==0 ? !dialogueActive_ : dialogueSignalsSeen_>=dialogueSignalWait_->second)) {
            stage_.EndEvent(dialogueSignalWait_->first);dialogueSignalWait_.reset();
        }
        if(!dialogueActive_&&dialogueAfterBattle_&&dialogueScene_==DialogueScene::FinalBoss) { BeginStageClear(); return; }
        // 对话计时或跳句后立即重采样；绘制只读取完成后的状态，不推进逻辑时钟。
        if(dialogueActive_ && !cues.empty())dialogueMotion_.Seek(cues,dialogueTimer_);
    }
    if(!dialogueActive_&&!WorldTimeStopped()) {
        rankTime_+=deltaTime;
        const float period=std::max(4.0F,40.0F-std::max(0,player_.RemainingLives()-1)*4.0F);
        while(rankTime_>=period) { rankTime_-=period; AdjustReferenceRank(100); }
    }
    const bool latePlayer=boss_&&boss_->active&&boss_->ControlsWorldTime();
    if(!latePlayer)UpdatePlayer(deltaTime,input);
    UpdateActors(deltaTime);
    UpdateBoss(deltaTime);
    if (state_ != RunState::Playing) return;
    if(latePlayer&&!WorldTimeStopped())UpdatePlayer(deltaTime,input);
    if(!WorldTimeStopped())scenerySeconds_+=deltaTime;
    std::array<Vector3, kActorCapacity + 1> targets{};
    std::size_t targetCount = 0;
    actors_.ForEachActive([&](const behavior::Actor& enemy) {
        if(enemy.CanBeTargeted())targets[targetCount++] = enemy.position;
    });
    if (boss_ && boss_->CanBeTargeted()) targets[targetCount++] = boss_->position;
    bullets_.Update(deltaTime, {targets.data(), targetCount}, player_.Position(),WorldTimeStopped());
    UpdateBattleAnimations(); // 本帧击破快照读取已推进的动作，而不是上一帧的动画。
    UpdatePlayerBomb(deltaTime); // 先让 Bomb 清除接触实体的敌弹，再检查玩家命中和普通弹伤害。
    ResolveCollisions();
    if (state_ == RunState::Playing&&!WorldTimeStopped()) UpdatePickups(deltaTime);
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

void Game::SpawnStageActor(const behavior::Spawn& spec) {
    // 新典道中 Boss 存在时，出生请求被跳过，但时间轴仍继续计时。
    if (boss_ && boss_->active) return;
    actors_.Spawn(spec);
}

void Game::BeginStageBoss(const bosses::Battle& battle, StageEventId id) {
    // 只停止旧批次的后续出怪；现存小怪和弹幕继续更新，不能在 Boss 入场时清场。
    stage_.CancelEnemySpawns();
    bossEventId_ = id;
    boss_ = stageContent_->CreateBoss(battle,player_.Shot());
    if (boss_->MusicOnEntrance() && !dialogueBossMusic_) assets_.PlayBossMusic();
    messageTimer_=0;
}

void Game::BeginStageDialogue(StageEventId id, bool afterBattle,DialogueScene scene,DialogueClearPolicy clearPolicy) {
    if(afterBattle && scene==DialogueScene::FinalBoss && stageClearShown_)return;
    dialogueAfterBattle_=afterBattle;
    dialogueSignalsSeen_=0;dialogueSignalWait_.reset();
    dialogueScene_=scene;
    dialogueBossMusic_=false;
    stage_.CancelEnemySpawns();
    if(clearPolicy==DialogueClearPolicy::ClearEnemiesAndBullets) {
        ClearActorsWithEffects();bullets_.ClearEnemyBullets();
    }
    dialogueActive_ = true;
    dialogueTimer_ = 0.0F;
    dialogueEventId_ = id;
    const auto lines = CurrentDialogue();
    if (!lines.empty()) dialogueMotion_.Reset(lines.front().bossSpeaking, lines.front().showBoss);
    const auto cues=stageContent_->SceneDialogueCues(scene,afterBattle,player_.Shot());
    if(!cues.empty())dialogueMotion_.Seek(cues,0);
    messageTimer_ = 0.0F;
}

void Game::ResumeStageBoss() {
    if(!boss_||!boss_->active)throw std::logic_error("No active boss to resume");
    boss_->ResumeCombat();
}
bool Game::AwaitStageDialogueSignal(StageEventId id,int signal) {
    if(signal<0)throw std::invalid_argument("Negative dialogue signal");
    if(signal==0 ? !dialogueActive_ : dialogueSignalsSeen_>=signal)return true;
    if(dialogueSignalWait_)throw std::logic_error("Another dialogue signal wait is pending");
    dialogueSignalWait_=std::pair{id,signal};return false;
}

void Game::AdjustReferenceRank(int subrank) {
    referenceSubrank_+=subrank;
    while(referenceSubrank_>=100) { ++referenceRank_; referenceSubrank_-=100; }
    while(referenceSubrank_<0) { --referenceRank_; referenceSubrank_+=100; }
    referenceRank_=std::clamp(referenceRank_,10,32);
}
std::span<const DialogueLine> Game::CurrentDialogue() const { return stageContent_->SceneDialogue(dialogueScene_,dialogueAfterBattle_,player_.Shot()); }
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
    return std::max(duration,stageContent_->DialogueMinimumDuration(dialogueScene_,dialogueAfterBattle_,player_.Shot()));
}
void Game::StageMessage(std::string_view message, float duration) {
    SetMessage(std::string(message), duration);
}

void Game::SetMessage(std::string message, float duration) {
    message_ = std::move(message);
    messageTimer_ = duration;
}

}  // namespace koumakyou
