#include "game/game.hpp"
#include "game/settings.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <numbers>
#include <utility>

namespace redmist {
namespace {

constexpr Color kPlayerBulletColor{90, 225, 255, 255};
constexpr Color kEnemyRed{255, 70, 105, 255};
constexpr Color kEnemyBlue{75, 145, 255, 255};
constexpr Color kEnemyPurple{205, 95, 255, 255};

constexpr std::array<const char*, 10> kBindingNames{
    "FORWARD", "BACKWARD", "MOVE LEFT", "MOVE RIGHT", "ASCEND",
    "DESCEND", "FOCUS / SLOW", "FIRE", "BOMB", "PAUSE",
};

Vector3 Add(Vector3 left, Vector3 right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vector3 Subtract(Vector3 left, Vector3 right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vector3 Scale(Vector3 vector, float scale) {
    return {vector.x * scale, vector.y * scale, vector.z * scale};
}

float DistanceSquared(Vector3 left, Vector3 right) {
    const Vector3 difference = Subtract(left, right);
    return difference.x * difference.x + difference.y * difference.y +
           difference.z * difference.z;
}

Vector3 Normalize(Vector3 vector) {
    const float length = BulletSystem::Length(vector);
    if (length < 0.00001F) {
        return {};
    }
    return Scale(vector, 1.0F / length);
}

}  // namespace

Game::Game() {
    camera_.up = {0.0F, 1.0F, 0.0F};
    camera_.fovy = 72.0F;
    camera_.projection = CAMERA_PERSPECTIVE;
    LoadSettings();
    Reset();
}

void Game::Reset() {
    StartRun();
    EnterMainMenu();
}

void Game::Start() {
    StartRun();
}

void Game::OpenOptions() {
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
    const GameSettings settings{bindings_, kSupportedFpsLimits[fpsLimitIndex_]};
    if (!SaveSettingsFile(kSettingsFilename, settings)) {
        TraceLog(LOG_WARNING, "SETTINGS: Failed to save %s", kSettingsFilename);
    }
}

void Game::StartRun() {
    state_ = RunState::Playing;
    player_ = Player{};
    boss_ = Boss{};
    bullets_.Reset();
    enemies_.Clear();
    stage_.Reset();
    score_ = 0;
    graze_ = 0;
    screenFlashTimer_ = 0.0F;
    shouldQuit_ = false;
    UpdateCamera();
    DisableCursor();
    SetMessage("Stage 1  Cubic Mist  Normal", 3.0F);
}

void Game::EnterMainMenu() {
    state_ = RunState::MainMenu;
    selectedMenuItem_ = 0;
    messageTimer_ = 0.0F;
    bullets_.Reset();
    enemies_.Clear();
    boss_ = Boss{};
    player_.position = {0.0F, 0.0F, -6.5F};
    player_.yaw = 0.0F;
    player_.pitch = 0.0F;
    UpdateCamera();
    EnableCursor();
}

void Game::Update(float deltaTime) {
    if (state_ == RunState::MainMenu) {
        UpdateMainMenu();
        return;
    }
    if (state_ == RunState::Options) {
        UpdateOptions();
        return;
    }

    if (state_ == RunState::Won || state_ == RunState::GameOver) {
        if (IsKeyPressed(KEY_R)) {
            StartRun();
        } else if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_M)) {
            EnterMainMenu();
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(bindings_.pause)) {
        if (state_ == RunState::Playing) {
            state_ = RunState::Paused;
            EnableCursor();
        } else if (state_ == RunState::Paused) {
            state_ = RunState::Playing;
            DisableCursor();
        }
    }
    if (state_ != RunState::Playing) {
        if (state_ == RunState::Paused && IsKeyPressed(KEY_M)) {
            EnterMainMenu();
        }
        return;
    }

    messageTimer_ = std::max(0.0F, messageTimer_ - deltaTime);
    screenFlashTimer_ = std::max(0.0F, screenFlashTimer_ - deltaTime);
    player_.invulnerableTimer =
        std::max(0.0F, player_.invulnerableTimer - deltaTime);
    player_.bombCooldown = std::max(0.0F, player_.bombCooldown - deltaTime);

    stage_.Update(deltaTime,
                  [this](const StageEvent& event) { HandleStageEvent(event); });
    UpdatePlayer(deltaTime);
    UpdateEnemies(deltaTime);
    UpdateBoss(deltaTime);
    bullets_.Update(deltaTime);
    ResolveCollisions();
}

void Game::UpdateMainMenu() {
    constexpr int itemCount = 3;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        selectedMenuItem_ = (selectedMenuItem_ + itemCount - 1) % itemCount;
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        selectedMenuItem_ = (selectedMenuItem_ + 1) % itemCount;
    }

    const float centerX = static_cast<float>(GetScreenWidth()) * 0.5F;
    const float startY = static_cast<float>(GetScreenHeight()) * 0.5F + 50.0F;
    const Vector2 mouse = GetMousePosition();
    bool hoveringItem = false;
    for (int index = 0; index < itemCount; ++index) {
        const Rectangle button{centerX - 170.0F,
                               startY + static_cast<float>(index) * 70.0F,
                               340.0F, 52.0F};
        if (CheckCollisionPointRec(mouse, button)) {
            selectedMenuItem_ = index;
            hoveringItem = true;
        }
    }

