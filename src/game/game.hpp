#pragma once

#include "game/boss.hpp"
#include "game/bullet_system.hpp"
#include "game/enemy.hpp"
#include "game/game_types.hpp"
#include "game/stage_system.hpp"
#include "game/sprite_renderer.hpp"

#include "raylib.h"

#include <cstddef>
#include <string>

namespace redmist {

class Game {
public:
    Game();
    ~Game();

    void Reset();
    void Start();
    void StartPreview();
    void SetPreviewView(float yawDegrees, float pitchDegrees);
    void SetDebugDraw(bool enabled) { debugRender_ = enabled; }
    void OpenOptions();
    void Update(float deltaTime);
    void Draw() const;
    [[nodiscard]] bool ShouldQuit() const { return shouldQuit_; }

private:
    static constexpr std::size_t kEnemyCapacity = 128;

    enum class OptionsPage {
        General,
        KeyBindings,
    };

    void StartRun();
    void EnterMainMenu();
    void LoadSettings();
    void SaveSettings() const;
    void UpdateMainMenu();
    void UpdateOptions();
    void UpdateKeyBindings();
    void AdjustFpsLimit(int offset);
    void UpdatePlayer(float deltaTime);
    void UpdateCamera();
    void UpdateEnemies(float deltaTime);
    void UpdateBoss(float deltaTime);
    void ResolveCollisions();
    void HandleStageEvent(const StageEvent& event);

    void SpawnEnemy(EnemyKind kind, PathKind path, Vector3 position,
                    float delay = 0.0F);
    void FirePlayerShot();
    void UseBomb();
    void DamagePlayer();
    void StartMidboss();
    void StartFinalBoss();
    void AdvanceBossPhase();
    void SetMessage(std::string message, float duration);

    [[nodiscard]] Vector3 Forward() const;
    [[nodiscard]] Vector3 Right() const;
    [[nodiscard]] int BindingAt(int index) const;
    void SetBinding(int index, int key);
    [[nodiscard]] static std::string KeyLabel(int key);

    void DrawWorld() const;
    void DrawArena() const;
    void DrawHud() const;
    void DrawOverlay() const;
    void DrawSpellCutIn() const;
    void DrawDialogue() const;
    void DrawMainMenu() const;
    void DrawOptions() const;
    void DrawKeyBindings() const;

    Camera3D camera_{};
    RunState state_ = RunState::MainMenu;
    Player player_{};
    RumiaBoss boss_{};
    BulletSystem bullets_{};
    EnemyPool<kEnemyCapacity> enemies_{};
    StageDirector stage_{};
    KeyBindings bindings_{};

    long long score_ = 0;
    int graze_ = 0;
    float messageTimer_ = 0.0F;
    float screenFlashTimer_ = 0.0F;
    float spellCutInTimer_ = 0.0F;
    float dialogueTimer_ = 0.0F;
    bool dialogueActive_ = false;
    Texture2D spellPortrait_{};
    Texture2D reimuPortrait_{};
    Model skyboxModel_{};
    TextureCubemap skyboxTexture_{};
    Shader skyboxShader_{};
    Texture2D fairyTexture_{};
    Texture2D eliteTexture_{};
    Texture2D bulletTexture_{};
    Texture2D redBulletTexture_{};
    SpriteRenderer spriteRenderer_{};
    std::string message_;
    int selectedMenuItem_ = 0;
    OptionsPage optionsPage_ = OptionsPage::General;
    int selectedOption_ = 0;
    int selectedBinding_ = 0;
    int fpsLimitIndex_ = 1;
    bool waitingForBinding_ = false;
    bool shouldQuit_ = false;
    bool debugRender_ = false;
};

}  // namespace redmist
