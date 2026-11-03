// 本文件负责录像保存弹窗、观战相机及定位操作；观察输入不进入模拟输入流。
#include "app/game.hpp"
#include "platform/window_input.hpp"
#include "ui/input.hpp"
#include "ui/components.hpp"
#include "ui/command_bar.hpp"
#include "render/menu_layout.hpp"
#include "render/replay_layout.hpp"
#include "ui/typography.hpp"
#include "raymath.h"
#include <algorithm>
#include <chrono>

namespace koumakyou {
namespace {
// 保存询问继续复用暂停布局；命名布局与编辑行为统一由ui::NameEntry提供。
struct SaveLayout {
    menu::OverlayLayout overlay{GetScreenWidth(),GetScreenHeight()};
    Rectangle Ask(int index) const { return overlay.PauseRow(index); }
};
}
bool Game::CanSaveReplay() const {
    return replayMode_!=replay::Mode::Playback && replayEnded_ && !replayData_.frames.empty() && replayData_.continues==0 &&
        replaySaveState_!=ReplaySaveState::None && replaySaveState_!=ReplaySaveState::Saved;
}
bool Game::ReplaySaveModalOpen() const {
    return replaySaveState_==ReplaySaveState::Ask || replaySaveState_==ReplaySaveState::Slots || replaySaveState_==ReplaySaveState::Overwrite || replaySaveState_==ReplaySaveState::Naming || replaySaveState_==ReplaySaveState::Failed;
}
void Game::RefreshReplayAvailability(float seconds,bool force) {
    replayScanSeconds_+=seconds;const bool focused=IsWindowFocused();
    force=force || (focused && !replayMenuFocused_);replayMenuFocused_=focused;
    if(!force && replayScanSeconds_<1)return;
    replayScanSeconds_=0;replayAvailable_=false;
    std::error_code error;
    if(!std::filesystem::exists("replays",error)) {if(error)replayStatus_="无法读取录像目录";return;}
    std::filesystem::directory_iterator iterator("replays",error),end;
    while(!error && iterator!=end) {
        const auto& file=*iterator;
        if(file.path().extension()==".krep" && file.is_regular_file(error)){replayAvailable_=true;break;}
        if(error)break;
        iterator.increment(error);
    }
    if(error){replayAvailable_=false;replayStatus_="无法读取录像目录";}
}
bool Game::MainMenuItemAvailable(int index) const { return index!=4 || replayAvailable_; }
void Game::RequestReplayAction(ReplayAction action) {
    replayAfterSave_=action;
    if(CanSaveReplay() && (replaySaveState_!=ReplaySaveState::Declined || action==ReplayAction::Stay)) {
        replaySaveState_=ReplaySaveState::Ask;replayChoice_=0;replaySaveMotion_={};
        if(!replayNamePrepared_) {
            replayName_=scoreProfile_.lastName;replayNameEditor_={.replaceOnType=!replayName_.empty()};replayNamePrepared_=true;
        }
        replaySaveSlot_=0;replayOverwrite_=false;
        return;
    }
    FinishReplayAction();
}
void Game::FinishReplayAction() {
    (void)DisableWindowInputMethods(GetWindowHandle());
    switch(replayAfterSave_) {
    case ReplayAction::Retry:StartRun();break;
    case ReplayAction::Menu:EnterMainMenu();break;
    case ReplayAction::StageSelection:ReturnToPracticeStage();break;
    case ReplayAction::SpellSelection:ReturnToSpellPractice();break;
    case ReplayAction::ContinueStage:ContinueStageClear();break;
    case ReplayAction::Stay:break;
    }
}
void Game::UpdateReplaySave() {
    const SaveLayout layout;auto input=ui::MenuInput::Read();
    const auto sound=[&](const ui::SelectionResult& result){if(result.changed)assets_.PlaySoundEffect("menu_move");if(result.confirmed)assets_.PlaySoundEffect("menu_confirm");};
    if(replaySaveState_==ReplaySaveState::Ask) {
        const std::array<ui::Item,2> items{{{0,layout.Ask(0)},{1,layout.Ask(1)}}};
        const auto result=ui::UpdateSelection(items,input,replayChoice_);sound(result);
        if(result.back || (result.confirmed && replayChoice_==1)) {replaySaveState_=ReplaySaveState::Declined;replayStatus_.clear();FinishReplayAction();return;}
        if(result.confirmed) {
            replayStatus_.clear();ReadReplayDirectory();selectedReplay_=0;
            while(selectedReplay_<replay::kSlotCount && replayOccupied_[selectedReplay_])++selectedReplay_;
            if(selectedReplay_==replay::kSlotCount)selectedReplay_=0;
            replaySaveState_=ReplaySaveState::Slots;
        }
        return;
    }
    if(replaySaveState_==ReplaySaveState::Slots) {
        if(input.back){replaySaveState_=ReplaySaveState::Ask;return;}
        if(!UpdateReplayTable(input,true))return;
        replaySaveSlot_=selectedReplay_+1;replayOverwrite_=false;
        if(replayOccupied_[selectedReplay_]){replaySaveState_=ReplaySaveState::Overwrite;replayChoice_=1;return;}
        replaySaveState_=ReplaySaveState::Naming;replayNameEditor_.selected=ui::NameEntryState::kSave;
        (void)EnableWindowTextInput(GetWindowHandle());while(GetCharPressed()){};return;
    }
    if(replaySaveState_==ReplaySaveState::Overwrite) {
        const std::array<ui::Item,2> items{{{0,layout.Ask(0)},{1,layout.Ask(1)}}};
        const auto result=ui::UpdateSelection(items,input,replayChoice_);sound(result);
        if(result.back || (result.confirmed && replayChoice_==1)){replaySaveState_=ReplaySaveState::Slots;return;}
        if(result.confirmed) {
            replayOverwrite_=true;replaySaveState_=ReplaySaveState::Naming;replayNameEditor_.selected=ui::NameEntryState::kSave;
            (void)EnableWindowTextInput(GetWindowHandle());while(GetCharPressed()){};
        }
        return;
    }
    const ui::NameEntry editor({GetScreenWidth(),GetScreenHeight()});
    const auto result=editor.Update(replayName_,replayNameEditor_,{score::kNameLength},ui::NameEntryInput::Read());
    if(result.edited)replayStatus_.clear();
    if(result.selectionChanged)assets_.PlaySoundEffect("menu_move");
    if(result.activated)assets_.PlaySoundEffect("menu_confirm");
    if(result.action==ui::NameEntryAction::Cancel) {replaySaveState_=ReplaySaveState::Slots;(void)DisableWindowInputMethods(GetWindowHandle());return;}
    if(result.action!=ui::NameEntryAction::Submit)return;
    std::string normalized;
    if(!ui::NameEntry::Valid(replayName_,{score::kNameLength}) || !replay::NormalizeName(replayName_,normalized)) {
        replayStatus_="请输入 1 至 8 个 ASCII 字符，名称不能全为空格";replaySaveState_=ReplaySaveState::Failed;return;
    }
    replayData_.name=replayName_;std::filesystem::path saved;std::string error;
    if(!replay::SaveSlot("replays",replayData_,replaySaveSlot_,replayOverwrite_,saved,error)) {
        TraceLog(LOG_WARNING,"REPLAY SAVE: %s",error.c_str());
        replayStatus_="保存失败，请检查目录权限后重试；名字已保留";replaySaveState_=ReplaySaveState::Failed;return;
    }
    const bool remembered=RememberName(replayName_);
    replaySaveState_=ReplaySaveState::Saved;replayStatus_=remembered?"":"默认名称保存失败，仅在本次会话内保留";RefreshReplayAvailability(0,true);FinishReplayAction();
}
void Game::DrawReplaySave() const {
    const SaveLayout layout;
    const ui::DrawContext context{assets_.Common().uiFont,layout.overlay.canvas.scale,1,replaySaveMotion_.Phase(),replaySaveMotion_.Pulse()};
    if(replaySaveState_==ReplaySaveState::Slots) {
        DrawReplayTable(true);
        const ui::CommandBar bar(GetScreenWidth(),GetScreenHeight());
        const std::array<ui::KeyHint,4> hints{{{"W / S / ↑↓","选择槽位"},{"A / D / ←→","翻页"},{"Z / Enter / 左键","确认"},{"Esc / 右键","返回"}}};bar.Draw(context,hints);return;
    }
    const bool ask=replaySaveState_==ReplaySaveState::Ask || replaySaveState_==ReplaySaveState::Overwrite;
    if(ask) {
        if(replaySaveState_==ReplaySaveState::Overwrite)DrawReplayTable(true);
        // 保存询问与暂停共用 OverlayLayout、Modal 和文字按钮，输入命中亦取同一 PauseRow。
        auto modalContext=context;modalContext.scale=layout.overlay.canvas.scale;modalContext.theme.accent=GOLD;
        ui::Modal{{0,0,static_cast<float>(GetScreenWidth()),static_cast<float>(GetScreenHeight())},layout.overlay.PausePanel(2),layout.overlay.canvas.Point(380,200)}.Draw(modalContext,replaySaveState_==ReplaySaveState::Overwrite?"覆盖这个录像槽位？":"保存 Replay？",RAYWHITE);
        for(int i=0;i<2;++i)ui::Button{layout.Ask(i),true,ui::ButtonStyle::Text}.Draw(modalContext,i==0?"是":"否",{replaySaveMotion_.Weight(i),replaySaveMotion_.Reveal(i*0.02F)});
    } else {
        const ui::NameEntry editor({GetScreenWidth(),GetScreenHeight()});
        const auto slot=std::string("No. ")+TextFormat("%02d",replaySaveSlot_);
        editor.Draw(context,replayName_,replayNameEditor_,{score::kNameLength},{"录像名称",slot.c_str(),"保存 Replay",replayStatus_.c_str()});
        editor.DrawHints(context);return;
    }
    const ui::CommandBar bar(GetScreenWidth(),GetScreenHeight());
    const std::array<ui::KeyHint,3> hints{{{"方向键 / 鼠标","选择"},{"Z / Enter / 左键","确认"},{"Esc / 右键","返回"}}};
    bar.Draw(context,hints);
}
void Game::SeekReplay(double seconds) {
    const auto target=static_cast<std::size_t>(std::upper_bound(replayTimes_.begin(),replayTimes_.end(),std::clamp(seconds,0.0,replayTimes_.back()))-replayTimes_.begin()-1);
    if(target==replayCursor_) {replaySeeking_=false;return;}
    if(target<replayCursor_) {
        assets_.SetMusicVolumePercent(0);assets_.SetSoundVolumePercent(0);replayAudioMuted_=true;
        // 从整轮首入口重算，确保跨关库存、rank和种子走相同路径。
        RestoreReplayEntry();replaySegment_=0;
        StartRun(false,true);replayCursor_=0;replayMusicSince_=0;replayMusicSlot_=assets_.MusicIdentity();
    }
    replayTarget_=target;replaySeeking_=true;replayFinished_=false;replayEnded_=false;replayBudget_=0;replayStatus_.clear();
}
void Game::UpdateObserver(float seconds) {
    if(replayView_==0){observer_=camera_;return;}
    if(!IsWindowHidden() && !IsCursorHidden())DisableCursor();
    const auto mouse=GetMouseDelta();
    const float sensitivity=kBaseMouseSensitivity*mouseSensitivity_/100.0F;
    // 环绕视角移动的是玩家背后的相机：正 yaw 使相机向屏幕右侧绕行；自由视角则沿用玩家转头约定。
    orbitYaw_+=(replayView_==1?1.0F:-1.0F)*mouse.x*sensitivity;
    orbitPitch_=std::clamp(orbitPitch_-mouse.y*sensitivity,-1.45F,1.45F);
    const auto aim=Vector3Normalize(Vector3Subtract(camera_.target,camera_.position));
    Vector3 direction{std::sin(orbitYaw_)*std::cos(orbitPitch_),std::sin(orbitPitch_),std::cos(orbitYaw_)*std::cos(orbitPitch_)};
    observer_.up={0,1,0};observer_.fovy=72;observer_.projection=CAMERA_PERSPECTIVE;
    if(replayView_==1) {
        orbitDistance_=std::clamp(orbitDistance_-GetMouseWheelMove()*0.3F,1.0F,12.0F);
        const float yaw=std::atan2(aim.x,aim.z)+orbitYaw_;
        direction={std::sin(yaw)*std::cos(orbitPitch_),-std::sin(orbitPitch_),std::cos(yaw)*std::cos(orbitPitch_)};
        observer_.position=Vector3Add(Vector3Subtract(player_.Position(),Vector3Scale(direction,orbitDistance_)),{0,1,0});observer_.target=Vector3Add(player_.Position(),aim);
    } else {
        // 与 Player::Right 一致：朝 +Z、世界上方 +Y 时，屏幕右侧为 -X。
        const auto right=Vector3Normalize(Vector3CrossProduct(direction,{0,1,0}));Vector3 move{};
        move=Vector3Add(move,Vector3Scale(direction,float(BindingDown(bindings_.forward)-BindingDown(bindings_.backward))));
        move=Vector3Add(move,Vector3Scale(right,float(BindingDown(bindings_.moveRight)-BindingDown(bindings_.moveLeft))));move.y+=BindingDown(bindings_.ascend)-BindingDown(bindings_.descend);
        // 观察移动复用本机移动/升降/低速绑定，归一化避免斜向移动加速；只改变观察相机。
        move=Vector3Normalize(move);
        observer_.position=Vector3Add(observer_.position,Vector3Scale(move,std::min(seconds,0.1F)*(BindingDown(bindings_.focus)?1.0F:4.0F)));
        observer_.target=Vector3Add(observer_.position,direction);
    }
}
void Game::DrawReplayBar() const {
    const ui::CommandBar bar(GetScreenWidth(),GetScreenHeight());const float w=static_cast<float>(GetScreenWidth()),top=bar.Bounds().y-64;
    ui::DrawContext context{assets_.Common().uiFont,bar.Scale()};ui::Panel{{0,top,w,64},0}.Draw(context);
    const double duration=replayTimes_.empty()?0:replayTimes_.back(),current=replayTimes_.empty()?0:replayTimes_[replayCursor_];
    auto progress=context;progress.theme.panel=GRAY;ui::Panel{{12,top+5,w-24,5},0}.Draw(progress);
    progress.theme.panel=context.theme.accent;if(duration>0)ui::Panel{{12,top+5,static_cast<float>((w-24)*current/duration),5},0}.Draw(progress);
    const auto state=replaySeeking_?"定位中":replayFinished_?"播放结束":replayPaused_?"暂停":"播放";
    const auto text=std::string(state)+"  "+std::to_string(int(current))+" / "+std::to_string(int(duration))+" s  "+std::array<const char*,5>{"0.25x","0.5x","1x","2x","4x"}[replaySpeed_]+"  "+std::array<const char*,3>{"第一人称","第三人称","自由视角"}[replayView_];
    ui::Label{{12,top+15,w-24,25},20}.Draw(context,text.c_str(),context.theme.text);
    ui::Label{{12,top+40,w-24,24},16}.Draw(context,replayStatus_.c_str(),context.theme.accent);
    const auto pauseKey=KeyLabel(bindings_.bomb);
    const std::array<ui::KeyHint,6> hints{{{pauseKey.c_str(),replayPaused_?"继续回放":"暂停回放"},{"←/→ · Shift","定位 5/30秒"},{"Home / End","首尾"},{"[ / ] / 1","倍速"},{"V / R · 移动绑定","视角"},{"Esc","退出"}}};bar.Draw(context,hints);
}
}