    const bool mouseConfirmed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                                hoveringItem;
    const bool confirmed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                           mouseConfirmed;
    if (confirmed && selectedMenuItem_ == 0) {
        StartRun();
    } else if (confirmed && selectedMenuItem_ == 1) {
        OpenOptions();
    } else if (confirmed && selectedMenuItem_ == 2) {
        shouldQuit_ = true;
    }
}

void Game::UpdateOptions() {
    if (optionsPage_ == OptionsPage::KeyBindings) {
        UpdateKeyBindings();
        return;
    }

    constexpr int itemCount = 3;
    constexpr int fpsIndex = 0;
    constexpr int bindingsIndex = 1;
    constexpr int backIndex = 2;

    if (IsKeyPressed(KEY_ESCAPE)) {
        state_ = RunState::MainMenu;
        selectedMenuItem_ = 1;
        return;
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        selectedOption_ = (selectedOption_ + itemCount - 1) % itemCount;
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        selectedOption_ = (selectedOption_ + 1) % itemCount;
    }

    const float centerX = static_cast<float>(GetScreenWidth()) * 0.5F;
    constexpr float startY = 238.0F;
    constexpr float rowSpacing = 74.0F;
    const Vector2 mouse = GetMousePosition();
    bool hoveringItem = false;
    for (int index = 0; index < itemCount; ++index) {
        const Rectangle row{centerX - 270.0F,
                            startY + static_cast<float>(index) * rowSpacing,
                            540.0F, 54.0F};
        if (CheckCollisionPointRec(mouse, row)) {
            selectedOption_ = index;
            hoveringItem = true;
        }
    }

    if (selectedOption_ == fpsIndex) {
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
            AdjustFpsLimit(-1);
        } else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
            AdjustFpsLimit(1);
        }
    }

    const bool mouseConfirmed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                                hoveringItem;
    const bool confirmed = IsKeyPressed(KEY_ENTER) || mouseConfirmed;
    if (!confirmed) return;

    if (selectedOption_ == fpsIndex) {
        AdjustFpsLimit(1);
    } else if (selectedOption_ == bindingsIndex) {
        optionsPage_ = OptionsPage::KeyBindings;
        selectedBinding_ = 0;
        waitingForBinding_ = false;
    } else if (selectedOption_ == backIndex) {
        state_ = RunState::MainMenu;
        selectedMenuItem_ = 1;
    }
}

void Game::UpdateKeyBindings() {
    constexpr int bindingCount = static_cast<int>(kBindingNames.size());
    constexpr int resetIndex = bindingCount;
    constexpr int backIndex = bindingCount + 1;
    constexpr int itemCount = bindingCount + 2;

    if (waitingForBinding_) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            waitingForBinding_ = false;
            return;
        }
        const int key = GetKeyPressed();
        if (key > 0 && key != KEY_ESCAPE) {
            SetBinding(selectedBinding_, key);
            waitingForBinding_ = false;
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        optionsPage_ = OptionsPage::General;
        selectedOption_ = 1;
        return;
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        selectedBinding_ = (selectedBinding_ + itemCount - 1) % itemCount;
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        selectedBinding_ = (selectedBinding_ + 1) % itemCount;
    }

    const float centerX = static_cast<float>(GetScreenWidth()) * 0.5F;
    const Vector2 mouse = GetMousePosition();
    bool hoveringItem = false;
    for (int index = 0; index < bindingCount; ++index) {
        const Rectangle row{centerX - 270.0F,
                            142.0F + static_cast<float>(index) * 34.0F,
                            540.0F, 29.0F};
        if (CheckCollisionPointRec(mouse, row)) {
            selectedBinding_ = index;
            hoveringItem = true;
        }
    }
    const Rectangle resetButton{centerX - 270.0F, 500.0F, 260.0F, 42.0F};
    const Rectangle backButton{centerX + 10.0F, 500.0F, 260.0F, 42.0F};
    if (CheckCollisionPointRec(mouse, resetButton)) {
        selectedBinding_ = resetIndex;
        hoveringItem = true;
    } else if (CheckCollisionPointRec(mouse, backButton)) {
        selectedBinding_ = backIndex;
        hoveringItem = true;
    }

    const bool mouseConfirmed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                                hoveringItem;
    const bool confirmed = IsKeyPressed(KEY_ENTER) || mouseConfirmed;
    if (!confirmed) return;

    if (selectedBinding_ < bindingCount) {
        waitingForBinding_ = true;
    } else if (selectedBinding_ == resetIndex) {
        bindings_ = KeyBindings{};
        SaveSettings();
    } else {
        optionsPage_ = OptionsPage::General;
        selectedOption_ = 1;
    }
}

void Game::AdjustFpsLimit(int offset) {
    const int count = static_cast<int>(kSupportedFpsLimits.size());
    fpsLimitIndex_ = (fpsLimitIndex_ + offset + count) % count;
    SetTargetFPS(kSupportedFpsLimits[fpsLimitIndex_]);
    SaveSettings();
}

void Game::UpdatePlayer(float deltaTime) {
    const Vector2 mouseDelta = GetMouseDelta();
    player_.yaw -= mouseDelta.x * player_.mouseSensitivity;
    player_.pitch -= mouseDelta.y * player_.mouseSensitivity;
    player_.pitch = std::clamp(player_.pitch, -1.45F, 1.45F);

    const Vector3 forward = Forward();
    const Vector3 right = Right();
    Vector3 movement{};
    if (IsKeyDown(bindings_.forward)) movement = Add(movement, forward);
    if (IsKeyDown(bindings_.backward)) movement = Subtract(movement, forward);
    if (IsKeyDown(bindings_.moveRight)) movement = Add(movement, right);
    if (IsKeyDown(bindings_.moveLeft)) movement = Subtract(movement, right);
    if (IsKeyDown(bindings_.ascend)) movement.y += 1.0F;
    if (IsKeyDown(bindings_.descend)) movement.y -= 1.0F;

    movement = Normalize(movement);
    const bool focused = IsKeyDown(bindings_.focus);
    const float speed = focused ? player_.focusSpeed : player_.speed;
    player_.position = Add(player_.position, Scale(movement, speed * deltaTime));

    const float limit = kArenaHalfExtent - kArenaPadding;
    player_.position.x = std::clamp(player_.position.x, -limit, limit);
    player_.position.y = std::clamp(player_.position.y, -limit, limit);
    player_.position.z = std::clamp(player_.position.z, -limit, limit);
    UpdateCamera();

    player_.fireTimer -= deltaTime;
    if ((IsKeyDown(bindings_.fire) || IsMouseButtonDown(MOUSE_BUTTON_LEFT)) &&
        player_.fireTimer <= 0.0F) {
        FirePlayerShot();
        player_.fireTimer = focused ? 0.075F : 0.09F;
    }
    if (IsKeyPressed(bindings_.bomb) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        UseBomb();
    }
}

