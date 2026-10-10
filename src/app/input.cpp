// 本文件负责菜单输入、按键设置以及将玩家按键采集为输入快照。

#include "app/game.hpp"
#include "app/constants.hpp"
#include "config/settings.hpp"
#include "render/menu_layout.hpp"
#include "stages/stage1/content.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace koumakyou {

void Game::UpdateMainMenu() {
    const bool confirm = IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    if (titleIntro_) {
        if (confirm || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            titleIntro_ = false;
            menuMotion_.Confirm();
            assets_.PlaySoundEffect("menu_confirm");
        }
        return;
    }
    constexpr int itemCount = menu::kTitleRowCount;
    const int previous = selectedMenuItem_;
    const auto available = [](int index) { return index != 1 && index != 4; };
    int offset = 0;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) offset = -1;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) offset = 1;
    if (offset != 0) {
        do { selectedMenuItem_ = (selectedMenuItem_ + itemCount + offset) % itemCount; }
        while (!available(selectedMenuItem_));
    }
    if (IsKeyPressed(KEY_ESCAPE)) selectedMenuItem_ = 9;
    const menu::TitleLayout layout(GetScreenWidth(), GetScreenHeight(), itemCount);
    const Vector2 mouse = GetMousePosition();
    const Vector2 movement = GetMouseDelta();
    bool hovered = false;
    for (int index = 0; index < itemCount; ++index) {
        if (available(index) && CheckCollisionPointRec(mouse, layout.Row(index))) {
            hovered = true;
            if (movement.x != 0 || movement.y != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                selectedMenuItem_ = index;
        }
    }
    if (selectedMenuItem_ != previous) assets_.PlaySoundEffect("menu_move");
    if (!confirm && !(hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) return;
    menuMotion_.Confirm();
    assets_.PlaySoundEffect("menu_confirm");
    switch (selectedMenuItem_) {
        case 0:
        case 2:
        case 3:
            practiceMode_ = selectedMenuItem_ != 0;
            spellPractice_ = selectedMenuItem_ == 3;
            setupStep_ = spellPractice_ ? 3 : 0;
            challengeMode_ = false;
            state_ = RunState::StartSetup;
            break;
        case 5: state_ = RunState::Score; break;
        case 6: {
            // 音乐页借用 Stage 1 的流，离开时 EnterMainMenu 统一停止并释放。
            stage1::Stage1Content content;
            assets_.LoadStage(content.Assets());
            selectedTrack_ = 0;
            state_ = RunState::MusicRoom;
            break;
        }
        case 7: state_ = RunState::Credit; break;
        case 8: OpenOptions(); break;
        case 9: shouldQuit_ = true; break;
#ifdef KOUMAKYOU_DEBUG_FEATURES
        case 10: practiceMode_ = true; spellPractice_ = false; StartShotTest(); break;
#endif
        default: break;
    }
}

void Game::UpdateMenuPage() {
    const menu::Canvas canvas(GetScreenWidth(), GetScreenHeight());
    const auto clicked = [&](Rectangle bounds) {
        return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), bounds);
    };
    const bool back = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_X) ||
                      clicked(canvas.Rect(70, 675, 170, 42));
    if (back) {
        assets_.PlaySoundEffect("menu_cancel");
        if (state_ == RunState::StartSetup && setupStep_ > 0 && setupStep_ < 3) --setupStep_;
        else { EnterMainMenu(); titleIntro_ = false; }
        return;
    }
    int direction = 0;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_LEFT)) direction = -1;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_RIGHT)) direction = 1;
    bool confirm = IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
                   clicked(canvas.Rect(775, 675, 250, 42));
    if (state_ == RunState::StartSetup) {
        if (setupStep_ > 0) {
            const int previous = setupStep_ == 1 ? selectedShot_ : setupStep_ == 2 ? static_cast<int>(challengeMode_)
                : (practicePhase_ == 2 ? 0 : 1);
            for (int index = 0; index < 2; ++index) {
                const bool hover=CheckCollisionPointRec(GetMousePosition(),canvas.Rect(640,300+index*125,385,105));
                const bool moved=GetMouseDelta().x!=0 || GetMouseDelta().y!=0;
                if (hover && (moved || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
                    if (setupStep_ == 1) selectedShot_ = index;
                    else if (setupStep_ == 2) challengeMode_ = index == 1;
                    else practicePhase_ = index == 0 ? 2 : 4;
                }
            }
            if (direction != 0) {
                if (setupStep_ == 1) selectedShot_ = 1 - selectedShot_;
                else if (setupStep_ == 2) challengeMode_ = !challengeMode_;
                else practicePhase_ = practicePhase_ == 2 ? 4 : 2;
            }
            const int current = setupStep_ == 1 ? selectedShot_ : setupStep_ == 2 ? static_cast<int>(challengeMode_)
                : (practicePhase_ == 2 ? 0 : 1);
            if (previous != current) assets_.PlaySoundEffect("menu_move");
        }
        if (confirm) {
            menuMotion_.Confirm();
            assets_.PlaySoundEffect("menu_confirm");
            if (setupStep_ < 2) ++setupStep_;
            else StartRun();
        }
    } else if (state_ == RunState::MusicRoom) {
        const int previous = selectedTrack_;
        if (direction != 0) {
            selectedTrack_ = (selectedTrack_ + 3 + direction) % 3;
        }
        for (int index = 0; index < 3; ++index) {
            const auto row=canvas.Rect(280,260+index*95,680,70);
            if (CheckCollisionPointRec(GetMousePosition(),row) &&
                (GetMouseDelta().x!=0 || GetMouseDelta().y!=0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
                selectedTrack_ = index;
                confirm = confirm || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            }
        }
        if (selectedTrack_ != previous) assets_.PlaySoundEffect("menu_move");
        if (confirm) {
            menuMotion_.Confirm();
            assets_.PlaySoundEffect("menu_confirm");
            if (selectedTrack_ == 0) assets_.PlayMenuMusic();
            else if (selectedTrack_ == 1) assets_.PlayStageMusic();
            else assets_.PlayBossMusic();
        }
    }
}

void Game::UpdateOptions() {
    if (optionsPage_ == OptionsPage::KeyBindings) {
        UpdateKeyBindings();
        return;
    }

    constexpr int itemCount = menu::kOptionsCount;
    const int previous = selectedOption_;
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_X)) {
        assets_.PlaySoundEffect("menu_cancel");
        state_ = RunState::MainMenu;
        selectedMenuItem_ = 8;
        return;
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
        selectedOption_ = (selectedOption_ + itemCount - 1) % itemCount;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
        selectedOption_ = (selectedOption_ + 1) % itemCount;

    const menu::Canvas canvas(GetScreenWidth(), GetScreenHeight());
    const Vector2 mouse = GetMousePosition();
    const Vector2 movement = GetMouseDelta();
    bool hoveringItem = false;
    // 鼠标静止时不抢回键盘选择；点击仍可以直接选择当前指向的行。
    for (int index = 0; index < itemCount; ++index) {
        const Rectangle row = menu::OptionRow(canvas, index);
        if (CheckCollisionPointRec(mouse, row)) {
            hoveringItem = true;
            if (movement.x != 0 || movement.y != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                selectedOption_ = index;
        }
    }
    if (selectedOption_ < menu::kNumericOptions) {
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) AdjustOption(-1);
        else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) AdjustOption(1);
    }
    const bool mouseConfirmed = hoveringItem && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (selectedOption_ != previous) assets_.PlaySoundEffect("menu_move");
    if (!IsKeyPressed(KEY_ENTER) && !IsKeyPressed(KEY_Z) && !mouseConfirmed) return;
    if (selectedOption_ < menu::kNumericOptions) {
        // 绘制与命中测试共用按钮矩形；点击标签、数值或行内空白只选择，不调整。
        if (mouseConfirmed) {
            for (const int direction : {-1, 1}) {
                if (CheckCollisionPointRec(mouse, menu::OptionArrow(canvas, selectedOption_, direction)))
                    AdjustOption(direction);
            }
        } else {
            AdjustOption(1);
        }
    } else if (selectedOption_ == menu::kBindingsIndex) {
        menuMotion_.Confirm();
        assets_.PlaySoundEffect("menu_confirm");
        optionsPage_ = OptionsPage::KeyBindings;
        selectedBinding_ = 0;
        waitingForBinding_ = false;
    } else if (selectedOption_ == menu::kResetIndex) {
        menuMotion_.Confirm();
        const GameSettings defaults;
        bindings_ = defaults.bindings;
        fpsLimitIndex_ = 1;
        musicVolume_ = defaults.musicVolume;
        mouseSensitivity_ = defaults.mouseSensitivity;
        soundVolume_ = defaults.soundVolume;
        initialLives_ = defaults.initialLives;
        initialBombs_ = defaults.initialBombs;
        SetTargetFPS(60);
        assets_.SetMusicVolumePercent(musicVolume_);
        assets_.SetSoundVolumePercent(soundVolume_);
        SaveSettings();
        assets_.PlaySoundEffect("menu_confirm");
    } else {
        assets_.PlaySoundEffect("menu_cancel");
        state_ = RunState::MainMenu;
        selectedMenuItem_ = 8;
    }
}

