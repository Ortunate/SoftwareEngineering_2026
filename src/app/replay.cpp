// 本文件负责录像会话、保存封存、播放调度及本地录像列表；模拟和绘制共用正式游戏资源。
#include "app/game.hpp"
#include "app/constants.hpp"
#include "ui/input.hpp"
#include "ui/components.hpp"
#include "ui/page.hpp"
#include "ui/typography.hpp"
#include "render/menu_layout.hpp"
#include <algorithm>
#include <random>
#include <cmath>
#include <chrono>
#include <fstream>

namespace koumakyou {
namespace {
// 列表布局同时用于绘制与点击；紧凑窗口拉开行距，给双行元数据留出空间。
Rectangle ReplayRow(const menu::Canvas& canvas,int row) {
    return canvas.Rect(48,(canvas.width<1000?210:180)+row*(canvas.width<1000?70:48),canvas.width-96,canvas.width<1000?64:44);
}
std::string ReplayLabel(const std::filesystem::path& path) {
    // 浏览只读有界前缀，不把未经完整校验的元数据用于启动游戏。
    std::ifstream input(path,std::ios::binary);
    const auto read=[&](){std::uint32_t v=0;for(int i=0;i<4;++i){const int b=input.get();if(b<0)return 0xffffffffu;v|=std::uint32_t(b)<<(8*i);}return v;};
    if(read()==0x3152504b && read()==2) {
        const auto build=read();if(build<=512){input.seekg(build,std::ios::cur);const auto length=read();
            if(length<=512){std::string label(length,' ');input.read(label.data(),length);std::string normalized;
                if(input && replay::NormalizeName(label,normalized))return normalized;}
        }
    }
    const auto utf=path.filename().u8string();const std::string name(utf.begin(),utf.end());
    if(name.size()>15 && name[8]=='-')return name.substr(0,4)+"-"+name.substr(4,2)+"-"+name.substr(6,2)+"  "+name.substr(9,2)+":"+name.substr(11,2)+":"+name.substr(13,2);
    return name;
}
}

replay::Header Game::ReplayHeader() const {
    return {1,static_cast<unsigned>(selectedCharacter_*2+selectedShot_),static_cast<unsigned>(initialLives_),static_cast<unsigned>(initialBombs_),static_cast<unsigned>(practicePhase_),practiceMode_,spellPractice_,challengeMode_};
}
replay::Result Game::ReplayResult() const {
    return {static_cast<std::uint64_t>(std::max(0LL,score_)),static_cast<unsigned>(std::max(0,player_.RemainingLives())),static_cast<unsigned>(player_.Power()),static_cast<unsigned>(missCount_),static_cast<unsigned>(bombCount_),finishResult_==RunState::Won};
}
void Game::PrepareReplay() {
    if(replayMode_!=replay::Mode::Playback) {
        auto header=ReplayHeader();header.seed=std::random_device{}();replay::BeginSession(replayData_,header);
        replayData_.route=spellPractice_?"spell-practice":practiceMode_?"stage-practice":"stage1-prototype";
        replay::Segment segment;segment.seed=header.seed;segment.lives=header.lives;segment.bombs=header.bombs;segment.power=spellPractice_?128:8;segment.entry=spellPractice_?"spell":"stage";segment.target=spellPractice_?(practicePhase_==2?"rumia-night-bird":"rumia-demarcation"):"";replay::BeginStage(replayData_,segment);
        replaySaveState_=ReplaySaveState::None;
        bool eligible=captureCursor_ && !previewMode_;
#ifdef KOUMAKYOU_DEBUG_FEATURES
        eligible=eligible && !debugInvincible_ && !loadingShotTest_;
#endif
        replayMode_=eligible?replay::Mode::Recording:replay::Mode::Idle;
    }
    drops_.ResetRun(replayData_.header.seed);
}
replay::Frame Game::ReadRunInput(float dt) const {
    const auto menu=ui::MenuInput::Read();replay::Frame f;f.dt=dt;
    f.confirm=menu.confirm || menu.click;f.fast=IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL);
    f.player={.mouseDelta=GetMouseDelta(),.mouseSensitivityScale=mouseSensitivity_/100.0F,
        .forward=BindingDown(bindings_.forward),.backward=BindingDown(bindings_.backward),
        .moveLeft=BindingDown(bindings_.moveLeft),.moveRight=BindingDown(bindings_.moveRight),
        .ascend=BindingDown(bindings_.ascend),.descend=BindingDown(bindings_.descend),
        .focus=BindingDown(bindings_.focus),.fire=BindingDown(bindings_.fire),.bomb=BindingPressed(bindings_.bomb)};
    return f;
}
void Game::Update(float dt,float playbackElapsed) {
    assets_.UpdateAudio();
    if(!std::isfinite(dt) || dt<0)return;
    if(state_==RunState::MainMenu)RefreshReplayAvailability(dt);
    if(ReplaySaveModalOpen()) { replaySaveMotion_.Advance(static_cast<int>(replaySaveState_),replaySaveState_==ReplaySaveState::Ask?replayChoice_:replayNameChoice_,dt);UpdateReplaySave();return; }
    if(replayMode_==replay::Mode::Playback && state_!=RunState::Loading) {
        if(IsKeyPressed(KEY_ESCAPE)) { replayStatus_="已退出回放";OpenReplayList();return; }
        const float elapsed=std::isfinite(playbackElapsed) && playbackElapsed>=0?playbackElapsed:dt;
        if(IsKeyPressed(KEY_V)) { replayView_=(replayView_+1)%3;observer_=camera_; }
        if(IsKeyPressed(KEY_R)) { observer_=camera_;orbitYaw_=0;orbitPitch_=0.2F;orbitDistance_=3; }
        if(IsKeyPressed(KEY_SPACE) && !replayFinished_) { replayPaused_=!replayPaused_;replayBudget_=0; }
        if(IsKeyPressed(KEY_LEFT_BRACKET))replaySpeed_=std::max(0,replaySpeed_-1);
        if(IsKeyPressed(KEY_RIGHT_BRACKET))replaySpeed_=std::min(4,replaySpeed_+1);
        if(IsKeyPressed(KEY_ONE))replaySpeed_=2;
        double target=replayTimes_[replaySeeking_?replayTarget_:replayCursor_];
        const float jump=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT)?30:5;
        if(IsKeyPressed(KEY_LEFT))SeekReplay(target-jump);
        if(IsKeyPressed(KEY_RIGHT))SeekReplay(target+jump);
        if(IsKeyPressed(KEY_HOME))SeekReplay(0);
        if(IsKeyPressed(KEY_END))SeekReplay(replayTimes_.back());
        const bool muted=replaySeeking_ || replayPaused_ || replayFinished_ || replaySpeed_!=2;
        if(!muted && replayAudioMuted_)assets_.SeekMusic(static_cast<float>(replayTimes_[replayCursor_]-replayMusicSince_));
        replayAudioMuted_=muted;
        assets_.SetMusicVolumePercent(muted?0:musicVolume_);assets_.SetSoundVolumePercent(muted?0:soundVolume_);
        if(replayPaused_ || replayFinished_)assets_.PauseMusic();else assets_.ResumeMusic();
        if(!replaySeeking_)UpdateObserver(elapsed);
        if((replayPaused_ || replayFinished_) && !replaySeeking_)return;
        if(!replaySeeking_)replayBudget_+=elapsed*std::array<double,5>{0.25,0.5,1,2,4}[replaySpeed_];
        const auto began=std::chrono::steady_clock::now();
        for(int work=0;work<512 && replayCursor_<replayData_.frames.size();++work) {
            if(replaySeeking_ && replayCursor_>=replayTarget_)break;
            const auto& frame=replayData_.frames[replayCursor_];
            if(!replaySeeking_ && replayBudget_<frame.dt)break;
            runInput_=frame;if(!replaySeeking_)replayBudget_-=frame.dt;++replayCursor_;StepRun(frame.dt);
            if(assets_.MusicIdentity()!=replayMusicSlot_) {replayMusicSlot_=assets_.MusicIdentity();replayMusicSince_=replayTimes_[replayCursor_];}
            if(replayEnded_ || replayCursor_==replayData_.frames.size()) {
                const bool matches=replayEnded_ && replayCursor_==replayData_.frames.size() && ReplayResult()==replayData_.result;
                replayStatus_=matches?"回放完成 · 结果一致":"回放异常 · 终局或结果不一致";
                replayFinished_=true;replayPaused_=true;break;
            }
            if(std::chrono::steady_clock::now()-began>std::chrono::milliseconds(6))break;
        }
        if(replaySeeking_ && replayCursor_>=replayTarget_) { replaySeeking_=false;replayBudget_=0; }
        if(replayView_==0)observer_=camera_;
        return;
    }
    runInput_=ReadRunInput(dt);
    const bool active=state_==RunState::Playing || state_==RunState::StageClear;
    bool recording=active && replayMode_==replay::Mode::Recording;
