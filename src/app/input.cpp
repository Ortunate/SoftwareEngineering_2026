// 本文件负责菜单输入、按键设置以及将玩家按键采集为输入快照。

#include "app/game.hpp"
#include "app/constants.hpp"
#include "ui/components.hpp"
#include "config/settings.hpp"
#include "render/menu_layout.hpp"
#include "resources/music_catalog.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace koumakyou {

void Game::UpdateMainMenu() {
    // 键盘与鼠标使用同一门禁；不缓存动画期间的确认，避免最后一帧突然跳入开局页。
    if (!titleEntrance_.Finished()) return;
    const auto input = ui::MenuInput::Read();
    constexpr int itemCount = menu::kTitleRowCount;
    const auto available = [this](int index) { return MainMenuItemAvailable(index); };
    if (input.back) {
        selectedMenuItem_ = 9;
        assets_.PlaySoundEffect("menu_cancel");
        return; // 首页没有上级页面；返回只选中 Quit，仍须单独确认才退出。
    }
    const menu::TitleLayout layout(GetScreenWidth(), GetScreenHeight(), itemCount);
    std::array<ui::Item,itemCount> items{};
    for(int index=0;index<itemCount;++index)items[index]={index,layout.Row(index),available(index)};
    const auto selection=ui::UpdateSelection(items,input,selectedMenuItem_);
    if(selection.changed)assets_.PlaySoundEffect("menu_move");
    if(!selection.confirmed)return;
    menuMotion_.Confirm();
    assets_.PlaySoundEffect("menu_confirm");
    switch (selectedMenuItem_) {
        case 0:
        case 1:
        case 2:
        case 3:
            practiceMode_ = selectedMenuItem_ == 2 || selectedMenuItem_ == 3;
            spellPractice_ = selectedMenuItem_ == 3;
            setupStep_ = spellPractice_ ? StartSetupStep::Spell : StartSetupStep::Difficulty;
            selectedShot_ = lastSelection_.shot;
            selectedCharacter_ = lastSelection_.character;
            selectedDifficulty_=lastSelection_.difficulty;
            if(!SupportsDifficulty(*native_combat::DifficultyFromIndex(selectedDifficulty_),
                practiceMode_ ? StageEntry::Practice : StageEntry::Campaign))selectedDifficulty_=1;
            selectedPracticeStage_ = lastSelection_.practiceStage-1;
            if(!kPracticeStages[selectedPracticeStage_]) {
                selectedPracticeStage_=0;
                lastSelection_.practiceStage=1;
                SaveSettings();
            }
            stageId_ = selectedMenuItem_==1 ? StageId::Extra : StageId::Stage1;
            // 符卡练习跳过模式页，仍沿用固定练习规则；不能将它的临时模式写回上次开局记录。
            challengeMode_ = !spellPractice_ && lastSelection_.challenge;
            practiceAttack_ = lastSelection_.practicePhase;
            if(spellPractice_)spellSelection_.page=spell::Page::Stage;
            state_ = RunState::StartSetup;
            break;
        case 4: OpenReplayList(); break;
        case 5:
            scoreMode_=lastSelection_.challenge?1:0;scoreShot_=lastSelection_.character*2+lastSelection_.shot;
            scoreDifficulty_=lastSelection_.difficulty;
            scorePageFromRun_=false;scoreHighlight_=-1;scoreViewStep_=ScoreViewStep::Difficulty;state_=RunState::Score;break;
        case 6: {
            // 试听独立加载音频，避免仅为浏览曲目加载一整关的模型和纹理。
            selectedTrack_ = 0;
            assets_.PlayMusicRoomTrack(0);
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
    if(state_==RunState::Score){UpdateScorePage();return;}
    if (state_ == RunState::StartSetup) {
        UpdateStartSetup();
        return;
    }
    const menu::PageLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto input = ui::MenuInput::Read();
    if (input.back) {
        assets_.PlaySoundEffect("menu_cancel");
        EnterMainMenu();
        return;
    }
    if (state_ != RunState::MusicRoom) {
        if (state_==RunState::Credit && input.confirm) {
            assets_.PlaySoundEffect("menu_confirm");
            EnterMainMenu();
        }
        return;
    }
    constexpr int count=static_cast<int>(music::kTracks.size()),pageSize=menu::PageLayout::kTracksPerPage;
    const int first=selectedTrack_/pageSize*pageSize;
    for(int direction:{-1,1}) {
        const bool enabled=direction<0?first>0:first+pageSize<count;
        if((enabled && input.horizontal==direction) || ui::ArrowButton{layout.TrackPageArrow(direction),direction,enabled}.Update(input).activated) {
            selectedTrack_=std::clamp(first+direction*pageSize,0,count-1);
            assets_.PlaySoundEffect("menu_move");return;
        }
    }
    // 左右只负责翻页；边界处也消费输入，不能退化成选曲或让同帧确认播放旧页曲目。
    if(input.horizontal!=0)return;
    std::array<ui::Item,count> items{};
    for(int index=0;index<count;++index)
        items[index]={index,index>=first && index<first+pageSize?layout.Track(index-first):Rectangle{-100,-100,0,0}};
    auto navigation=input;
    const float wheel=GetMouseWheelMove();
    if(wheel!=0)navigation.vertical=wheel>0?-1:1;
    const auto selection=ui::UpdateSelection(items,navigation,selectedTrack_,ui::Navigation::Vertical);
    if(selection.changed)assets_.PlaySoundEffect("menu_move");
    if(!selection.confirmed)return;
    menuMotion_.Confirm();
    assets_.PlaySoundEffect(assets_.PlayMusicRoomTrack(selectedTrack_)?"menu_confirm":"menu_cancel");
}

void Game::UpdateOptions() {
    if (optionsPage_ == OptionsPage::KeyBindings) {
        UpdateKeyBindings();
        return;
    }

    constexpr int itemCount = menu::kOptionsCount;
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
    const auto canvas=menu::PageCanvas(GetScreenWidth(),GetScreenHeight());
    std::array<ui::Item,itemCount> items{};
    for(int index=0;index<itemCount;++index)items[index]={index,menu::OptionRow(canvas,index)};
    auto navigationInput=input;navigationInput.vertical=direction;navigationInput.horizontal=0;
    const auto selection=ui::UpdateSelection(items,navigationInput,selectedOption_,ui::Navigation::Vertical);
    if(selection.changed)assets_.PlaySoundEffect("menu_move");
    if (selectedOption_ < menu::kNumericOptions) {
        const ui::ValueSelector selector{menu::OptionRow(canvas,selectedOption_),menu::OptionArrow(canvas,selectedOption_,-1),
                                         menu::OptionArrow(canvas,selectedOption_,1)};
        auto valueInput=input;if(direction!=0)valueInput.horizontal=0;
        const int adjustment=selector.Update(valueInput,true);
        if(adjustment!=0)AdjustOption(adjustment);
        return;
    }
    if(!selection.confirmed)return;
    if (selectedOption_ == menu::kBindingsIndex) {
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
        aimAssistEnabled_ = defaults.aimAssistEnabled;
        soundVolume_ = defaults.soundVolume;
        initialLives_ = defaults.initialLives;
        initialBombs_ = defaults.initialBombs;
        SetTargetFPS(60);
        assets_.SetMusicVolumePercent(musicVolume_);
        assets_.SetSoundVolumePercent(soundVolume_);
        SaveSettings();
        assets_.PlaySoundEffect("menu_confirm");
    }
}

void Game::UpdateKeyBindings() {
    constexpr int bindingCount = static_cast<int>(kBindingNames.size());
    constexpr int resetIndex = bindingCount;
    constexpr int itemCount = bindingCount + 1;
    const auto input = ui::MenuInput::Read();

    if (waitingForBinding_) {
        if (input.back) {
            assets_.PlaySoundEffect("menu_cancel"); waitingForBinding_ = false; return;
        }
        if (!bindingCaptureReady_) {
            // 清空进入录入状态的旧键盘队列，确认键及所有鼠标键释放后才收集新输入；返回操作始终优先。
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
    const menu::BindingsLayout layout(GetScreenWidth(),GetScreenHeight());
    std::array<ui::Item,itemCount> items{};
    for (int index = 0; index < itemCount; ++index)
        items[index]={index,layout.Row(index)};
    const auto selection=ui::UpdateSelection(items,input,selectedBinding_);
    if(selection.changed)assets_.PlaySoundEffect("menu_move");
    if(!selection.confirmed)return;
    menuMotion_.Confirm();
    assets_.PlaySoundEffect("menu_confirm");

    if (selectedBinding_ < bindingCount) {
        waitingForBinding_ = true;
        bindingCaptureReady_ = false;
    } else if (selectedBinding_ == resetIndex) {
        bindings_ = KeyBindings{};
        SaveSettings();
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
    if (selectedOption_ == 4) {
        soundVolume_ = std::clamp(soundVolume_ + offset * 5, 0, 100);
        assets_.SetSoundVolumePercent(soundVolume_);
    } else if (selectedOption_ == 5) initialLives_ = std::clamp(initialLives_ + offset, 1, 5);
    else if (selectedOption_ == 3) aimAssistEnabled_ = offset>0;
    else if (selectedOption_ == 6) initialBombs_ = std::clamp(initialBombs_ + offset, 0, 3);
    assets_.PlaySoundEffect("menu_move");
    SaveSettings();
}

PlayerInput Game::ReadPlayerInput() const {
    return PlayerInput{
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
        .aimAssistEnabled = aimAssistEnabled_,
    };
}

void Game::UpdatePlayer(float deltaTime) { UpdatePlayer(deltaTime,runInput_.player); }

void Game::UpdatePlayer(float deltaTime,const PlayerInput& input) {
    playerBombStartedThisFrame_=false;
    shotSoundTimer_ = std::max(0.0F, shotSoundTimer_ - deltaTime);
    if (input.fire && !dialogueActive_ && shotSoundTimer_ <= 0) {
        assets_.PlaySoundEffect("shoot");
        shotSoundTimer_ = 0.10F;
    }
    std::array<PlayerAimTarget, kActorCapacity + 1> aimTargets{};
    std::size_t aimTargetCount = 0;
    actors_.ForEachActive([&](const behavior::Actor& enemy) {
        aimTargets[aimTargetCount++] = {enemy.position, enemy.hitRadius,
            enemy.CanBeTargeted() && enemy.health>0 && enemy.CanTakeDamage()};
    });
    if (boss_ && boss_->active) aimTargets[aimTargetCount++] = {boss_->position, boss_->hitRadius, boss_->health>0 && boss_->CanTakeDamage()};
    const bool usedBomb=player_.Update(deltaTime,input,bullets_,stageContent_->PlayerVisuals(player_.Shot()),!dialogueActive_,
                                       {aimTargets.data(),aimTargetCount});
    player_.SyncCamera(camera_);
    // 战斗总控在敌弹运动后推进攻击；本帧刚释放的 Bomb 保留 frame=0，不能提前消费首个伤害帧。
    if(usedBomb) { ApplyBombEffect();playerBombStartedThisFrame_=true; }
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
