// 本文件负责游戏生命周期、主循环调度和关卡事件转发。

#include "app/game.hpp"
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

namespace redmist {
#ifdef RED_MIST_DEBUG_FEATURES
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
#ifdef RED_MIST_DEBUG_FEATURES
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
    stageId_ = StageId::Stage1;
    StartRun(captureCursor);
}

void Game::StartStage(StageId id, bool captureCursor) {
    stageId_ = id;
    StartRun(captureCursor);
}

void Game::StartPreview() {
    StartRun(false);
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
    assets_.SetMusicVolumePercent(musicVolume_);

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
    const GameSettings settings{bindings_, kSupportedFpsLimits[fpsLimitIndex_], musicVolume_, mouseSensitivity_};
    if (!SaveSettingsFile(kSettingsFilename, settings)) {
        TraceLog(LOG_WARNING, "SETTINGS: Failed to save %s", kSettingsFilename);
    }
}

void Game::StartRun(bool captureCursor) {
    stage_ = StageDirector{};
    stageContent_ = CreateStageContent(stageId_);
    stage_ = StageDirector(stageContent_->Events());
    assets_.LoadStage(stageContent_->Assets());
    assets_.PlayStageMusic();
    state_ = RunState::Playing;
    player_.Reset();
    boss_.reset();
    bullets_.Reset();
    enemies_.Clear();
    drops_.Clear();
    stage_.Reset();
    score_ = 0;
    graze_ = 0;
    screenFlashTimer_ = 0.0F;
    spellCutInTimer_ = 0.0F;
    dialogueTimer_ = 0.0F;
    dialogueActive_ = false;
    bossEventId_.reset();
    dialogueEventId_.reset();
    shouldQuit_ = false;
#ifdef RED_MIST_DEBUG_FEATURES
    shotTestActive_ = false;
#endif
    player_.SyncCamera(camera_);
    if (captureCursor) DisableCursor();
    SetMessage(stageContent_->StartTitle(), 3.0F);
}

void Game::EnterMainMenu() {
    bullets_.Reset();
    enemies_.Clear();
    drops_.Clear();
    boss_.reset();
    assets_.UnloadStage();
    stage_ = StageDirector{};
    stageContent_.reset();
    assets_.PlayMenuMusic();
    state_ = RunState::MainMenu;
#ifdef RED_MIST_DEBUG_FEATURES
    shotTestActive_ = false;
#endif
    selectedMenuItem_ = 0;
    messageTimer_ = 0.0F;
    dialogueActive_ = false;
    bossEventId_.reset();
    dialogueEventId_.reset();
    player_.ResetPose();
    player_.SyncCamera(camera_);
    EnableCursor();
}

void Game::EndStage(RunState result) {
    bullets_.Reset();
    enemies_.Clear();
    drops_.Clear();
    boss_.reset();
    dialogueActive_ = false;
    assets_.UnloadStage();
    state_ = result;
    EnableCursor();
}

void Game::Update(float deltaTime) {
    assets_.UpdateAudio();
    if (state_ == RunState::MainMenu) {
        UpdateMainMenu();
        return;
    }
    if (state_ == RunState::Options) {
        UpdateOptions();
        return;
    }
#ifdef RED_MIST_DEBUG_FEATURES
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

    if (IsKeyPressed(KEY_ESCAPE) || BindingPressed(bindings_.pause)) {
        if (state_ == RunState::Playing) {
            state_ = RunState::Paused;
            assets_.PauseMusic();
            EnableCursor();
        } else if (state_ == RunState::Paused) {
            state_ = RunState::Playing;
            assets_.ResumeMusic();
            DisableCursor();
        }
    }
    if (state_ != RunState::Playing) {
        if (state_ == RunState::Paused && IsKeyPressed(KEY_M)) {
            EnterMainMenu();
        }
        return;
    }
#ifdef RED_MIST_DEBUG_FEATURES
    if (IsKeyPressed(KEY_F4)) debugInvincible_ = !debugInvincible_;
    if (IsKeyPressed(KEY_F5)) AdjustPlayerPower(-8);
    if (IsKeyPressed(KEY_F6)) AdjustPlayerPower(8);
#endif

    messageTimer_ = std::max(0.0F, messageTimer_ - deltaTime);
    screenFlashTimer_ = std::max(0.0F, screenFlashTimer_ - deltaTime);
    spellCutInTimer_ = std::max(0.0F, spellCutInTimer_ - deltaTime);

    const bool dialogueWasActive = dialogueActive_;
    stage_.Update(deltaTime,
                  [this](const StageEvent& event) {
                      return HandleStageEvent(event);
                  });
    // 本帧由时间轴刚启动的对话从下一帧计时，避免第一句少显示一个 deltaTime。
    if (dialogueWasActive && dialogueActive_) {
        dialogueTimer_ += deltaTime;
        if (dialogueTimer_ >=
            stageContent_->DialogueLineDuration() *
                static_cast<float>(stageContent_->Dialogue().size())) {
            dialogueActive_ = false;
            if (dialogueEventId_) {
                stage_.EndEvent(*dialogueEventId_);
                dialogueEventId_.reset();
            }
        }
    }
    UpdatePlayer(deltaTime);
    UpdateEnemies(deltaTime);
    UpdateBoss(deltaTime);
    std::array<Vector3, kEnemyCapacity + 1> targets{};
    std::size_t targetCount = 0;
    enemies_.ForEachActive([&](const Enemy& enemy) {
        targets[targetCount++] = enemy.position;
    });
    if (boss_ && boss_->active) targets[targetCount++] = boss_->position;
    bullets_.Update(deltaTime, {targets.data(), targetCount});
    ResolveCollisions();
    if (state_ == RunState::Playing) UpdatePickups(deltaTime);
}

#ifdef RED_MIST_DEBUG_FEATURES
void Game::StartShotTest() {
    stageId_ = StageId::Stage1;
    StartRun();
    // 保留 Stage 1 素材和战场，清空调度器以确保不会触发出怪或对话。
    stage_ = StageDirector{};
    enemies_.Clear();
    enemies_.Add(std::make_unique<ShotTestEnemy>());
    shotTestActive_ = true;
    SetMessage("SHOT TEST  F5: POWER -8  F6: POWER +8", 3.0F);
}
#endif

bool Game::HandleStageEvent(const StageEvent& event) {
    return stageContent_->HandleEvent(event, *this);
}

void Game::SpawnStageEnemy(const EnemySpec& spec) {
    if (auto enemy = stageContent_->CreateEnemy(spec))
        enemies_.Add(std::move(enemy));
}

void Game::BeginStageBoss(bool final, StageEventId id) {
    stage_.CancelEnemySpawns();
    enemies_.Clear();
    bullets_.ClearEnemyBullets();
    bossEventId_ = id;
    boss_ = stageContent_->CreateBoss(final);
    if (final) assets_.PlayBossMusic();
    SetMessage(stageContent_->BossStartMessage(final), 2.0F);
}

void Game::BeginStageDialogue(StageEventId id) {
    stage_.CancelEnemySpawns();
    enemies_.Clear();
    bullets_.ClearEnemyBullets();
    dialogueActive_ = true;
    dialogueTimer_ = 0.0F;
    dialogueEventId_ = id;
    messageTimer_ = 0.0F;
}

void Game::StageMessage(std::string_view message, float duration) {
    SetMessage(std::string(message), duration);
}

void Game::SetMessage(std::string message, float duration) {
    message_ = std::move(message);
    messageTimer_ = duration;
}

}  // namespace redmist