void Game::UpdateKeyBindings() {
    constexpr int bindingCount = static_cast<int>(kBindingNames.size());
    constexpr int resetIndex = bindingCount;
    constexpr int backIndex = bindingCount + 1;
    constexpr int itemCount = bindingCount + 2;

    if (waitingForBinding_) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            assets_.PlaySoundEffect("menu_cancel"); waitingForBinding_ = false; return;
        }
        if (!bindingCaptureReady_) {
            // 清空进入弹窗时的旧键盘队列，所有鼠标键释放后才开始收集新输入。
            while (GetKeyPressed() != KEY_NULL) {}
            bool held = IsKeyDown(KEY_ENTER) || IsKeyDown(KEY_Z);
            for (int button = MOUSE_BUTTON_LEFT; button <= MOUSE_BUTTON_BACK; ++button)
                held = held || IsMouseButtonDown(button);
            bindingCaptureReady_ = !held;
            return;
        }
        int code = GetKeyPressed();
        for (int button = MOUSE_BUTTON_LEFT; button <= MOUSE_BUTTON_BACK; ++button) {
            if (IsMouseButtonPressed(button)) { code = MouseBinding(button); break; }
        }
        if (IsValidBinding(code)) {
            SetBinding(selectedBinding_, code);
            menuMotion_.Confirm();
            assets_.PlaySoundEffect("menu_confirm");
            waitingForBinding_ = false;
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        assets_.PlaySoundEffect("menu_cancel");
        optionsPage_ = OptionsPage::General;
        selectedOption_ = menu::kBindingsIndex;
        return;
    }
    const int previous = selectedBinding_;
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
                            kBindingsTop + static_cast<float>(index) * kBindingsSpacing,
                            540.0F, kBindingsHeight};
        if (CheckCollisionPointRec(mouse, row)) {
            if (GetMouseDelta().x != 0 || GetMouseDelta().y != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                selectedBinding_ = index;
            hoveringItem = true;
        }
    }
    const Rectangle resetButton{centerX - 270.0F, 500.0F, 260.0F, 42.0F};
    const Rectangle backButton{centerX + 10.0F, 500.0F, 260.0F, 42.0F};
    if (CheckCollisionPointRec(mouse, resetButton)) {
        if (GetMouseDelta().x != 0 || GetMouseDelta().y != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            selectedBinding_ = resetIndex;
        hoveringItem = true;
    } else if (CheckCollisionPointRec(mouse, backButton)) {
        if (GetMouseDelta().x != 0 || GetMouseDelta().y != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            selectedBinding_ = backIndex;
        hoveringItem = true;
    }

    const bool mouseConfirmed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                                hoveringItem;
    if (selectedBinding_ != previous) assets_.PlaySoundEffect("menu_move");
    const bool confirmed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_Z) || mouseConfirmed;
    if (!confirmed) return;
    menuMotion_.Confirm();
    assets_.PlaySoundEffect("menu_confirm");

    if (selectedBinding_ < bindingCount) {
        waitingForBinding_ = true;
        bindingCaptureReady_ = false;
    } else if (selectedBinding_ == resetIndex) {
        bindings_ = KeyBindings{};
        SaveSettings();
    } else {
        optionsPage_ = OptionsPage::General;
        selectedOption_ = menu::kBindingsIndex;
    }
}

