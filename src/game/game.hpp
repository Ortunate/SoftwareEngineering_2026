#pragma once

#include "core/fixed_pool.hpp"
#include "game/bullet_system.hpp"
#include "game/game_types.hpp"
#include "game/stage_system.hpp"

#include "raylib.h"

#include <cstddef>
#include <string>

namespace redmist {

class Game {
public:
    Game();

    void Reset();
    void Start();
    void OpenOptions();
    void Update(float deltaTime);
    void Draw() const;
    [[nodiscard]] bool ShouldQuit() const { return shouldQuit_; }

private:
    static constexpr std::size_t kEnemyCapacity = 128;

    void StartRun();
    void EnterMainMenu();
    void UpdateMainMenu();
    void UpdateOptions();
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
    void StartBoss();
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
    void DrawMainMenu() const;
    void DrawOptions() const;

    Camera3D camera_{};
    RunState state_ = RunState::MainMenu;
    Player player_{};
    Boss boss_{};
    BulletSystem bullets_{};
    FixedPool<Enemy, kEnemyCapacity> enemies_{};
    StageDirector stage_{};
    KeyBindings bindings_{};

    long long score_ = 0;
    int graze_ = 0;
    float messageTimer_ = 0.0F;
    float screenFlashTimer_ = 0.0F;
    std::string message_;
    int selectedMenuItem_ = 0;
    int selectedBinding_ = 0;
    bool waitingForBinding_ = false;
    bool shouldQuit_ = false;
};

}  // namespace redmist
