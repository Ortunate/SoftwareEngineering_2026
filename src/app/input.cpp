// 本文件负责菜单输入、按键设置以及将玩家按键采集为输入快照。

#include "app/game.hpp"
#include "app/constants.hpp"
#include "app/ui_input.hpp"
#include "config/settings.hpp"
#include "render/menu_layout.hpp"
#include "stages/stage1/content.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace koumakyou {

void Game::UpdateMainMenu() {
    // 键盘与鼠标使用同一门禁；不缓存动画期间的确认，避免最后一帧突然跳入开局页。
    if (!titleEntrance_.Finished()) return;
    const auto input = ui::MenuInput::Read();
    const bool confirm = input.confirm;
    constexpr int itemCount = menu::kTitleRowCount;
    const int previous = selectedMenuItem_;
    const auto available = [](int index) { return index != 1 && index != 4; };
    const int offset = input.Direction();
    if (offset != 0) {
        do { selectedMenuItem_ = (selectedMenuItem_ + itemCount + offset) % itemCount; }
        while (!available(selectedMenuItem_));
    }
    if (input.back) {
        selectedMenuItem_ = 9;
        assets_.PlaySoundEffect("menu_cancel");
        return; // 首页没有上级页面；返回只选中 Quit，仍须单独确认才退出。
    }
    const menu::TitleLayout layout(GetScreenWidth(), GetScreenHeight(), itemCount);
    bool mouseConfirmed = false;
    for (int index = 0; index < itemCount; ++index)
        if (available(index)) mouseConfirmed = input.Select(layout.Row(index),index,selectedMenuItem_) || mouseConfirmed;
    if (selectedMenuItem_ != previous) assets_.PlaySoundEffect("menu_move");
    if (!confirm && !mouseConfirmed) return;
    menuMotion_.Confirm();
    assets_.PlaySoundEffect("menu_confirm");
    switch (selectedMenuItem_) {
        case 0:
        case 2:
        case 3:
            practiceMode_ = selectedMenuItem_ != 0;
            spellPractice_ = selectedMenuItem_ == 3;
            setupStep_ = spellPractice_ ? StartSetupStep::Spell : StartSetupStep::Difficulty;
            selectedShot_ = lastSelection_.shot;
            // 符卡练习跳过模式页，仍沿用固定练习规则；不能将它的临时模式写回上次开局记录。
            challengeMode_ = !spellPractice_ && lastSelection_.challenge;
            practicePhase_ = lastSelection_.practicePhase;
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
    if (state_ == RunState::StartSetup) {
        UpdateStartSetup();
        return;
    }
    const menu::PageLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto input = ui::MenuInput::Read();
    const bool back = input.back || input.Clicked(layout.Back());
    if (back) {
        assets_.PlaySoundEffect("menu_cancel");
        EnterMainMenu();
        return;
    }
    if (state_ != RunState::MusicRoom) {
        if (input.confirm) {
            assets_.PlaySoundEffect("menu_confirm");
            EnterMainMenu();
        }
        return;
    }
    const int previous = selectedTrack_;
    // 音乐页的第 3 项为返回；底部按钮与曲目共用键盘选择和高亮。
    selectedTrack_ = (selectedTrack_ + 4 + input.Direction()) % 4;
    bool confirm = input.confirm;
    for (int index = 0; index < 3; ++index)
        confirm = input.Select(layout.Track(index),index,selectedTrack_) || confirm;
    input.Select(layout.Back(),3,selectedTrack_);
    if (selectedTrack_ != previous) assets_.PlaySoundEffect("menu_move");
    if (!confirm) return;
    menuMotion_.Confirm();
    assets_.PlaySoundEffect("menu_confirm");
    if (selectedTrack_ == 0) assets_.PlayMenuMusic();
    else if (selectedTrack_ == 1) assets_.PlayStageMusic();
    else if (selectedTrack_ == 2) assets_.PlayBossMusic();
    else EnterMainMenu();
}

void Game::UpdateOptions() {
    if (optionsPage_ == OptionsPage::KeyBindings) {
        UpdateKeyBindings();
        return;
    }

    constexpr int itemCount = menu::kOptionsCount;
    const int previous = selectedOption_;
    const auto input = ui::MenuInput::Read();
    if (input.back) {
        assets_.PlaySoundEffect("menu_cancel");
        state_ = RunState::MainMenu;
        selectedMenuItem_ = 8;
        return;
    }
    // 数值项保留 A/D、左右键调整；其余行允许两组方向导航。
    const int direction = input.vertical != 0 ? input.vertical :
        selectedOption_ >= menu::kNumericOptions ? input.horizontal : 0;
    selectedOption_ = (selectedOption_ + itemCount + direction) % itemCount;
    const auto canvas=menu::PageCanvas(GetScreenWidth(),GetScreenHeight());
    bool mouseConfirmed = false;
    for (int index = 0; index < itemCount; ++index)
        mouseConfirmed = input.Select(menu::OptionRow(canvas,index),index,selectedOption_) || mouseConfirmed;
    if (selectedOption_ < menu::kNumericOptions && input.vertical == 0 && input.horizontal != 0 && direction == 0)
        AdjustOption(input.horizontal);
    if (selectedOption_ != previous) assets_.PlaySoundEffect("menu_move");
    if (!input.confirm && !mouseConfirmed) return;
    if (selectedOption_ < menu::kNumericOptions) {
        const bool decrease = mouseConfirmed && input.Clicked(menu::OptionArrow(canvas,selectedOption_,-1));
        AdjustOption(decrease ? -1 : 1);
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
    const auto input = ui::MenuInput::Read();

    if (waitingForBinding_) {
        if (input.back) {
            assets_.PlaySoundEffect("menu_cancel"); waitingForBinding_ = false; return;
        }
        if (!bindingCaptureReady_) {
            // 清空激活弹窗的旧键盘队列，确认键及所有鼠标键释放后才收集新输入；返回操作始终优先。
            while (GetKeyPressed() != KEY_NULL) {}
            bool held = IsKeyDown(KEY_ENTER) || IsKeyDown(KEY_Z) || IsKeyDown(KEY_SPACE);
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

    if (input.back) {
        assets_.PlaySoundEffect("menu_cancel");
        optionsPage_ = OptionsPage::General;
        selectedOption_ = menu::kBindingsIndex;
        return;
    }
    const int previous = selectedBinding_;
    selectedBinding_ = (selectedBinding_ + itemCount + input.Direction()) % itemCount;
    const menu::BindingsLayout layout(GetScreenWidth(),GetScreenHeight());
    bool confirmed = input.confirm;
    for (int index = 0; index < bindingCount; ++index)
        confirmed = input.Select(layout.Row(index),index,selectedBinding_) || confirmed;
    confirmed = input.Select(layout.Button(0),resetIndex,selectedBinding_) || confirmed;
    confirmed = input.Select(layout.Button(1),backIndex,selectedBinding_) || confirmed;
    if (selectedBinding_ != previous) assets_.PlaySoundEffect("menu_move");
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
        .fire = BindingDown(bindings_.fire),
        .bomb = BindingPressed(bindings_.bomb),
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
            "鼠标左键", "鼠标右键", "鼠标中键", "鼠标侧键 1（4）",
            "鼠标侧键 2（5）", "鼠标按键 6", "鼠标按键 7"};
        return labels[key - kMouseBindingBase];
    }
    // 字母、数字和功能键沿用实体键帽标识；小键盘单独标注，避免与主键区数字混淆。
    if (key >= KEY_A && key <= KEY_Z) return std::string(1,static_cast<char>('A'+key-KEY_A));
    if (key >= KEY_ZERO && key <= KEY_NINE) return std::string(1,static_cast<char>('0'+key-KEY_ZERO));
    if (key >= KEY_F1 && key <= KEY_F12) return TextFormat("F%d",key-KEY_F1+1);
    if (key >= KEY_KP_0 && key <= KEY_KP_9) return TextFormat("小键盘 %d",key-KEY_KP_0);
    switch (key) {
        case KEY_NULL: return "未设置";
        case KEY_SPACE: return "空格";
        case KEY_ESCAPE: return "Esc";
        case KEY_ENTER: return "回车";
        case KEY_TAB: return "Tab";
        case KEY_BACKSPACE: return "退格";
        case KEY_INSERT: return "插入";
        case KEY_DELETE: return "删除";
        case KEY_RIGHT: return "向右";
        case KEY_LEFT: return "向左";
        case KEY_DOWN: return "向下";
        case KEY_UP: return "向上";
        case KEY_PAGE_UP: return "上翻页";
        case KEY_PAGE_DOWN: return "下翻页";
        case KEY_HOME: return "Home（行首）";
        case KEY_END: return "End（行尾）";
        case KEY_CAPS_LOCK: return "大写锁定";
        case KEY_SCROLL_LOCK: return "滚动锁定";
        case KEY_NUM_LOCK: return "数字锁定";
        case KEY_PRINT_SCREEN: return "截屏";
        case KEY_PAUSE: return "暂停键";
        case KEY_LEFT_SHIFT: return "左 Shift";
        case KEY_LEFT_CONTROL: return "左 Ctrl";
        case KEY_LEFT_ALT: return "左 Alt";
        case KEY_RIGHT_SHIFT: return "右 Shift";
        case KEY_RIGHT_CONTROL: return "右 Ctrl";
        case KEY_RIGHT_ALT: return "右 Alt";
        case KEY_LEFT_SUPER: return "左系统键";
        case KEY_RIGHT_SUPER: return "右系统键";
        case KEY_KB_MENU: return "菜单键";
        case KEY_KP_DECIMAL: return "小键盘 .";
        case KEY_KP_DIVIDE: return "小键盘 /";
        case KEY_KP_MULTIPLY: return "小键盘 *";
        case KEY_KP_SUBTRACT: return "小键盘 -";
        case KEY_KP_ADD: return "小键盘 +";
        case KEY_KP_ENTER: return "小键盘回车";
        case KEY_KP_EQUAL: return "小键盘 =";
        case KEY_APOSTROPHE: return "'";
        case KEY_COMMA: return ",";
        case KEY_MINUS: return "-";
        case KEY_PERIOD: return ".";
        case KEY_SLASH: return "/";
        case KEY_SEMICOLON: return ";";
        case KEY_EQUAL: return "=";
        case KEY_LEFT_BRACKET: return "[";
        case KEY_BACKSLASH: return "\\";
        case KEY_RIGHT_BRACKET: return "]";
        case KEY_GRAVE: return "`";
        default: return TextFormat("按键 %d",key);
    }
}

}  // namespace koumakyou