void Game::AdjustFpsLimit(int offset) {
    const int count = static_cast<int>(kSupportedFpsLimits.size());
    fpsLimitIndex_ = std::clamp(fpsLimitIndex_ + offset, 0, count - 1);
    SetTargetFPS(kSupportedFpsLimits[fpsLimitIndex_]);
    SaveSettings();
}

void Game::AdjustOption(int offset) {
    menuMotion_.Confirm();
    if (selectedOption_ == 0) { AdjustFpsLimit(offset); assets_.PlaySoundEffect("menu_move"); return; }
    if (selectedOption_ == 1) {
        musicVolume_ = std::clamp(musicVolume_ + offset * 5, 0, 100);
        assets_.SetMusicVolumePercent(musicVolume_);
    } else if (selectedOption_ == 2) {
        mouseSensitivity_ = std::clamp(mouseSensitivity_ + offset * 10,
                                     kMinMouseSensitivity, kMaxMouseSensitivity);
    }
    if (selectedOption_ == 3) {
        soundVolume_ = std::clamp(soundVolume_ + offset * 5, 0, 100);
        assets_.SetSoundVolumePercent(soundVolume_);
    } else if (selectedOption_ == 4) initialLives_ = std::clamp(initialLives_ + offset, 1, 5);
    else if (selectedOption_ == 5) initialBombs_ = std::clamp(initialBombs_ + offset, 0, 3);
    assets_.PlaySoundEffect("menu_move");
    SaveSettings();
}