void Game::UpdateCamera() {
    camera_.position = player_.position;
    camera_.target = Add(player_.position, Forward());
    camera_.up = {0.0F, 1.0F, 0.0F};
}

void Game::UpdateEnemies(float deltaTime) {
    enemies_.ForEachActive([this, deltaTime](Enemy& enemy) {
        enemy.age += deltaTime;
        if (enemy.age < 0.0F) return;

        switch (enemy.path) {
            case PathKind::Straight:
                enemy.position = Add(enemy.origin,
                                     {0.0F, 0.0F, -enemy.speed * enemy.age});
                break;
            case PathKind::Helix: {
                const float angle = enemy.age * 2.1F + enemy.origin.x * 0.3F;
                enemy.position = {
                    enemy.origin.x + std::cos(angle) * 1.25F,
                    enemy.origin.y + std::sin(angle) * 1.25F,
                    enemy.origin.z - enemy.speed * enemy.age,
                };
                break;
            }
            case PathKind::SweepLeft:
                enemy.position = {
                    enemy.origin.x + enemy.speed * 0.9F * enemy.age,
                    enemy.origin.y + std::sin(enemy.age * 1.7F) * 0.7F,
                    enemy.origin.z - enemy.speed * 0.5F * enemy.age,
                };
                break;
            case PathKind::SweepRight:
                enemy.position = {
                    enemy.origin.x - enemy.speed * 0.9F * enemy.age,
                    enemy.origin.y + std::cos(enemy.age * 1.7F) * 0.7F,
                    enemy.origin.z - enemy.speed * 0.5F * enemy.age,
                };
                break;
        }

        enemy.fireTimer -= deltaTime;
        if (enemy.age > 0.55F && enemy.fireTimer <= 0.0F) {
            if (enemy.kind == EnemyKind::Fairy) {
                const Vector3 direction =
                    BulletSystem::DirectionTo(enemy.position, player_.position);
                bullets_.Spawn(BulletOwner::Enemy, enemy.position,
                               Scale(direction, 3.3F), 0.16F, kEnemyRed);
            } else if (enemy.kind == EnemyKind::Orb) {
                bullets_.SpawnAimedCone(enemy.position, player_.position, 1, 5,
                                        0.24F, 3.0F, enemy.age * 0.2F,
                                        kEnemyBlue);
            } else {
                bullets_.SpawnSphereStar(BulletOwner::Enemy, enemy.position, 34,
                                         2.45F, enemy.age * 0.35F,
                                         kEnemyPurple);
                bullets_.SpawnAimedCone(enemy.position, player_.position, 1, 7,
                                        0.32F, 3.5F, enemy.age, kEnemyRed);
            }
            enemy.fireTimer = enemy.fireInterval;
        }

        constexpr float outside = kArenaHalfExtent + 2.0F;
        if (std::abs(enemy.position.x) > outside ||
            std::abs(enemy.position.y) > outside ||
            std::abs(enemy.position.z) > outside) {
            enemy.active = false;
        }
    });
}

void Game::UpdateBoss(float deltaTime) {
    if (!boss_.active) return;

    boss_.phaseTimer += deltaTime;
    boss_.moveClock += deltaTime;
    boss_.fireTimer -= deltaTime;
    boss_.position = {
        std::sin(boss_.moveClock * 0.63F) * 3.2F,
        2.0F + std::cos(boss_.moveClock * 0.81F) * 2.0F,
        6.8F + std::sin(boss_.moveClock * 0.37F) * 0.7F,
    };

    if (boss_.phase == 1 && boss_.fireTimer <= 0.0F) {
        bullets_.SpawnAimedCone(boss_.position, player_.position, 2, 8,
                                0.48F, 3.3F, boss_.starRotation, kEnemyRed);
        bullets_.SpawnSphereStar(BulletOwner::Enemy, boss_.position, 38, 1.8F,
                                 boss_.starRotation, kEnemyBlue, 0.14F);
        boss_.starRotation += 0.33F;
        boss_.fireTimer = 0.95F;
    } else if (boss_.phase == 2 && boss_.fireTimer <= 0.0F) {
        bullets_.SpawnSphereStar(BulletOwner::Enemy, boss_.position, 64, 2.65F,
                                 boss_.starRotation, kEnemyPurple, 0.15F);
        bullets_.SpawnAimedCone(boss_.position, player_.position, 1, 10,
                                0.28F, 4.0F, -boss_.starRotation, kEnemyRed);
        boss_.starRotation += 0.27F;
        boss_.fireTimer = 0.58F;
    }

    if ((boss_.phase == 1 && boss_.phaseTimer > 30.0F) ||
        (boss_.phase == 2 && boss_.phaseTimer > 38.0F)) {
        AdvanceBossPhase();
    }
}

