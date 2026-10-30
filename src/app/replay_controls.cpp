// 本文件负责录像保存弹窗、观战相机及定位操作；观察输入不进入模拟输入流。
#include "app/game.hpp"
#include "platform/window_input.hpp"
#include "ui/input.hpp"
#include "ui/typography.hpp"
#include "raymath.h"
#include <algorithm>
#include <chrono>

namespace koumakyou {
void Game::RequestReplayAction(int action) {
    replayAfterSave_=action;
    if(!replayData_.frames.empty() && replayEnded_ && !replaySaved_ && (!replayAsked_ || action==4)) {
        replaySavePage_=1;replayChoice_=0;replayName_=replayData_.name=="Replay"?std::string(spell::Characters()[selectedCharacter_*2+selectedShot_].displayName)+(spellPractice_?" Spell ":practiceMode_?" Practice ":" ")+std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())):replayData_.name;
        return;
    }
    FinishReplayAction();
}
void Game::FinishReplayAction() {
    replaySavePage_=0;(void)DisableWindowInputMethods(GetWindowHandle());
    switch(replayAfterSave_) { case 0:StartRun();break;case 1:EnterMainMenu();break;case 2:ReturnToPracticeStage();break;case 3:ReturnToSpellPractice();break;default:break; }
}
void Game::UpdateReplaySave() {
    const auto input=ui::MenuInput::Read();
    if(replaySavePage_==1) {
        if(input.Direction())replayChoice_=1-replayChoice_;
        if(input.back || (input.confirm && replayChoice_==1)) {replayAsked_=true;FinishReplayAction();return;}
        if(input.confirm) {replaySavePage_=2;(void)EnableWindowTextInput(GetWindowHandle());while(GetCharPressed()){};}
        return;
    }
    if(IsKeyPressed(KEY_ESCAPE)) {replaySavePage_=1;(void)DisableWindowInputMethods(GetWindowHandle());return;}
    if(IsKeyPressed(KEY_BACKSPACE) && !replayName_.empty()) {
        do {auto b=static_cast<unsigned char>(replayName_.back());replayName_.pop_back();if((b&192)!=128)break;}while(!replayName_.empty());
    }
    for(int code=GetCharPressed();code;code=GetCharPressed()) {
        int bytes=0;const char* utf=CodepointToUTF8(code,&bytes);std::string normalized;
        if(replay::NormalizeName(replayName_+std::string(utf,bytes),normalized))replayName_+=std::string(utf,bytes);
    }
    // 屏幕字符表始终可用，平台文字合成不可用时也能完成命名。
    constexpr const char* characters="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
    if(IsKeyPressed(KEY_TAB))replayChar_=(replayChar_+1)%38;
    if(IsKeyPressed(KEY_INSERT)) {std::string normalized;if(replay::NormalizeName(replayName_+characters[replayChar_],normalized))replayName_+=characters[replayChar_];}
    if(IsKeyPressed(KEY_ENTER)) {
        std::string normalized;if(!replay::NormalizeName(replayName_,normalized)){replayStatus_="请输入 1 至 32 个字符";return;}
        replayData_.name=normalized;std::filesystem::path saved;std::string error;
        if(!replay::Save("replays",replayData_,saved,error)){replayStatus_="保存失败："+error;return;}
        replaySaved_=replayAsked_=true;replayStatus_="Replay 已保存";FinishReplayAction();
    }
}
void Game::DrawReplaySave() const {
    const auto font=assets_.Common().uiFont;const float w=static_cast<float>(GetScreenWidth()),h=static_cast<float>(GetScreenHeight());
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Color{0,0,0,220});
    const auto line=[&](const std::string& text,float y,Color color){ui::DrawCentered(font,text.c_str(),w/2,y,ui::FittedSize(font,text.c_str(),26,w-48),color);};
    line(replaySavePage_==1?"保存 Replay？":"录像名称",h*0.30F,GOLD);
    if(replaySavePage_==1) {line(replayChoice_==0?"> 是        否":"是        > 否",h*0.45F,WHITE);line("方向键选择 · 确认 · Esc 放弃",h*0.62F,WHITE);}
    else {line(replayName_+"_",h*0.42F,WHITE);line("输入名称 · Backspace 删除 · Enter 保存 · Esc 返回",h*0.56F,WHITE);line(std::string("Tab 选字符 / Insert 插入：")+"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"[replayChar_],h*0.65F,GOLD);}
    line(replayStatus_,h*0.76F,WHITE);
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
    const float w=static_cast<float>(GetScreenWidth()),h=static_cast<float>(GetScreenHeight());const auto font=assets_.Common().uiFont;
    const float top=h-100;DrawRectangleRec({0,top,w,100},Color{15,12,22,245});
    const double duration=replayTimes_.empty()?0:replayTimes_.back(),current=replayTimes_.empty()?0:replayTimes_[replayCursor_];
    DrawRectangleRec({12,top+8,w-24,5},GRAY);if(duration>0)DrawRectangleRec({12,top+8,static_cast<float>((w-24)*current/duration),5},GOLD);
    const auto text=[&](const std::string& t,float y){ui::DrawText(font,t.c_str(),{12,y},ui::FittedSize(font,t.c_str(),20,w-24),WHITE);};
    const auto state=replaySeeking_?"定位中":replayFinished_?"播放结束":replayPaused_?"暂停":"播放";
    text(std::string(state)+"  "+std::to_string(int(current))+" / "+std::to_string(int(duration))+" s  "+std::array<const char*,5>{"0.25x","0.5x","1x","2x","4x"}[replaySpeed_]+"  "+std::array<const char*,3>{"第一人称","第三人称","自由视角"}[replayView_],top+20);
    text("Space 暂停 · ←/→ 5秒 · Shift 30秒 · Home/End · [/] 倍速 · 1 复位",top+47);
    text(replayStatus_.empty()?"V 视角 · R 重置 · 自由：WASD / Q E / Shift · Esc 退出":replayStatus_,top+73);
}
}
