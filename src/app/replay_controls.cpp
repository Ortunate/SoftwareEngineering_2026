// 本文件负责录像保存弹窗、观战相机及定位操作；观察输入不进入模拟输入流。
#include "app/game.hpp"
#include "platform/window_input.hpp"
#include "ui/input.hpp"
#include "ui/components.hpp"
#include "ui/command_bar.hpp"
#include "render/menu_layout.hpp"
#include "ui/typography.hpp"
#include "raymath.h"
#include <algorithm>
#include <chrono>

namespace koumakyou {
namespace {
constexpr const char* kNameCharacters="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
// 同一布局定义用于输入和绘制；命名不增加通用控件，由现有按钮和标签组合。
struct SaveLayout {
    menu::Canvas canvas=menu::PageCanvas(GetScreenWidth(),GetScreenHeight());
    Rectangle Ask(int index) const { return canvas.Rect(canvas.width/2-220+index*240,350,200,48); }
    Rectangle Character(int index) const { return canvas.Rect(canvas.width/2-250+(index%10)*50,300+(index/10)*48,44,42); }
    Rectangle Action(int index) const { return canvas.Rect(canvas.width/2-250+index*170,525,155,48); }
};
std::vector<ui::Item> NameItems(const SaveLayout& layout) {
    std::vector<ui::Item> items;for(int i=0;i<38;++i)items.push_back({i,layout.Character(i)});
    for(int i=0;i<3;++i)items.push_back({38+i,layout.Action(i)});
    return items;
}
}
bool Game::CanSaveReplay() const {
    return replayMode_!=replay::Mode::Playback && replayEnded_ && !replayData_.frames.empty() && replayData_.continues==0 &&
        replaySaveState_!=ReplaySaveState::None && replaySaveState_!=ReplaySaveState::Saved;
}
bool Game::ReplaySaveModalOpen() const {
    return replaySaveState_==ReplaySaveState::Ask || replaySaveState_==ReplaySaveState::Naming || replaySaveState_==ReplaySaveState::Failed;
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
bool Game::MainMenuItemAvailable(int index) const { return index!=1 && (index!=4 || replayAvailable_); }
void Game::RequestReplayAction(ReplayAction action) {
    replayAfterSave_=action;
    if(CanSaveReplay() && (replaySaveState_!=ReplaySaveState::Declined || action==ReplayAction::Stay)) {
        replaySaveState_=ReplaySaveState::Ask;replayChoice_=0;replayNameChoice_=38;replaySaveMotion_={};
        if(replayName_.empty())replayName_=replayData_.name=="Replay"?std::string(spell::Characters()[selectedCharacter_*2+selectedShot_].displayName)+" "+std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())):replayData_.name;
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
        if(result.confirmed) {replaySaveState_=ReplaySaveState::Naming;(void)EnableWindowTextInput(GetWindowHandle());while(GetCharPressed()){};}
        return;
    }
    // 字母和空格只作为名称输入，不能触发菜单的 Z/X/WASD 别名。
    input.confirm=IsKeyPressed(KEY_ENTER);input.back=IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    input.vertical=IsKeyPressed(KEY_DOWN)-IsKeyPressed(KEY_UP);input.horizontal=IsKeyPressed(KEY_RIGHT)-IsKeyPressed(KEY_LEFT);
    const auto erase=[&](){if(replayName_.empty())return;do{auto byte=static_cast<unsigned char>(replayName_.back());replayName_.pop_back();if((byte&192)!=128)break;}while(!replayName_.empty());};
    if(IsKeyPressed(KEY_BACKSPACE))erase();
    for(int code=GetCharPressed();code;code=GetCharPressed()) {
        int bytes=0;const char* utf=CodepointToUTF8(code,&bytes);std::string normalized;
        if(replay::NormalizeName(replayName_+std::string(utf,bytes),normalized))replayName_+=std::string(utf,bytes);
    }
    if(IsKeyPressed(KEY_TAB))replayNameChoice_=(replayNameChoice_+1)%41;
    const auto items=NameItems(layout);const auto result=ui::UpdateSelection(items,input,replayNameChoice_);sound(result);
    if(result.back || (result.confirmed && replayNameChoice_==39)) {replaySaveState_=ReplaySaveState::Ask;(void)DisableWindowInputMethods(GetWindowHandle());return;}
    if(!result.confirmed)return;
    if(replayNameChoice_<38) {std::string normalized;if(replay::NormalizeName(replayName_+kNameCharacters[replayNameChoice_],normalized))replayName_+=kNameCharacters[replayNameChoice_];return;}
    if(replayNameChoice_==40){erase();return;}
    std::string normalized;if(!replay::NormalizeName(replayName_,normalized)){replayStatus_="请输入 1 至 32 个字符";replaySaveState_=ReplaySaveState::Failed;return;}
    replayData_.name=normalized;std::filesystem::path saved;std::string error;
    if(!replay::Save("replays",replayData_,saved,error)){replayStatus_="保存失败："+error;replaySaveState_=ReplaySaveState::Failed;return;}
    replaySaveState_=ReplaySaveState::Saved;replayStatus_="Replay 已保存";RefreshReplayAvailability(0,true);FinishReplayAction();
}
void Game::DrawReplaySave() const {
    const SaveLayout layout;const auto& canvas=layout.canvas;
    const ui::DrawContext context{assets_.Common().uiFont,canvas.scale,1,replaySaveMotion_.Phase(),replaySaveMotion_.Pulse()};
    const bool ask=replaySaveState_==ReplaySaveState::Ask;
    ui::Modal{{0,0,static_cast<float>(GetScreenWidth()),static_cast<float>(GetScreenHeight())},canvas.Rect(24,130,canvas.width-48,ask?350:530),canvas.Point(canvas.width/2,155)}.Draw(context,ask?"保存 Replay？":"录像名称",context.theme.accent);
    if(ask) {
        for(int i=0;i<2;++i)ui::Button{layout.Ask(i)}.Draw(context,i==0?"是":"否",{replayChoice_==i?1.0F:0.0F,1});
    } else {
        ui::Panel{canvas.Rect(48,220,canvas.width-96,56)}.Draw(context);
        ui::Label{canvas.Rect(60,220,canvas.width-120,56),26}.Draw(context,(replayName_+"_").c_str(),context.theme.text);
        for(int i=0;i<38;++i){const char label[]{kNameCharacters[i],0};ui::Button{layout.Character(i)}.Draw(context,label,{replayNameChoice_==i?1.0F:0.0F,1});}
        for(int i=0;i<3;++i)ui::Button{layout.Action(i)}.Draw(context,std::array<const char*,3>{"保存 Replay","返回","删除"}[i],{replayNameChoice_==38+i?1.0F:0.0F,1});
        ui::Label{canvas.Rect(48,590,canvas.width-96,44),20}.Draw(context,replayStatus_.c_str(),context.theme.accent);
    }
    const ui::CommandBar bar(GetScreenWidth(),GetScreenHeight());
    const std::array<ui::KeyHint,3> hints{{{ask?"方向键 / 鼠标":"键盘 / 字符按钮",ask?"选择":"输入名称"},{ask?"Z / Enter / 左键":"Enter / 左键",ask?"确认":"激活按钮"},{"Esc / 右键","返回"}}};
    bar.Draw(context,hints);
}
void Game::SeekReplay(double seconds) {
    const auto target=static_cast<std::size_t>(std::upper_bound(replayTimes_.begin(),replayTimes_.end(),std::clamp(seconds,0.0,replayTimes_.back()))-replayTimes_.begin()-1);
    if(target==replayCursor_) {replaySeeking_=false;return;}
    if(target<replayCursor_) {
        assets_.SetMusicVolumePercent(0);assets_.SetSoundVolumePercent(0);replayAudioMuted_=true;
        // 当前仅有一面；未来解析片段后使用目标关入口资源，仍走同一初始化和输入推进路径。
        StartRun(false,true);replayCursor_=0;replayMusicSince_=0;replayMusicSlot_=assets_.MusicIdentity();
    }
    replayTarget_=target;replaySeeking_=true;replayFinished_=false;replayEnded_=false;replayBudget_=0;replayStatus_.clear();
}
void Game::UpdateObserver(float seconds) {
    if(replayView_==0){observer_=camera_;return;}
    if(!IsWindowHidden() && !IsCursorHidden())DisableCursor();
    const auto mouse=GetMouseDelta();
    orbitYaw_-=mouse.x*0.003F;orbitPitch_=std::clamp(orbitPitch_-mouse.y*0.003F,-1.45F,1.45F);
    const auto aim=Vector3Normalize(Vector3Subtract(camera_.target,camera_.position));
    Vector3 direction{std::sin(orbitYaw_)*std::cos(orbitPitch_),std::sin(orbitPitch_),std::cos(orbitYaw_)*std::cos(orbitPitch_)};
    observer_.up={0,1,0};observer_.fovy=72;observer_.projection=CAMERA_PERSPECTIVE;
    if(replayView_==1) {
        orbitDistance_=std::clamp(orbitDistance_-GetMouseWheelMove()*0.3F,1.0F,12.0F);
        const float yaw=std::atan2(aim.x,aim.z)+orbitYaw_;
        direction={std::sin(yaw)*std::cos(orbitPitch_),-std::sin(orbitPitch_),std::cos(yaw)*std::cos(orbitPitch_)};
        observer_.position=Vector3Add(Vector3Subtract(player_.Position(),Vector3Scale(direction,orbitDistance_)),{0,1,0});observer_.target=Vector3Add(player_.Position(),aim);
    } else {
        const auto right=Vector3Normalize(Vector3CrossProduct({0,1,0},direction));Vector3 move{};
        move=Vector3Add(move,Vector3Scale(direction,float(IsKeyDown(KEY_W)-IsKeyDown(KEY_S))));
        move=Vector3Add(move,Vector3Scale(right,float(IsKeyDown(KEY_D)-IsKeyDown(KEY_A))));move.y+=IsKeyDown(KEY_E)-IsKeyDown(KEY_Q);
        observer_.position=Vector3Add(observer_.position,Vector3Scale(move,std::min(seconds,0.1F)*(IsKeyDown(KEY_LEFT_SHIFT)?12:4)));
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
    const std::array<ui::KeyHint,6> hints{{{"Space","暂停"},{"←/→ · Shift","定位 5/30秒"},{"Home / End","首尾"},{"[ / ] / 1","倍速"},{"V / R · WASD / QE","视角"},{"Esc","退出"}}};bar.Draw(context,hints);
}
}
