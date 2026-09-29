// 本文件负责三维世界、主菜单、HUD、对话和符卡界面的绘制。

#include "app/game.hpp"
#include "app/game_constants.hpp"
#include "app/game_math.hpp"
#include "config/settings.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>

namespace redmist {
using namespace game_math;

#include "rlgl.h"

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
    if (assets_.Common().skyboxModel.meshCount != 0) {
        // 天空盒仅提供远景，不写入深度，确保战场和角色正常显示在前方。
        rlDisableBackfaceCulling();
        rlDisableDepthMask();
        DrawModel(assets_.Common().skyboxModel, {}, 1.0F, WHITE);
        rlDrawRenderBatchActive();
        rlEnableDepthMask();
        rlEnableBackfaceCulling();
    }
    DrawArena();

    std::array<const Enemy*, kEnemyCapacity> visibleEnemies{};
    std::size_t visibleCount = 0;
    enemies_.ForEachActive([&](const Enemy& enemy) {
        if (enemy.age >= 0.0F) visibleEnemies[visibleCount++] = &enemy;
    });
    const std::span<const Enemy*> visible(visibleEnemies.data(), visibleCount);
    spriteRenderer_.Draw(camera_, bullets_, visible, boss_.get(),
                         assets_.Stage().visuals, assets_.Stage().shaders);
#ifdef RED_MIST_DEBUG_FEATURES
    if (debugRender_) {
        spriteRenderer_.DrawDebugGeometry(bullets_, visible, boss_.get(),
                                          player_.position);
    }
#endif

    EndMode3D();
}

