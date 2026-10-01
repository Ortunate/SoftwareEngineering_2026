// 本文件负责菜单输入、按键设置以及将玩家按键采集为输入快照。

#include "app/game.hpp"
#include "app/constants.hpp"
#include "config/settings.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace redmist {

void Game::UpdateMainMenu() {
#ifdef RED_MIST_DEBUG_FEATURES
    constexpr int itemCount = 4;
#else
    constexpr int itemCount = 3;
#endif
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        selectedMenuItem_ = (selectedMenuItem_ + itemCount - 1) % itemCount;
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        selectedMenuItem_ = (selectedMenuItem_ + 1) % itemCount;
    }

    const float centerX = static_cast<float>(GetScreenWidth()) * 0.5F;
#ifdef RED_MIST_DEBUG_FEATURES
    const float startY = static_cast<float>(GetScreenHeight()) * 0.5F + 10.0F;
#else
    const float startY = static_cast<float>(GetScreenHeight()) * 0.5F + 50.0F;
#endif
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
#ifdef RED_MIST_DEBUG_FEATURES
    } else if (confirmed && selectedMenuItem_ == 2) {
        StartShotTest();
    } else if (confirmed && selectedMenuItem_ == 3) {
#else
    } else if (confirmed && selectedMenuItem_ == 2) {
#endif
        shouldQuit_ = true;
    }
}

void Game::UpdateOptions() {
    if (optionsPage_ == OptionsPage::KeyBindings) {
        UpdateKeyBindings();
        return;
    }

    constexpr int itemCount = 5;
    if (IsKeyPressed(KEY_ESCAPE)) {
        state_ = RunState::MainMenu;
        selectedMenuItem_ = 1;
        return;
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
        selectedOption_ = (selectedOption_ + itemCount - 1) % itemCount;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
        selectedOption_ = (selectedOption_ + 1) % itemCount;

    const float centerX = GetScreenWidth() * 0.5F;
    const Vector2 mouse = GetMousePosition();
    const Vector2 movement = GetMouseDelta();
    bool hoveringItem = false;
    // 鼠标静止时不抢回键盘选择；点击仍可以直接选择当前指向的行。
    for (int index = 0; index < itemCount; ++index) {
        Rectangle row{centerX - 270.0F, kOptionsTop + index * kOptionsSpacing, 540.0F, 50.0F};
        if (CheckCollisionPointRec(mouse, row)) {
            hoveringItem = true;
            if (movement.x != 0 || movement.y != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                selectedOption_ = index;
        }
    }
    if (selectedOption_ < kBindingsOptionIndex) {
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) AdjustOption(-1);
        else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) AdjustOption(1);
    }
    const bool mouseConfirmed = hoveringItem && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (!IsKeyPressed(KEY_ENTER) && !mouseConfirmed) return;
    if (selectedOption_ < kBindingsOptionIndex) {
        // 绘制与命中测试共用按钮矩形；点击标签、数值或行内空白只选择，不调整。
        if (mouseConfirmed) {
            for (const int direction : {-1, 1}) {
                if (CheckCollisionPointRec(mouse, OptionArrowBounds(centerX, selectedOption_, direction)))
                    AdjustOption(direction);
            }
        } else {
            AdjustOption(1);
        }
    } else if (selectedOption_ == kBindingsOptionIndex) {
        optionsPage_ = OptionsPage::KeyBindings;
        selectedBinding_ = 0;
        waitingForBinding_ = false;
    } else {
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
        if (IsKeyPressed(KEY_ESCAPE)) { waitingForBinding_ = false; return; }
        if (!bindingCaptureReady_) {
            // 清空进入弹窗时的旧键盘队列，所有鼠标键释放后才开始收集新输入。
            while (GetKeyPressed() != KEY_NULL) {}
            bool held = IsKeyDown(KEY_ENTER);
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
            waitingForBinding_ = false;
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        optionsPage_ = OptionsPage::General;
        selectedOption_ = kBindingsOptionIndex;
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
    const bool confirmed = IsKeyPressed(KEY_ENTER) || mouseConfirmed;
    if (!confirmed) return;

    if (selectedBinding_ < bindingCount) {
        waitingForBinding_ = true;
        bindingCaptureReady_ = false;
    } else if (selectedBinding_ == resetIndex) {
        bindings_ = KeyBindings{};
        SaveSettings();
    } else {
        optionsPage_ = OptionsPage::General;
        selectedOption_ = kBindingsOptionIndex;
    }
}

void Game::AdjustFpsLimit(int offset) {
    const int count = static_cast<int>(kSupportedFpsLimits.size());
    fpsLimitIndex_ = std::clamp(fpsLimitIndex_ + offset, 0, count - 1);
    SetTargetFPS(kSupportedFpsLimits[fpsLimitIndex_]);
    SaveSettings();
}

void Game::AdjustOption(int offset) {
    if (selectedOption_ == 0) { AdjustFpsLimit(offset); return; }
    if (selectedOption_ == 1) {
        musicVolume_ = std::clamp(musicVolume_ + offset * 5, 0, 100);
        assets_.SetMusicVolumePercent(musicVolume_);
    } else if (selectedOption_ == 2) {
        mouseSensitivity_ = std::clamp(mouseSensitivity_ + offset * 10,
                                     kMinMouseSensitivity, kMaxMouseSensitivity);
    }
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

}  // namespace redmist