void Game::ResolveCollisions() {
    bool playerWasHit = false;
    bullets_.Pool().ForEachActive([this, &playerWasHit](Bullet& bullet) {
        if (bullet.owner != BulletOwner::Enemy) return;
        const float distanceSquared = DistanceSquared(bullet.position, player_.position);
        const float hitDistance = bullet.radius + player_.hitRadius;
        if (distanceSquared <= hitDistance * hitDistance) {
            bullet.active = false;
            if (player_.invulnerableTimer <= 0.0F) playerWasHit = true;
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
        bullets_.Pool().ForEachActive([&enemy](Bullet& bullet) {
            if (bullet.owner != BulletOwner::Player) return;
            constexpr float enemyRadius = 0.55F;
            const float collisionDistance = bullet.radius + enemyRadius;
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

    if (boss_.active) {
        bullets_.Pool().ForEachActive([this](Bullet& bullet) {
            if (bullet.owner != BulletOwner::Player) return;
            constexpr float bossRadius = 0.95F;
            const float collisionDistance = bullet.radius + bossRadius;
            if (DistanceSquared(bullet.position, boss_.position) <=
                collisionDistance * collisionDistance) {
                bullet.active = false;
                boss_.health -= 1.6F;
                score_ += 10;
            }
        });
        if (boss_.health <= 0.0F) AdvanceBossPhase();
    }
}

void Game::HandleStageEvent(const StageEvent& event) {
    switch (event.kind) {
        case StageEventKind::SpawnLine: {
            const float middle = static_cast<float>(event.count - 1) * 0.5F;
            for (int index = 0; index < event.count; ++index) {
                const float offset = static_cast<float>(index) - middle;
                SpawnEnemy(event.enemy,
                           index % 2 == 0 ? PathKind::Straight : PathKind::Helix,
                           {event.center.x + offset * event.spacing,
                            event.center.y + std::sin(offset) * 0.8F,
                            event.center.z},
                           static_cast<float>(index) * 0.1F);
            }
            break;
        }
        case StageEventKind::SpawnStarFormation: {
            constexpr float goldenAngle = 2.39996322973F;
            for (int index = 0; index < event.count; ++index) {
                const float t = (static_cast<float>(index) + 0.5F) /
                                static_cast<float>(event.count);
                const float y = 1.0F - 2.0F * t;
                const float radial = std::sqrt(std::max(0.0F, 1.0F - y * y));
                const float azimuth = goldenAngle * static_cast<float>(index);
                const Vector3 offset{
                    radial * std::cos(azimuth) * event.spacing,
                    y * event.spacing,
                    radial * std::sin(azimuth) * event.spacing * 0.35F,
                };
                SpawnEnemy(event.enemy, PathKind::Helix,
                           Add(event.center, offset), index * 0.08F);
            }
            break;
        }
        case StageEventKind::SpawnCrossingPair:
            for (int index = 0; index < event.count / 2; ++index) {
                const float y = -4.5F + static_cast<float>(index) * 2.6F;
                const float z = 7.8F - static_cast<float>(index) * 0.45F;
                SpawnEnemy(event.enemy, PathKind::SweepLeft,
                           {-9.0F, y, z}, index * 0.18F);
                SpawnEnemy(event.enemy, PathKind::SweepRight,
                           {9.0F, -y * 0.7F, z}, index * 0.18F);
            }
            break;
        case StageEventKind::SpawnElite:
            SpawnEnemy(EnemyKind::Elite, PathKind::Helix, event.center);
            break;
        case StageEventKind::StartBoss:
            StartBoss();
            break;
        case StageEventKind::ShowMessage:
            if (stage_.Elapsed() < 5.0F) {
                SetMessage("Stage 1  Cubic Mist  Normal", 2.4F);
            } else if (stage_.Elapsed() < 30.0F) {
                SetMessage("Spatial star burst detected", 2.0F);
            } else if (stage_.Elapsed() < 43.0F) {
                SetMessage("WARNING  3D BULLET FIELD", 1.8F);
            } else {
                SetMessage("Rumia  Spatial Phase", 2.0F);
            }
            break;
    }
}

void Game::SpawnEnemy(EnemyKind kind, PathKind path, Vector3 position,
                      float delay) {
    Enemy* enemy = enemies_.Acquire();
    if (enemy == nullptr) return;
    enemy->kind = kind;
    enemy->path = path;
    enemy->origin = position;
    enemy->position = position;
    enemy->age = -delay;
    if (kind == EnemyKind::Fairy) {
        enemy->health = enemy->maxHealth = 10.0F;
        enemy->speed = 1.5F;
        enemy->fireInterval = 1.8F;
        enemy->scoreValue = 500;
    } else if (kind == EnemyKind::Orb) {
        enemy->health = enemy->maxHealth = 18.0F;
        enemy->speed = 1.15F;
        enemy->fireInterval = 1.5F;
        enemy->scoreValue = 900;
    } else {
        enemy->health = enemy->maxHealth = 135.0F;
        enemy->speed = 0.28F;
        enemy->fireInterval = 1.15F;
        enemy->scoreValue = 5000;
    }
    enemy->fireTimer = 0.8F + delay;
}

void Game::FirePlayerShot() {
    const Vector3 forward = Forward();
    const Vector3 right = Right();
    const Vector3 muzzle = Add(player_.position, Scale(forward, 0.55F));
    const Vector3 velocity = Scale(forward, 20.0F);
    bullets_.Spawn(BulletOwner::Player, Add(muzzle, Scale(right, -0.11F)),
                   velocity, 0.09F, kPlayerBulletColor);
    bullets_.Spawn(BulletOwner::Player, Add(muzzle, Scale(right, 0.11F)),
                   velocity, 0.09F, kPlayerBulletColor);
}

void Game::UseBomb() {
    if (player_.bombs <= 0 || player_.bombCooldown > 0.0F) return;
    --player_.bombs;
    player_.bombCooldown = 1.0F;
    player_.invulnerableTimer = 2.6F;
    screenFlashTimer_ = 0.35F;
    bullets_.ClearEnemyBullets();
    enemies_.ForEachActive([](Enemy& enemy) { enemy.health -= 22.0F; });
    if (boss_.active) boss_.health -= 35.0F;
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
        state_ = RunState::GameOver;
        EnableCursor();
    } else {
        SetMessage("Miss  Camera hitbox restored", 0.9F);
    }
}

void Game::StartBoss() {
    enemies_.Clear();
    bullets_.ClearEnemyBullets();
    boss_ = Boss{};
    boss_.active = true;
    boss_.phase = 1;
    boss_.health = boss_.maxHealth = 320.0F;
    boss_.fireTimer = 1.2F;
    SetMessage("Rumia  Spatial Non-spell", 2.0F);
}

void Game::AdvanceBossPhase() {
    if (!boss_.active) return;
    bullets_.ClearEnemyBullets();
    if (boss_.phase == 1) {
        score_ += 15000;
        boss_.phase = 2;
        boss_.health = boss_.maxHealth = 480.0F;
        boss_.phaseTimer = 0.0F;
        boss_.fireTimer = 1.0F;
        boss_.starRotation = 0.0F;
        SetMessage("Night Sign  Spherical Starfield", 2.4F);
    } else {
        score_ += 50000;
        boss_.active = false;
        state_ = RunState::Won;
        EnableCursor();
    }
}

void Game::SetMessage(std::string message, float duration) {
    message_ = std::move(message);
    messageTimer_ = duration;
}

Vector3 Game::Forward() const {
    return BulletSystem::DirectionFromYawPitch(player_.yaw, player_.pitch);
}

Vector3 Game::Right() const {
    return {-std::cos(player_.yaw), 0.0F, std::sin(player_.yaw)};
}

int Game::BindingAt(int index) const {
    switch (index) {
        case 0: return bindings_.forward;
        case 1: return bindings_.backward;
        case 2: return bindings_.moveLeft;
        case 3: return bindings_.moveRight;
        case 4: return bindings_.ascend;
        case 5: return bindings_.descend;
        case 6: return bindings_.focus;
        case 7: return bindings_.fire;
        case 8: return bindings_.bomb;
        case 9: return bindings_.pause;
        default: return KEY_NULL;
    }
}

void Game::SetBinding(int index, int key) {
    auto bindingPointer = [this](int bindingIndex) -> int* {
        switch (bindingIndex) {
            case 0: return &bindings_.forward;
            case 1: return &bindings_.backward;
            case 2: return &bindings_.moveLeft;
            case 3: return &bindings_.moveRight;
            case 4: return &bindings_.ascend;
            case 5: return &bindings_.descend;
            case 6: return &bindings_.focus;
            case 7: return &bindings_.fire;
            case 8: return &bindings_.bomb;
            case 9: return &bindings_.pause;
            default: return nullptr;
        }
    };

    int* target = bindingPointer(index);
    if (target == nullptr || key == KEY_NULL) return;
    const int previousKey = *target;
    for (int other = 0; other < static_cast<int>(kBindingNames.size()); ++other) {
        if (other == index) continue;
        int* otherBinding = bindingPointer(other);
        if (*otherBinding == key) {
            *otherBinding = previousKey;
            break;
        }
    }
    *target = key;
    SaveSettings();
}

std::string Game::KeyLabel(int key) {
    if (key >= KEY_A && key <= KEY_Z) {
        return std::string(1, static_cast<char>('A' + key - KEY_A));
    }
    if (key >= KEY_ZERO && key <= KEY_NINE) {
        return std::string(1, static_cast<char>('0' + key - KEY_ZERO));
    }
    switch (key) {
        case KEY_SPACE: return "SPACE";
        case KEY_ENTER: return "ENTER";
        case KEY_TAB: return "TAB";
        case KEY_BACKSPACE: return "BACKSPACE";
        case KEY_INSERT: return "INSERT";
        case KEY_DELETE: return "DELETE";
        case KEY_RIGHT: return "RIGHT";
        case KEY_LEFT: return "LEFT";
        case KEY_DOWN: return "DOWN";
        case KEY_UP: return "UP";
        case KEY_PAGE_UP: return "PAGE UP";
        case KEY_PAGE_DOWN: return "PAGE DOWN";
        case KEY_HOME: return "HOME";
        case KEY_END: return "END";
        case KEY_CAPS_LOCK: return "CAPS LOCK";
        case KEY_SCROLL_LOCK: return "SCROLL LOCK";
        case KEY_NUM_LOCK: return "NUM LOCK";
        case KEY_PRINT_SCREEN: return "PRINT SCREEN";
        case KEY_PAUSE: return "PAUSE";
        case KEY_LEFT_SHIFT: return "LEFT SHIFT";
        case KEY_LEFT_CONTROL: return "LEFT CTRL";
        case KEY_LEFT_ALT: return "LEFT ALT";
        case KEY_RIGHT_SHIFT: return "RIGHT SHIFT";
        case KEY_RIGHT_CONTROL: return "RIGHT CTRL";
        case KEY_RIGHT_ALT: return "RIGHT ALT";
        case KEY_F1: return "F1";
        case KEY_F2: return "F2";
        case KEY_F3: return "F3";
        case KEY_F4: return "F4";
        case KEY_F5: return "F5";
        case KEY_F6: return "F6";
        case KEY_F7: return "F7";
        case KEY_F8: return "F8";
        case KEY_F9: return "F9";
        case KEY_F10: return "F10";
        case KEY_F11: return "F11";
        case KEY_F12: return "F12";
        default: return TextFormat("KEY %d", key);
    }
}

void Game::Draw() const {
    DrawWorld();
    if (state_ == RunState::MainMenu) {
        DrawMainMenu();
    } else if (state_ == RunState::Options) {
        DrawOptions();
    } else {
        DrawHud();
        DrawOverlay();
    }
}

void Game::DrawWorld() const {
    BeginMode3D(camera_);
    DrawArena();

    bullets_.Pool().ForEachActive([](const Bullet& bullet) {
        DrawSphere(bullet.position, bullet.radius, bullet.color);
        DrawSphereWires(bullet.position, bullet.radius * 1.22F, 5, 5,
                        bullet.owner == BulletOwner::Enemy ? RAYWHITE
                                                          : Color{130, 240, 255, 255});
    });

    enemies_.ForEachActive([](const Enemy& enemy) {
        if (enemy.age < 0.0F) return;
        Color color = Color{100, 210, 255, 255};
        float radius = 0.5F;
        if (enemy.kind == EnemyKind::Orb) {
            color = Color{255, 125, 195, 255};
            radius = 0.62F;
        } else if (enemy.kind == EnemyKind::Elite) {
            color = Color{190, 95, 255, 255};
            radius = 0.85F;
        }
        DrawSphere(enemy.position, radius, color);
        DrawSphereWires(enemy.position, radius * 1.28F, 9, 9, WHITE);
        DrawCubeWires(enemy.position, radius * 2.4F, radius * 2.4F,
                      radius * 2.4F, Fade(color, 0.65F));
    });

    if (boss_.active) {
        DrawSphere(boss_.position, 0.95F, Color{255, 85, 120, 255});
        DrawSphereWires(boss_.position, 1.32F, 16, 12,
                        Color{255, 220, 120, 255});
        DrawCubeWires(boss_.position, 2.6F, 2.6F, 2.6F,
                      Color{255, 80, 170, 190});
    }

    EndMode3D();
}

void Game::DrawArena() const {
    constexpr float h = kArenaHalfExtent;
    const Color border{80, 185, 235, 230};
    const Color grid{45, 95, 130, 80};
    DrawCubeWires({0.0F, 0.0F, 0.0F}, h * 2.0F, h * 2.0F, h * 2.0F, border);

    constexpr float step = 2.5F;
    for (float value = -h + step; value < h; value += step) {
        // Floor and ceiling grids.
        DrawLine3D({-h, -h, value}, {h, -h, value}, grid);
        DrawLine3D({value, -h, -h}, {value, -h, h}, grid);
        DrawLine3D({-h, h, value}, {h, h, value}, grid);
        DrawLine3D({value, h, -h}, {value, h, h}, grid);

        // Left and right wall grids.
        DrawLine3D({-h, -h, value}, {-h, h, value}, grid);
        DrawLine3D({-h, value, -h}, {-h, value, h}, grid);
        DrawLine3D({h, -h, value}, {h, h, value}, grid);
        DrawLine3D({h, value, -h}, {h, value, h}, grid);

        // Front and back wall grids.
        DrawLine3D({-h, value, -h}, {h, value, -h}, grid);
        DrawLine3D({value, -h, -h}, {value, h, -h}, grid);
        DrawLine3D({-h, value, h}, {h, value, h}, grid);
        DrawLine3D({value, -h, h}, {value, h, h}, grid);
    }

    DrawLine3D({-h, 0.0F, 0.0F}, {h, 0.0F, 0.0F}, Color{255, 95, 120, 130});
    DrawLine3D({0.0F, -h, 0.0F}, {0.0F, h, 0.0F}, Color{95, 255, 150, 130});
    DrawLine3D({0.0F, 0.0F, -h}, {0.0F, 0.0F, h}, Color{90, 155, 255, 160});
}

void Game::DrawHud() const {
    DrawRectangleRounded({16.0F, 14.0F, 292.0F, 168.0F}, 0.08F, 8,
                         Color{4, 7, 18, 205});
    DrawRectangleLinesEx({16.0F, 14.0F, 292.0F, 168.0F}, 1.0F,
                         Color{95, 165, 210, 200});
    DrawText(TextFormat("SCORE  %09lld", score_), 30, 27, 21, RAYWHITE);
    DrawText(TextFormat("LIVES  %d", std::max(0, player_.lives + 1)),
             30, 57, 19, Color{255, 120, 140, 255});
    DrawText(TextFormat("BOMBS  %d", player_.bombs), 162, 57, 19,
             Color{110, 190, 255, 255});
    DrawText(TextFormat("GRAZE  %05d", graze_), 30, 86, 19,
             Color{230, 210, 120, 255});
    DrawText(TextFormat("XYZ  %+.1f  %+.1f  %+.1f", player_.position.x,
                        player_.position.y, player_.position.z),
             30, 116, 17, Color{160, 190, 210, 255});
    DrawText(TextFormat("BULLETS  %04d",
                        static_cast<int>(bullets_.Pool().ActiveCount())),
             30, 143, 17, Color{160, 190, 210, 255});

    float nearestEnemyBullet = 999.0F;
    bullets_.Pool().ForEachActive([this, &nearestEnemyBullet](const Bullet& bullet) {
        if (bullet.owner == BulletOwner::Enemy) {
            nearestEnemyBullet = std::min(
                nearestEnemyBullet,
                std::sqrt(DistanceSquared(bullet.position, player_.position)));
        }
    });
    const bool danger = nearestEnemyBullet < 1.15F;
    const int centerX = GetScreenWidth() / 2;
    const int centerY = GetScreenHeight() / 2;
    const Color reticle = danger ? Color{255, 70, 90, 255}
                                  : Color{215, 240, 255, 235};
    DrawCircleLines(centerX, centerY, 11.0F, reticle);
    DrawLine(centerX - 22, centerY, centerX - 7, centerY, reticle);
    DrawLine(centerX + 7, centerY, centerX + 22, centerY, reticle);
    DrawLine(centerX, centerY - 22, centerX, centerY - 7, reticle);
    DrawLine(centerX, centerY + 7, centerX, centerY + 22, reticle);
    DrawCircle(centerX, centerY, 2.0F, reticle);

    if (danger) {
        const char* warning = "HITBOX PROXIMITY";
        DrawText(warning, centerX - MeasureText(warning, 18) / 2,
                 centerY + 32, 18, reticle);
    }

    if (boss_.active) {
        const float width = std::min(540.0F, static_cast<float>(GetScreenWidth()) - 390.0F);
        const float ratio = std::clamp(boss_.health / boss_.maxHealth, 0.0F, 1.0F);
        const float left = (static_cast<float>(GetScreenWidth()) - width) * 0.5F;
        DrawRectangleRounded({left, 20.0F, width, 24.0F}, 0.5F, 12,
                             Color{20, 9, 26, 220});
        DrawRectangleRounded({left + 3.0F, 23.0F, (width - 6.0F) * ratio, 18.0F},
                             0.5F, 12,
                             boss_.phase == 1 ? Color{255, 80, 110, 255}
                                              : Color{190, 80, 255, 255});
        DrawText(TextFormat("RUMIA  SPATIAL PHASE %d", boss_.phase),
                 static_cast<int>(left), 48, 18, RAYWHITE);
    }

    const char* controls =
        "Mouse Look  WASD Move  Space Up  LShift Down  C Focus  LMB/Z Fire  RMB/X Bomb";
    DrawText(controls, 16, GetScreenHeight() - 29, 17,
             Color{185, 205, 220, 235});
    DrawText("CAMERA = HITBOX", GetScreenWidth() - 190, GetScreenHeight() - 56,
             17, Color{255, 125, 145, 240});
    DrawFPS(GetScreenWidth() - 90, 16);
}

void Game::DrawOverlay() const {
    if (messageTimer_ > 0.0F) {
        const int fontSize = 29;
        const int width = MeasureText(message_.c_str(), fontSize);
        const int x = (GetScreenWidth() - width) / 2;
        const int y = GetScreenHeight() / 2 - 105;
        DrawRectangle(x - 24, y - 12, width + 48, fontSize + 24,
                      Color{5, 7, 18, 185});
        DrawText(message_.c_str(), x + 2, y + 2, fontSize,
                 Color{80, 20, 40, 230});
        DrawText(message_.c_str(), x, y, fontSize, RAYWHITE);
    }

    if (state_ == RunState::Paused) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Color{0, 0, 0, 155});
        const char* text = "PAUSED";
        DrawText(text, (GetScreenWidth() - MeasureText(text, 52)) / 2,
                 GetScreenHeight() / 2 - 50, 52, RAYWHITE);
        const char* hint = "Esc / P Resume     M Main Menu";
        DrawText(hint, (GetScreenWidth() - MeasureText(hint, 20)) / 2,
                 GetScreenHeight() / 2 + 20, 20, Color{185, 205, 220, 255});
    } else if (state_ == RunState::Won || state_ == RunState::GameOver) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Color{0, 0, 0, 175});
        const char* title = state_ == RunState::Won ? "STAGE CLEAR" : "GAME OVER";
        DrawText(title, (GetScreenWidth() - MeasureText(title, 56)) / 2,
                 GetScreenHeight() / 2 - 75, 56,
                 state_ == RunState::Won ? GOLD : Color{255, 90, 110, 255});
        const char* retry = "R Restart     Esc / M Main Menu";
        DrawText(retry, (GetScreenWidth() - MeasureText(retry, 22)) / 2,
                 GetScreenHeight() / 2 + 18, 22, RAYWHITE);
    }

    if (screenFlashTimer_ > 0.0F) {
        const unsigned char alpha = static_cast<unsigned char>(
            std::clamp(screenFlashTimer_ / 0.35F, 0.0F, 1.0F) * 145.0F);
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Color{220, 240, 255, alpha});
    }
}