void Game::UpdatePlayer(float deltaTime) {
    const PlayerInput input{
        .mouseDelta = GetMouseDelta(),
        .mouseSensitivityScale = mouseSensitivity_ / 100.0F,
        .forward = BindingDown(bindings_.forward),
        .backward = BindingDown(bindings_.backward),
        .moveLeft = BindingDown(bindings_.moveLeft),
        .moveRight = BindingDown(bindings_.moveRight),
        .ascend = BindingDown(bindings_.ascend),
        .descend = BindingDown(bindings_.descend),
        .focus = BindingDown(bindings_.focus),
        .fire = BindingDown(bindings_.fire) || BindingDown(bindings_.fireAlt),
        .bomb = BindingPressed(bindings_.bomb) || BindingPressed(bindings_.bombAlt),
    };
    shotSoundTimer_ = std::max(0.0F, shotSoundTimer_ - deltaTime);
    if (input.fire && !dialogueActive_ && shotSoundTimer_ <= 0) {
        assets_.PlaySoundEffect("shoot");
        shotSoundTimer_ = 0.10F;
    }
    if (player_.Update(deltaTime, input, bullets_,
                       stageContent_->PlayerBulletVisual(),
                       stageContent_->PlayerHomingVisual(), !dialogueActive_)) {
        ApplyBombEffect();
    }
    player_.SyncCamera(camera_);
}

int Game::BindingAt(int index) const {
    return index >= 0 && index < static_cast<int>(kBindingMembers.size())
        ? bindings_.*kBindingMembers[index] : KEY_NULL;
}

void Game::SetBinding(int index, int key) {
    if (Rebind(bindings_, index, key)) SaveSettings();
}

std::string Game::KeyLabel(int key) {
    if (IsMouseBinding(key)) {
        constexpr std::array<const char*, 7> labels{
            "MOUSE LEFT", "MOUSE RIGHT", "MOUSE MIDDLE", "MOUSE 4 / SIDE 1",
            "MOUSE 5 / SIDE 2", "MOUSE 6", "MOUSE 7"};
        return labels[key - kMouseBindingBase];
    }
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

}  // namespace koumakyou