void Game::DrawArena() const {
    constexpr float h = kArenaHalfExtent;
    const Color border{170, 105, 150, 190};
    const Color grid{100, 65, 115, 70};
    DrawCubeWires({0.0F, 0.0F, 0.0F}, h * 2.0F, h * 2.0F, h * 2.0F, border);

    constexpr float step = 2.5F;
    for (float value = -h + step; value < h; value += step) {
        // 地面与天花板网格。
        DrawLine3D({-h, -h, value}, {h, -h, value}, grid);
        DrawLine3D({value, -h, -h}, {value, -h, h}, grid);
        DrawLine3D({-h, h, value}, {h, h, value}, grid);
        DrawLine3D({value, h, -h}, {value, h, h}, grid);

        // 左右墙面网格。
        DrawLine3D({-h, -h, value}, {-h, h, value}, grid);
        DrawLine3D({-h, value, -h}, {-h, value, h}, grid);
        DrawLine3D({h, -h, value}, {h, h, value}, grid);
        DrawLine3D({h, value, -h}, {h, value, h}, grid);

        // 前后墙面网格。
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

    if (boss_ && boss_->active) {
        const float width = std::min(540.0F, static_cast<float>(GetScreenWidth()) - 390.0F);
        const float ratio = std::clamp(boss_->health / boss_->maxHealth, 0.0F, 1.0F);
        const float left = (static_cast<float>(GetScreenWidth()) - width) * 0.5F;
        DrawRectangleRounded({left, 20.0F, width, 24.0F}, 0.5F, 12,
                             Color{20, 9, 26, 220});
        DrawRectangleRounded({left + 3.0F, 23.0F, (width - 6.0F) * ratio, 18.0F},
                             0.5F, 12,
                             boss_->phase == 1 ? Color{255, 80, 110, 255}
                                              : Color{190, 80, 255, 255});
        DrawText(boss_->DisplayName(),
                 static_cast<int>(left), 48, 18, RAYWHITE);
    }

    if (!dialogueActive_ && state_ != RunState::Won &&
        state_ != RunState::GameOver) {
        const char* controls =
            "Mouse Look  WASD Move  Space Up  LShift Down  C Focus  LMB/Z Fire  RMB/X Bomb";
        DrawText(controls, 16, GetScreenHeight() - 29, 17,
                 Color{185, 205, 220, 235});
        DrawText("CAMERA = HITBOX", GetScreenWidth() - 190, GetScreenHeight() - 56,
                 17, Color{255, 125, 145, 240});
    }
#ifdef RED_MIST_DEBUG_FEATURES
    DrawFPS(GetScreenWidth() - 90, 16);
    DrawText(debugInvincible_ ? "DEBUG INVINCIBLE: ON  [F4]"
                              : "DEBUG INVINCIBLE: OFF [F4]",
             GetScreenWidth() - 280, 75, 17,
             debugInvincible_ ? Color{125, 255, 200, 255}
                              : Color{180, 190, 205, 255});
    if (debugRender_) {
        DrawText("F3  HITBOX: yellow bullet / cyan body / magenta muzzle",
                 16, GetScreenHeight() - 57, 14,
                 Color{255, 225, 120, 255});
    }
#endif
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

    DrawDialogue();
    DrawSpellCutIn();

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
        const char* title = state_ == RunState::Won ? stageContent_->ClearTitle() : "GAME OVER";
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

void Game::DrawDialogue() const {
    if (!dialogueActive_) return;
    const Texture2D reimuPortrait = assets_.GetVisual(
        stageContent_->HeroPortraitVisual()).texture;
    const Texture2D rumiaPortrait = assets_.GetVisual(
        stageContent_->BossPortraitVisual()).texture;
    const auto lines = stageContent_->Dialogue();
    const int index = std::clamp(static_cast<int>(dialogueTimer_ / stageContent_->DialogueLineDuration()),
                                  0, static_cast<int>(lines.size()) - 1);
    const DialogueLine& line = lines[index];
    const float screenW = static_cast<float>(GetScreenWidth());
    const float screenH = static_cast<float>(GetScreenHeight());
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Color{10, 5, 22, 45});
    const float portraitH = screenH * 0.58F;
    if (reimuPortrait.id != 0) {
        const float portraitW = portraitH * reimuPortrait.width /
                                reimuPortrait.height;
        DrawTexturePro(reimuPortrait,
                       {0.0F, 0.0F, static_cast<float>(reimuPortrait.width),
                        static_cast<float>(reimuPortrait.height)},
                       {12.0F, screenH - portraitH - 82.0F,
                        portraitW, portraitH}, {0.0F, 0.0F}, 0.0F,
                       line.bossSpeaking ? Color{170, 170, 185, 165} : WHITE);
    }
    if (rumiaPortrait.id != 0) {
        const float portraitW = portraitH * rumiaPortrait.width /
                                rumiaPortrait.height;
        DrawTexturePro(rumiaPortrait,
                       {0.0F, 0.0F, static_cast<float>(rumiaPortrait.width),
                        static_cast<float>(rumiaPortrait.height)},
                       {screenW - portraitW - 12.0F,
                        screenH - portraitH - 82.0F, portraitW, portraitH},
                       {0.0F, 0.0F}, 0.0F,
                       line.bossSpeaking ? WHITE : Color{170, 170, 185, 165});
    }
    const Rectangle panel{screenW * 0.15F, screenH - 118.0F,
                          screenW * 0.7F, 92.0F};
    DrawRectangleRounded(panel, 0.2F, 12, Color{11, 7, 25, 230});
    DrawRectangleLinesEx(panel, 2.0F, Color{225, 155, 190, 220});
    DrawText(line.speaker, static_cast<int>(panel.x + 20.0F),
             static_cast<int>(panel.y + 10.0F), 22,
             Color{255, 165, 198, 255});
    DrawText(line.text, static_cast<int>(panel.x + 20.0F),
             static_cast<int>(panel.y + 42.0F), 21, RAYWHITE);
}

void Game::DrawSpellCutIn() const {
    if (spellCutInTimer_ <= 0.0F || !boss_ || !boss_->active ||
        boss_->SpellName() == nullptr) return;

    const float elapsed = kSpellCutInDuration - spellCutInTimer_;
    const float opacity = std::clamp(
        std::min(elapsed / 0.25F, spellCutInTimer_ / 0.35F), 0.0F, 1.0F);
    const unsigned char alpha = static_cast<unsigned char>(opacity * 255.0F);
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    DrawRectangle(0, 0, screenWidth, screenHeight,
                  Color{11, 2, 22, static_cast<unsigned char>(105.0F * opacity)});
    const float portraitHeight = std::min(600.0F,
                                          static_cast<float>(screenHeight) - 90.0F);
    const float portraitWidth = portraitHeight * 2.0F / 3.0F;
    const Rectangle frame{24.0F, static_cast<float>(screenHeight) - portraitHeight - 22.0F,
                          portraitWidth, portraitHeight};
    const Texture2D rumiaPortrait = assets_.GetVisual(
        stageContent_->BossPortraitVisual()).texture;
    if (rumiaPortrait.id != 0) {
        DrawTexturePro(rumiaPortrait,
                       {0.0F, 0.0F, static_cast<float>(rumiaPortrait.width),
                        static_cast<float>(rumiaPortrait.height)},
                       frame, {0.0F, 0.0F}, 0.0F, Color{255, 255, 255, alpha});
    }
    DrawRectangleLinesEx(frame, 3.0F, Color{255, 115, 175, alpha});

    const float bannerY = static_cast<float>(screenHeight) * 0.48F;
    DrawRectangle(0, static_cast<int>(bannerY), screenWidth, 146,
                  Color{15, 3, 30, static_cast<unsigned char>(220.0F * opacity)});
    DrawLine(0, static_cast<int>(bannerY), screenWidth,
             static_cast<int>(bannerY), Color{255, 95, 155, alpha});
    DrawLine(0, static_cast<int>(bannerY) + 146, screenWidth,
             static_cast<int>(bannerY) + 146, Color{255, 95, 155, alpha});

    const std::string name = boss_->SpellName();
    const std::size_t separator = name.find("  -  ");
    const std::string sign = name.substr(0, separator);
    const std::string title = separator == std::string::npos
                                  ? std::string{} : name.substr(separator + 5);
    const int textX = static_cast<int>(std::max(400.0F, portraitWidth + 36.0F));
    DrawText("SPELL CARD", textX, static_cast<int>(bannerY) + 16, 23,
             Color{255, 170, 205, alpha});
    DrawText(sign.c_str(), textX, static_cast<int>(bannerY) + 51, 35,
             Color{255, 245, 250, alpha});
    DrawText(title.c_str(), textX, static_cast<int>(bannerY) + 97, 26,
             Color{215, 180, 255, alpha});
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