void Game::DrawMainMenu() const {
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    DrawRectangle(0, 0, screenWidth, screenHeight, Color{2, 4, 14, 118});

    const char* title = "RED MIST 3D";
    const int titleSize = 72;
    const int titleX = (screenWidth - MeasureText(title, titleSize)) / 2;
    DrawText(title, titleX + 4, 88, titleSize, Color{82, 8, 30, 230});
    DrawText(title, titleX, 84, titleSize, Color{255, 235, 240, 255});

    const char* subtitle = "FIRST-PERSON  6DoF  BULLET HELL";
    DrawText(subtitle, (screenWidth - MeasureText(subtitle, 25)) / 2,
             176, 25, Color{120, 195, 235, 255});
    const char* description = "Fight inside a full 3D cube  -  the camera itself is your hitbox";
    DrawText(description, (screenWidth - MeasureText(description, 18)) / 2,
             220, 18, Color{175, 195, 210, 255});

    const float centerX = static_cast<float>(screenWidth) * 0.5F;
    const float startY = static_cast<float>(screenHeight) * 0.5F + 50.0F;
    const char* labels[3] = {"START GAME", "OPTIONS", "QUIT"};
    for (int index = 0; index < 3; ++index) {
        const Rectangle button{centerX - 170.0F,
                               startY + static_cast<float>(index) * 70.0F,
                               340.0F, 52.0F};
        const bool selected = selectedMenuItem_ == index;
        DrawRectangleRounded(button, 0.18F, 10,
                             selected ? Color{106, 24, 52, 235}
                                      : Color{7, 13, 28, 220});
        DrawRectangleRoundedLinesEx(button, 0.18F, 10, selected ? 3.0F : 1.0F,
                                    selected ? Color{255, 120, 150, 255}
                                             : Color{80, 130, 165, 210});
        DrawText(labels[index],
                 static_cast<int>(centerX) - MeasureText(labels[index], 25) / 2,
                 static_cast<int>(button.y + 13.0F), 25,
                 selected ? RAYWHITE : Color{165, 180, 198, 255});
    }

    const char* hint = "W/S or Arrow Keys to select     Enter or Mouse to confirm";
    DrawText(hint, (screenWidth - MeasureText(hint, 18)) / 2,
             screenHeight - 68, 18, Color{160, 180, 200, 230});
    DrawText("No original game assets are included",
             (screenWidth - MeasureText("No original game assets are included", 16)) / 2,
             screenHeight - 38, 16, Color{105, 125, 145, 220});
}

