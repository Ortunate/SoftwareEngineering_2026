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
    const PlayerInput input{
        .mouseDelta = GetMouseDelta(),
        .forward = IsKeyDown(bindings_.forward),
        .backward = IsKeyDown(bindings_.backward),
        .moveLeft = IsKeyDown(bindings_.moveLeft),
        .moveRight = IsKeyDown(bindings_.moveRight),
        .ascend = IsKeyDown(bindings_.ascend),
        .descend = IsKeyDown(bindings_.descend),
        .focus = IsKeyDown(bindings_.focus),
        .fire = IsKeyDown(bindings_.fire) || IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        .bomb = IsKeyPressed(bindings_.bomb) ||
                IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),
    };
    if (player_.Update(deltaTime, input, bullets_,
                       stageContent_->PlayerBulletVisual(),
                       stageContent_->PlayerHomingVisual(), !dialogueActive_)) {
        ApplyBombEffect();
    }
    player_.SyncCamera(camera_);
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

}  // namespace redmist