#ifdef KOUMAKYOU_DEBUG_FEATURES
    if(recording && (debugInvincible_ || IsKeyPressed(KEY_F4) || IsKeyPressed(KEY_F5) || IsKeyPressed(KEY_F6))) {
        replayMode_=replay::Mode::Idle;replayData_={};replayStatus_="调试操作：本局不保存 Replay";recording=false;
    }
#endif
    if(recording && (dt<=0 || dt>0.05F || replayData_.frames.size()>=replay::kMaxFrames)) {
        replayMode_=replay::Mode::Idle;replayData_={};replayStatus_="录像超出时间或容量限制，本局未保存";recording=false;
    }
    StepRun(dt);
    // 打开暂停的步骤没有推进战斗；恢复暂停的步骤也不能当作有效输入录制。
    if(recording && state_!=RunState::Paused) {
        replayData_.frames.push_back(runInput_);
        if(replayEnded_) {
            replay::FinishSession(replayData_,ReplayResult());
            replaySaveState_=ReplaySaveState::Ready;
            replayStatus_="Replay 等待保存";
            replayMode_=replay::Mode::Idle;
        }
    }
}
void Game::RestoreReplaySettings() {
    if(!replayOriginal_)return;
    const auto h=*replayOriginal_;initialLives_=static_cast<int>(h.lives);initialBombs_=static_cast<int>(h.bombs);
    selectedCharacter_=static_cast<int>(h.shot/2);selectedShot_=static_cast<int>(h.shot%2);practicePhase_=static_cast<int>(h.phase);
    practiceMode_=h.practice;spellPractice_=h.spell;challengeMode_=h.challenge;replayOriginal_.reset();
#ifdef KOUMAKYOU_DEBUG_FEATURES
    debugInvincible_=replayOriginalDebug_;
#endif
}
void Game::PlayReplay(replay::Data data) {
    std::string error;if(!replay::CanPlay(data,error)){replayStatus_=error;return;}
    FinishSpellRecord(false);replayOriginal_=ReplayHeader();replayData_=std::move(data);
    const auto h=replayData_.header;selectedCharacter_=static_cast<int>(h.shot/2);selectedShot_=static_cast<int>(h.shot%2);
    initialLives_=static_cast<int>(h.lives);initialBombs_=static_cast<int>(h.bombs);practicePhase_=static_cast<int>(h.phase);
    practiceMode_=h.practice;spellPractice_=h.spell;challengeMode_=h.challenge;stageId_=StageId::Stage1;
    replayMode_=replay::Mode::Playback;replayCursor_=0;replayBudget_=0;replayPaused_=false;replayStatus_.clear();
#ifdef KOUMAKYOU_DEBUG_FEATURES
    replayOriginalDebug_=debugInvincible_;debugInvincible_=false;
#endif
    replayTimes_={0};for(const auto& frame:replayData_.frames)replayTimes_.push_back(replayTimes_.back()+frame.dt);
    replayAudioMuted_=true;replayMusicSlot_=0;replayMusicSince_=0;
    replaySeeking_=replayFinished_=false;replayView_=0;replaySpeed_=2;observer_=camera_;
    StartRun(false);
}
void Game::OpenReplayList() {
    assets_.SetMusicVolumePercent(musicVolume_);assets_.SetSoundVolumePercent(soundVolume_);
    EnterMainMenu();state_=RunState::ReplayList;replayFiles_.clear();selectedReplay_=0;
    try {
        if(std::filesystem::exists("replays"))for(const auto& entry:std::filesystem::directory_iterator("replays"))
            if(entry.is_regular_file() && entry.path().extension()==".krep")replayFiles_.push_back(entry.path());
        std::sort(replayFiles_.begin(),replayFiles_.end(),[](const auto& a,const auto& b){return a.filename()>b.filename();});
    } catch(const std::exception&) { replayStatus_="无法读取录像目录"; }
    replayLabels_.clear();for(const auto& file:replayFiles_)replayLabels_.push_back(ReplayLabel(file));
    RefreshReplaySelection();
}
void Game::RefreshReplaySelection() {
    replayDescription_.clear();replaySelected_.reset();
    if(replayFiles_.empty())return;
    replay::Data data;std::string error;
    if(!replay::Load(replayFiles_[selectedReplay_],data,error)) { replayDescription_=error;return; }
    double seconds=0;for(const auto& frame:data.frames)seconds+=frame.dt;
    const auto& h=data.header;
    replayDescription_=data.name+"\n"+std::string(spell::Characters()[h.shot].displayName)+" · "+(h.spell?(h.phase==2?"夜符练习":"暗符练习"):h.practice?"一面练习":"一面")+
        (h.challenge?" · 挑战":" · 标准")+"\n"+std::to_string(static_cast<int>(seconds))+" 秒 · "+std::to_string(data.result.score)+(data.result.won?" · 通关":" · 失败");
    if(!replay::CanPlay(data,error))replayDescription_+="\n"+error;
    replaySelected_=std::move(data);
}
void Game::UpdateReplayList() {
    const auto input=ui::MenuInput::Read();if(input.back){replayStatus_.clear();EnterMainMenu();return;}
    if(replayFiles_.empty())return;
    const int count=static_cast<int>(replayFiles_.size()),previous=selectedReplay_;
    const auto canvas=menu::PageCanvas(GetScreenWidth(),GetScreenHeight());const int start=(selectedReplay_/8)*8;
    for(int direction:{-1,1}) {
        const Rectangle bounds=canvas.Rect(direction<0?48:canvas.width-190,canvas.width<1000?1010:690,142,44);
        if(ui::Button{bounds,direction<0?start>0:start+8<count}.Update(input).activated) {
            selectedReplay_=std::clamp(start+direction*8,0,count-1);RefreshReplaySelection();assets_.PlaySoundEffect("menu_move");return;
        }
    }
    std::vector<ui::Item> items;items.reserve(count);
    for(int i=0;i<count;++i)items.push_back({i,i>=start && i<start+8?ReplayRow(canvas,i-start):Rectangle{-100,-100,0,0}});
    const auto result=ui::UpdateSelection(items,input,selectedReplay_,ui::Navigation::Vertical);
    if(previous!=selectedReplay_) {RefreshReplaySelection();assets_.PlaySoundEffect("menu_move");}
    if(result.confirmed) {assets_.PlaySoundEffect("menu_confirm");if(replaySelected_)PlayReplay(*replaySelected_);else replayStatus_=replayDescription_;}
}
void Game::DrawReplayList() const {
    const auto canvas=menu::PageCanvas(GetScreenWidth(),GetScreenHeight());const auto font=assets_.Common().uiFont;
    const ui::DrawContext context{font,canvas.scale,1,menuMotion_.Phase(),menuMotion_.Pulse()};
    ui::DrawPageSurface(canvas,menuMotion_);ui::DrawPageHeader(canvas,font,"录像回放","选择录像 · 保存名称与日期见详情","");
    const auto label=[&](const std::string& value,float y,Color color,float size=24){ui::Label{canvas.Rect(48,y,canvas.width-96,34),size}.Draw(context,value.c_str(),color);};
    if(replayFiles_.empty())label("暂无录像",185,context.theme.text);
    const int start=(selectedReplay_/8)*8,count=static_cast<int>(replayFiles_.size());
    for(int i=start;i<std::min(start+8,count);++i) {
        const auto row=ReplayRow(canvas,i-start);const auto color=ui::SelectableRow{row,i}.Draw(context,i==selectedReplay_?1.0F:0.0F);
        ui::Label{{row.x+12*canvas.scale,row.y,row.width-24*canvas.scale,row.height},28}.Draw(context,replayLabels_[i].c_str(),color);
    }
    const bool compact=canvas.width<1000;std::size_t offset=0;int line=0;
    do {const auto end=replayDescription_.find('\n',offset);label(replayDescription_.substr(offset,end==std::string::npos?end:end-offset),(compact?810:575)+line*30.0F,context.theme.accent,20);if(end==std::string::npos)break;offset=end+1;++line;}while(line<4);
    label(replayStatus_,compact?950:655,context.theme.text,18);
    for(int direction:{-1,1})ui::Button{canvas.Rect(direction<0?48:canvas.width-190,canvas.width<1000?1010:690,142,44),direction<0?start>0:start+8<count}.Draw(context,direction<0?"上一页":"下一页");
    ui::Label{canvas.Rect(canvas.width/2-80,compact?1010:690,160,44),22,ui::TextAlign::Center}.Draw(context,(std::to_string(count?selectedReplay_+1:0)+" / "+std::to_string(count)).c_str(),context.theme.text);
}
}

namespace koumakyou {
bool Game::OpenReplayFile(const std::filesystem::path& path,int view) {
    replay::Data data;std::string error;
    if(!replay::Load(path,data,error) || !replay::CanPlay(data,error)){replayStatus_=error;return false;}
    PlayReplay(std::move(data));replayView_=std::clamp(view,0,2);return true;
}
}