void Game::DrawOptions() const {
    if (optionsPage_ == OptionsPage::KeyBindings) {
        DrawKeyBindings();
        return;
    }

    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const float centerX = static_cast<float>(screenWidth) * 0.5F;
    DrawRectangle(0, 0, screenWidth, screenHeight, Color{2, 4, 14, 205});

    const char* title = "OPTIONS";
    DrawText(title, (screenWidth - MeasureText(title, 48)) / 2,
             70, 48, RAYWHITE);
    const char* instruction = "Display and control settings";
    DrawText(instruction, (screenWidth - MeasureText(instruction, 19)) / 2,
             132, 19, Color{155, 190, 215, 255});

    constexpr int itemCount = 3;
    constexpr float startY = 238.0F;
    constexpr float rowSpacing = 74.0F;
    const char* labels[itemCount] = {"FPS LIMIT", "KEY BINDINGS", "BACK"};
    for (int index = 0; index < itemCount; ++index) {
        const Rectangle row{centerX - 270.0F,
                            startY + static_cast<float>(index) * rowSpacing,
                            540.0F, 54.0F};
        const bool selected = selectedOption_ == index;
        DrawRectangleRounded(row, 0.16F, 8,
                             selected ? Color{90, 24, 50, 235}
                                      : Color{8, 15, 30, 220});
        DrawRectangleRoundedLinesEx(row, 0.16F, 8,
                                    selected ? 2.0F : 1.0F,
                                    selected ? Color{255, 110, 145, 255}
                                             : Color{58, 95, 125, 210});
        DrawText(labels[index], static_cast<int>(row.x + 18.0F),
                 static_cast<int>(row.y + 16.0F), 22,
                 selected ? RAYWHITE : Color{175, 195, 210, 255});

        if (index == 0) {
            const int fpsLimit = kSupportedFpsLimits[fpsLimitIndex_];
            const char* fpsLabel = fpsLimit == 0 ? "<  UNLIMITED  >"
                                                 : TextFormat("<  %d  >", fpsLimit);
            DrawText(fpsLabel,
                     static_cast<int>(row.x + row.width - 18.0F) -
                         MeasureText(fpsLabel, 22),
                     static_cast<int>(row.y + 16.0F), 22,
                     selected ? Color{120, 215, 255, 255}
                              : Color{110, 160, 195, 255});
        } else if (index == 1) {
            const char* arrow = ">";
            DrawText(arrow,
                     static_cast<int>(row.x + row.width - 30.0F),
                     static_cast<int>(row.y + 16.0F), 22,
                     selected ? Color{120, 215, 255, 255}
                              : Color{110, 160, 195, 255});
        }
    }

    const char* currentFps = TextFormat("CURRENT FPS  %d", GetFPS());
    DrawText(currentFps, (screenWidth - MeasureText(currentFps, 18)) / 2,
             474, 18, Color{120, 175, 205, 235});
    const char* footer =
        "Up / Down: Select     Left / Right: Adjust     Enter: Confirm     Esc: Back";
    DrawText(footer, (screenWidth - MeasureText(footer, 17)) / 2,
             screenHeight - 42, 17, Color{145, 175, 198, 235});
}

void Game::DrawKeyBindings() const {
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const float centerX = static_cast<float>(screenWidth) * 0.5F;
    DrawRectangle(0, 0, screenWidth, screenHeight, Color{2, 4, 14, 205});

    const char* title = "KEY BINDINGS";
    DrawText(title, (screenWidth - MeasureText(title, 42)) / 2,
             45, 42, RAYWHITE);
    const char* instruction =
        "Select an action and press Enter, then press the new key";
    DrawText(instruction, (screenWidth - MeasureText(instruction, 18)) / 2,
             101, 18, Color{155, 190, 215, 255});

    for (int index = 0; index < static_cast<int>(kBindingNames.size()); ++index) {
        const Rectangle row{centerX - 270.0F,
                            142.0F + static_cast<float>(index) * 34.0F,
                            540.0F, 29.0F};
        const bool selected = selectedBinding_ == index;
        DrawRectangleRounded(row, 0.16F, 8,
                             selected ? Color{90, 24, 50, 235}
                                      : Color{8, 15, 30, 220});
        DrawRectangleRoundedLinesEx(row, 0.16F, 8, selected ? 2.0F : 1.0F,
                                    selected ? Color{255, 110, 145, 255}
                                             : Color{58, 95, 125, 210});
        DrawText(kBindingNames[index], static_cast<int>(row.x + 15.0F),
                 static_cast<int>(row.y + 6.0F), 17,
                 selected ? RAYWHITE : Color{175, 195, 210, 255});
        const std::string keyName = KeyLabel(BindingAt(index));
        DrawText(keyName.c_str(),
                 static_cast<int>(row.x + row.width - 16.0F) -
                     MeasureText(keyName.c_str(), 17),
                 static_cast<int>(row.y + 6.0F), 17,
                 selected ? Color{120, 215, 255, 255}
                          : Color{110, 160, 195, 255});
    }

    constexpr int resetIndex = static_cast<int>(kBindingNames.size());
    constexpr int backIndex = resetIndex + 1;
    const Rectangle resetButton{centerX - 270.0F, 500.0F, 260.0F, 42.0F};
    const Rectangle backButton{centerX + 10.0F, 500.0F, 260.0F, 42.0F};
    const Rectangle buttons[2] = {resetButton, backButton};
    const char* labels[2] = {"RESET DEFAULTS", "BACK"};
    for (int index = 0; index < 2; ++index) {
        const int selection = index == 0 ? resetIndex : backIndex;
        const bool selected = selectedBinding_ == selection;
        DrawRectangleRounded(buttons[index], 0.18F, 8,
                             selected ? Color{90, 24, 50, 235}
                                      : Color{8, 15, 30, 220});
        DrawRectangleRoundedLinesEx(buttons[index], 0.18F, 8,
                                    selected ? 2.0F : 1.0F,
                                    selected ? Color{255, 110, 145, 255}
                                             : Color{58, 95, 125, 210});
        DrawText(labels[index],
                 static_cast<int>(buttons[index].x + buttons[index].width * 0.5F) -
                     MeasureText(labels[index], 18) / 2,
                 static_cast<int>(buttons[index].y + 12.0F), 18, RAYWHITE);
    }

    const char* footer = "Arrow Keys / W / S: Select     Enter: Rebind     Esc: Options";
    DrawText(footer, (screenWidth - MeasureText(footer, 17)) / 2,
             screenHeight - 42, 17, Color{145, 175, 198, 235});

    if (waitingForBinding_) {
        DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 185});
        const Rectangle modal{centerX - 250.0F,
                              static_cast<float>(screenHeight) * 0.5F - 82.0F,
                              500.0F, 164.0F};
        DrawRectangleRounded(modal, 0.16F, 10, Color{12, 18, 36, 250});
        DrawRectangleRoundedLinesEx(modal, 0.16F, 10, 2.0F,
                                    Color{255, 110, 145, 255});
        const char* prompt = "PRESS A NEW KEY";
        DrawText(prompt, (screenWidth - MeasureText(prompt, 30)) / 2,
                 static_cast<int>(modal.y + 32.0F), 30, RAYWHITE);
        DrawText(kBindingNames[selectedBinding_],
                 (screenWidth - MeasureText(kBindingNames[selectedBinding_], 20)) / 2,
                 static_cast<int>(modal.y + 78.0F), 20,
                 Color{120, 215, 255, 255});
        const char* cancel = "Esc to cancel";
        DrawText(cancel, (screenWidth - MeasureText(cancel, 16)) / 2,
                 static_cast<int>(modal.y + 120.0F), 16,
                 Color{145, 165, 185, 255});
    }
}

}  // namespace redmist
