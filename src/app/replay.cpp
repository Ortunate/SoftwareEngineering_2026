// 本文件负责录像会话、保存封存、播放调度及本地录像列表；模拟和绘制共用正式游戏资源。
#include "app/game.hpp"
#include "app/constants.hpp"
#include "ui/input.hpp"
#include "ui/components.hpp"
#include "ui/page.hpp"
#include "ui/typography.hpp"
#include "render/menu_layout.hpp"
#include "render/replay_layout.hpp"
#include <algorithm>
#include <random>
#include <cmath>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace koumakyou {
namespace {
// 展示只读取录像快照；目录无法识别的 ID 保留原文，不借用当前菜单选择补全。
std::string RecordedStage(const std::string& id) {
    for(int i=0;i<6;++i)if(id=="stage"+std::to_string(i+1))return spell::Stages()[i].label;
    return id=="extra"?"Extra":id;
}

}

replay::Header Game::ReplayHeader() const {
    return {1,static_cast<unsigned>(selectedCharacter_*2+selectedShot_),static_cast<unsigned>(initialLives_),static_cast<unsigned>(initialBombs_),static_cast<unsigned>(spellPractice_?practiceAttack_:2),practiceMode_,spellPractice_,challengeMode_};
}
replay::Result Game::ReplayResult() const {
    return {static_cast<std::uint64_t>(std::max(0LL,score_)),static_cast<unsigned>(std::max(0,player_.RemainingLives())),static_cast<unsigned>(player_.Power()),static_cast<unsigned>(missCount_),static_cast<unsigned>(bombCount_),(state_==RunState::StageClear || state_==RunState::Won || (state_==RunState::Finishing && finishResult_==RunState::Won))};
}
void Game::PrepareReplay() {
    if(replayMode_!=replay::Mode::Playback) {
        if(!continuingStage_) {
            auto header=ReplayHeader();header.seed=std::random_device{}();replay::BeginSession(replayData_,header);
            replayData_.route=spellPractice_?"spell-practice":practiceMode_?"stage-practice":stageId_==StageId::Extra?"extra":"campaign";
            replaySaveState_=ReplaySaveState::None;
            bool eligible=captureCursor_ && !previewMode_;
#ifdef KOUMAKYOU_DEBUG_FEATURES
            eligible=eligible && !debugInvincible_ && !loadingShotTest_;
#endif
            replayMode_=eligible?replay::Mode::Recording:replay::Mode::Idle;
        }
        // 入口快照在库存/rank初始化后采样；符卡开局火力在Boss定位后补满，快照仍描述战斗入口。
        replay::Segment segment;
        segment.stage=stageId_==StageId::Extra?"extra":"stage"+std::to_string(StageNumber(stageId_));
        segment.difficulty=stageId_==StageId::Extra?replay::Difficulty::Extra:static_cast<replay::Difficulty>(native_combat::DifficultyIndex(runDifficulty_));
        segment.seed=replayData_.header.seed;segment.lives=std::max(0,player_.RemainingLives()-1);segment.bombs=player_.Bombs();
        segment.power=spellPractice_?128:player_.Power();segment.rank=referenceRank_;segment.score=score_;
        segment.entry=spellPractice_?"spell":"stage";
        if(spellPractice_)segment.target="spell-"+std::to_string(spell::Find(spellSelection_.stage,spellSelection_.group,spellSelection_.difficulty)->number);
        // 未录制的预览跨关不需要保留片段生命周期。
        if(replayMode_==replay::Mode::Idle && continuingStage_)replayData_.segments.clear();
        replay::BeginStage(replayData_,segment);
    }
    if(replayMode_==replay::Mode::Playback) {
        const auto& segment=replayData_.segments[replaySegment_];
        if(segment.stage!=(stageId_==StageId::Extra?"extra":"stage"+std::to_string(StageNumber(stageId_))) ||
           segment.lives!=static_cast<unsigned>(std::max(0,player_.RemainingLives()-1)) || segment.bombs!=static_cast<unsigned>(player_.Bombs()) ||
           segment.power!=static_cast<unsigned>(spellPractice_?128:player_.Power()) || segment.score!=static_cast<std::uint64_t>(score_) ||
           (segment.rank!=static_cast<unsigned>(referenceRank_) && !(replaySegment_==0 && segment.rank==16))) {
            replayStatus_="回放异常 · 关卡入口资源不一致";replayFinished_=replayPaused_=true;
        }
    }
    // 每关重置掉落随机流，首种子由会话固定；关卡脚本自己的固定种子仍由内容管理。
    drops_.ResetRun(replayData_.header.seed);
}
replay::Frame Game::ReadRunInput(float dt) const {
    const auto menu=ui::MenuInput::Read();replay::Frame f;f.dt=dt;
    f.confirm=menu.confirm || menu.click;f.fast=IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL);
    f.player={.mouseDelta=GetMouseDelta(),.mouseSensitivityScale=mouseSensitivity_/100.0F,
        .forward=BindingDown(bindings_.forward),.backward=BindingDown(bindings_.backward),
        .moveLeft=BindingDown(bindings_.moveLeft),.moveRight=BindingDown(bindings_.moveRight),
        .ascend=BindingDown(bindings_.ascend),.descend=BindingDown(bindings_.descend),
        .focus=BindingDown(bindings_.focus),.fire=BindingDown(bindings_.fire),.bomb=BindingPressed(bindings_.bomb),.aimAssistEnabled=aimAssistEnabled_};
    return f;
}
void Game::Update(float dt,float playbackElapsed) {
    assets_.UpdateAudio();
    if(!std::isfinite(dt) || dt<0)return;
    if(state_==RunState::MainMenu)RefreshReplayAvailability(dt);
    if(ReplaySaveModalOpen()) { replaySaveMotion_.Advance(static_cast<int>(replaySaveState_),(replaySaveState_==ReplaySaveState::Ask || replaySaveState_==ReplaySaveState::Overwrite)?replayChoice_:-1,dt);UpdateReplaySave();return; }
    if(replayMode_==replay::Mode::Playback && state_!=RunState::Loading) {
        if(IsKeyPressed(KEY_ESCAPE)) { replayStatus_="已退出回放";OpenReplayList();return; }
        const float elapsed=std::isfinite(playbackElapsed) && playbackElapsed>=0?playbackElapsed:dt;
        // 回放暂停沿用本机 Bomb 绑定，键盘和鼠标统一按首次按下触发；同帧优先于其他观战快捷键。
        const bool pausePressed=BindingPressed(bindings_.bomb);
        if(pausePressed && !replayFinished_) { replayPaused_=!replayPaused_;replayBudget_=0; }
        if(!pausePressed) {
            if(IsKeyPressed(KEY_V)) { replayView_=(replayView_+1)%3;observer_=camera_; }
            if(IsKeyPressed(KEY_R)) { observer_=camera_;orbitYaw_=0;orbitPitch_=0.2F;orbitDistance_=3; }
            if(IsKeyPressed(KEY_LEFT_BRACKET))replaySpeed_=std::max(0,replaySpeed_-1);
            if(IsKeyPressed(KEY_RIGHT_BRACKET))replaySpeed_=std::min(4,replaySpeed_+1);
            if(IsKeyPressed(KEY_ONE))replaySpeed_=2;
            double target=replayTimes_[replaySeeking_?replayTarget_:replayCursor_];
            const float jump=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT)?30:5;
            if(IsKeyPressed(KEY_LEFT))SeekReplay(target-jump);
            if(IsKeyPressed(KEY_RIGHT))SeekReplay(target+jump);
            if(IsKeyPressed(KEY_HOME))SeekReplay(0);
            if(IsKeyPressed(KEY_END))SeekReplay(replayTimes_.back());
        }
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
            const auto& segment=replayData_.segments[replaySegment_];
            if(replayCursor_==segment.end) {
                auto result=ReplayResult();
                if(state_==RunState::Loading && continuingStage_)result.won=true;
                const bool last=replaySegment_+1==replayData_.segments.size();
                if(result!=segment.result || (!last && (state_!=RunState::Loading || !continuingStage_))) {
                    replayStatus_="回放异常 · 关卡边界不一致";replayFinished_=replayPaused_=true;break;
                }
                if(!last)++replaySegment_;
            }
            if(assets_.MusicIdentity()!=replayMusicSlot_) {replayMusicSlot_=assets_.MusicIdentity();replayMusicSince_=replayTimes_[replayCursor_];}
            if(replayEnded_ || replayCursor_==replayData_.frames.size()) {
                const bool matches=replayEnded_ && replayCursor_==replayData_.frames.size() && ReplayResult()==replayData_.result;
                replayStatus_=matches?"回放完成 · 结果一致":"回放异常 · 终局或结果不一致";
                replayFinished_=true;replayPaused_=true;break;
            }
            if(state_==RunState::Loading)break;
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
        if(state_==RunState::Loading && continuingStage_) {
            auto result=ReplayResult();result.won=true;replay::EndStage(replayData_,result);
        }
        if(replayEnded_) {
            replay::FinishSession(replayData_,ReplayResult());
            replaySaveState_=ReplaySaveState::Ready;
            replayStatus_="Replay 等待保存";
            replayMode_=replay::Mode::Idle;
            // 本篇先完成成绩登记，再在离开成绩页时询问Demo；练习仍在独立结果页选存。
        }
    }
}
void Game::RestoreReplaySettings() {
    if(!replayOriginal_)return;
    const auto h=*replayOriginal_;initialLives_=static_cast<int>(h.lives);initialBombs_=static_cast<int>(h.bombs);
    selectedCharacter_=static_cast<int>(h.shot/2);selectedShot_=static_cast<int>(h.shot%2);practiceAttack_=static_cast<int>(h.phase);
    practiceMode_=h.practice;spellPractice_=h.spell;challengeMode_=h.challenge;
    stageId_=replayOriginalStage_;selectedDifficulty_=replayOriginalDifficulty_;spellSelection_=replayOriginalSpell_;replayOriginal_.reset();
#ifdef KOUMAKYOU_DEBUG_FEATURES
    debugInvincible_=replayOriginalDebug_;
#endif
}
void Game::RestoreReplayEntry() {
    replay::Entry entry;
    if(!replay::ResolveEntry(replayData_.segments.front(),entry))throw std::logic_error("Invalid replay entry");
    stageId_=*SpellPracticeStage(entry.stage);
    selectedDifficulty_=entry.difficulty==4?1:entry.difficulty;
    spellSelection_={.stage=entry.stage,.group=entry.group,.difficulty=entry.difficulty,.character=static_cast<int>(replayData_.header.shot)};
    practiceAttack_=entry.phase;
}
void Game::PlayReplay(replay::Data data) {
    std::string error;if(!replay::CanPlay(data,error)){replayStatus_=error;return;}
    FinishSpellRecord(false);replayOriginal_=ReplayHeader();
    replayOriginalStage_=stageId_;replayOriginalDifficulty_=selectedDifficulty_;replayOriginalSpell_=spellSelection_;replayData_=std::move(data);
    const auto h=replayData_.header;selectedCharacter_=static_cast<int>(h.shot/2);selectedShot_=static_cast<int>(h.shot%2);
    initialLives_=static_cast<int>(h.lives);initialBombs_=static_cast<int>(h.bombs);practiceAttack_=static_cast<int>(h.phase);
    practiceMode_=h.practice;spellPractice_=h.spell;challengeMode_=h.challenge;
    RestoreReplayEntry();
    replayMode_=replay::Mode::Playback;replaySegment_=0;replayCursor_=0;replayBudget_=0;replayPaused_=false;replayStatus_.clear();
#ifdef KOUMAKYOU_DEBUG_FEATURES
    replayOriginalDebug_=debugInvincible_;debugInvincible_=false;
#endif
    replayTimes_={0};for(const auto& frame:replayData_.frames)replayTimes_.push_back(replayTimes_.back()+frame.dt);
    replayAudioMuted_=true;replayMusicSlot_=0;replayMusicSince_=0;
    replaySeeking_=replayFinished_=false;replayView_=0;replaySpeed_=2;observer_=camera_;
    StartRun(false);
}
void Game::ReadReplayDirectory() {
    replayFiles_.clear();replayRows_.clear();replayRowErrors_.clear();replayOccupied_.clear();
    std::vector<std::filesystem::path> legacy;
    for(int slot=1;slot<=replay::kSlotCount;++slot)replayFiles_.push_back(replay::SlotPath("replays",slot));
    std::error_code ec;
    if(std::filesystem::exists("replays",ec)) {
        for(std::filesystem::directory_iterator it("replays",ec),end;!ec && it!=end;it.increment(ec)) {
            if(it->path().extension()==".krep" && it->is_regular_file(ec) &&
               std::find(replayFiles_.begin(),replayFiles_.end(),it->path())==replayFiles_.end())legacy.push_back(it->path());
        }
    }
    if(ec)replayStatus_="无法读取录像目录";
    std::sort(legacy.begin(),legacy.end());replayFiles_.insert(replayFiles_.end(),legacy.begin(),legacy.end());
    for(const auto& path:replayFiles_) {
        std::string error;replay::Summary summary;std::error_code existsError;
        const bool occupied=std::filesystem::exists(path,existsError);
        replayOccupied_.push_back(occupied || bool(existsError));
        if(occupied && replay::ReadSummary(path,summary,error))replayRows_.push_back(std::move(summary));
        else replayRows_.push_back(std::nullopt);
        replayRowErrors_.push_back(existsError?"无法读取录像槽":error);
    }
}
void Game::OpenReplayList() {
    assets_.SetMusicVolumePercent(musicVolume_);assets_.SetSoundVolumePercent(soundVolume_);
    EnterMainMenu();state_=RunState::ReplayList;selectedReplay_=0;ReadReplayDirectory();RefreshReplaySelection();
}
void Game::RefreshReplaySelection() {
    if(selectedReplay_<0 || selectedReplay_>=static_cast<int>(replayRows_.size()))return;
    const auto& summary=replayRows_[selectedReplay_];
    if(summary && (summary->simulation!=replay::SimulationId() || summary->content!=replay::ContentId()))
        replayRowErrors_[selectedReplay_]="模拟或内容版本不兼容";
}
bool Game::UpdateReplayTable(const ui::MenuInput& input,bool saving) {
    const menu::ReplayLayout layout(GetScreenWidth(),GetScreenHeight());
    const int count=saving?replay::kSlotCount:static_cast<int>(replayFiles_.size()),pageSize=layout.PageSize();
    const int previous=selectedReplay_,first=selectedReplay_/pageSize*pageSize;
    for(int direction:{-1,1}) {
        const bool enabled=direction<0?first>0:first+pageSize<count;
        if((enabled && input.horizontal==direction) || ui::ArrowButton{layout.Arrow(direction),direction,enabled}.Update(input).activated) {
            selectedReplay_=std::clamp(first+direction*pageSize,0,count-1);
            replayStatus_.clear();if(!saving)RefreshReplaySelection();assets_.PlaySoundEffect("menu_move");return false;
        }
    }
    if(input.horizontal)return false;
    std::vector<ui::Item> items;items.reserve(count);
    for(int i=0;i<count;++i)items.push_back({i,i>=first && i<first+pageSize?layout.Row(i-first):Rectangle{-100,-100,0,0}});
    auto navigation=input;const float wheel=GetMouseWheelMove();if(wheel!=0)navigation.vertical=wheel>0?-1:1;
    const auto result=ui::UpdateSelection(items,navigation,selectedReplay_,ui::Navigation::Vertical);
    if(previous!=selectedReplay_){replayStatus_.clear();if(!saving)RefreshReplaySelection();assets_.PlaySoundEffect("menu_move");}
    return result.confirmed;
}
void Game::UpdateReplayList() {
    const auto input=ui::MenuInput::Read();if(input.back){replayStatus_.clear();EnterMainMenu();return;}
    if(!UpdateReplayTable(input,false))return;
    if(!replayOccupied_[selectedReplay_]){replayStatus_="空槽";assets_.PlaySoundEffect("menu_cancel");return;}
    replay::Data data;std::string error;
    // 选槽只浏览缓存；确认播放时才读取全部输入并核验，头部成功不代表负载或入口有效。
    if(!replay::Load(replayFiles_[selectedReplay_],data,error) || !replay::CanPlay(data,error)) {
        replayStatus_=replayRowErrors_[selectedReplay_]=error;assets_.PlaySoundEffect("menu_cancel");return;
    }
    assets_.PlaySoundEffect("menu_confirm");PlayReplay(std::move(data));
}
void Game::DrawReplayTable(bool saving,float opacity,const ui::MenuMotion* motion) const {
    const auto& animation=motion?*motion:menuMotion_;
    const menu::ReplayLayout layout(GetScreenWidth(),GetScreenHeight());const auto& c=layout.canvas;
    const ui::DrawContext paint{assets_.Common().uiFont,c.scale,opacity,animation.Phase(),animation.Pulse()};
    ui::DrawPageSurface(c,animation,opacity);
    ui::DrawPageHeader(c,paint.font,saving?"保存 Replay":"REPLAY",saving?"选择录像槽位 · 已有录像需确认覆盖":"选择录像 · 方向键切换槽位与页面","",opacity);
    const int count=saving?replay::kSlotCount:static_cast<int>(replayFiles_.size()),pageSize=layout.PageSize(),first=selectedReplay_/pageSize*pageSize;
    const auto cell=[&](Rectangle row,float left,float width,const std::string& text,Color color,float size=19) {
        ui::Label{{row.x+row.width*left,row.y,row.width*width,row.height},size}.Draw(paint,text.c_str(),color);
    };
    const float tableWidth=c.width-128;
    const Rectangle heading=c.Rect(64,layout.compact?202:166,tableWidth,28);
    if(layout.compact) {
        cell(heading,0,.09F,"No.",paint.theme.accent,16);cell(heading,.09F,.25F,"NAME",paint.theme.accent,16);
        cell(heading,.34F,.27F,"DATE",paint.theme.accent,16);cell(heading,.61F,.39F,"SCORE",paint.theme.accent,16);
    } else {
        constexpr std::array<const char*,7> titles{"No.","NAME","DATE","SHOT","RANK","SCORE","STAGE"};
        constexpr std::array<float,8> cuts{0,.07F,.25F,.39F,.58F,.70F,.90F,1};
        for(int i=0;i<7;++i)cell(heading,cuts[i],cuts[i+1]-cuts[i],titles[i],paint.theme.accent,17);
    }
    for(int index=first;index<std::min(first+pageSize,count);++index) {
        const auto row=layout.Row(index-first);const auto color=ui::SelectableRow{row,index}.Draw(paint,index==selectedReplay_?1:0,!replayOccupied_[index] || replayRowErrors_[index].empty());
        const auto& summary=replayRows_[index];
        const std::string number=index<replay::kSlotCount?TextFormat("%02d",index+1):"OLD";
        const std::string name=summary?summary->name:replayOccupied_[index]?"INVALID":"--------";
        std::string date="--------",shot="-------",difficulty="----",value="---------",stage="--";
        if(summary) {
            if(summary->recordedAt) {const auto seconds=static_cast<std::time_t>(summary->recordedAt);if(const auto* time=std::gmtime(&seconds)){std::ostringstream out;out<<std::put_time(time,"%m/%d/%y");date=out.str();}}
            shot=summary->shot;difficulty=spell::kDifficulties[static_cast<unsigned>(summary->difficulty)];value=std::to_string(summary->result.score);
            stage=summary->stage=="extra"?"EX":summary->stage.rfind("stage",0)==0?summary->stage.substr(5):summary->stage;
            if(summary->result.won && (summary->route=="campaign" || summary->route=="extra"))stage="ALL";
            else if(summary->route=="spell-practice")stage=summary->target.rfind("spell-",0)==0?"S"+summary->target.substr(6):summary->target;
        }
        if(layout.compact) {
            Rectangle top=row;top.height=row.height*.50F;
            cell(top,0,.09F,number,color,18);cell(top,.09F,.25F,name,color,18);cell(top,.34F,.27F,date,color,16);cell(top,.61F,.39F,value,color,18);
            Rectangle lower=row;lower.y+=row.height*.50F;lower.height*=.45F;
            cell(lower,.09F,.36F,shot,paint.theme.accent,16);cell(lower,.45F,.25F,difficulty,paint.theme.accent,16);cell(lower,.70F,.30F,stage,paint.theme.accent,16);
        } else {
            const std::array<std::string,7> cells{number,name,date,shot,difficulty,value,stage};
            constexpr std::array<float,8> cuts{0,.07F,.25F,.39F,.58F,.70F,.90F,1};
            for(int i=0;i<7;++i)cell(row,cuts[i],cuts[i+1]-cuts[i],cells[i],color,18);
        }
    }
    for(int direction:{-1,1})ui::ArrowButton{layout.Arrow(direction),direction,direction<0?first>0:first+pageSize<count}.Draw(paint);
    const auto page=std::to_string(first/pageSize+1)+" / "+std::to_string((count+pageSize-1)/pageSize);
    ui::Label{layout.Page(),21,ui::TextAlign::Center}.Draw(paint,page.c_str(),paint.theme.accent);
    std::string status=replayStatus_;
    if(!saving && status.empty() && selectedReplay_<static_cast<int>(replayRows_.size())) {
        const auto& summary=replayRows_[selectedReplay_];
        if(!replayRowErrors_[selectedReplay_].empty())status=replayRowErrors_[selectedReplay_];
        else if(summary) {
            const auto mode=summary->route=="spell-practice"?"符卡练习":summary->route=="stage-practice"?"整关练习":summary->route=="extra"?"Extra Start":"Start";
            status=std::string(mode)+" · "+RecordedStage(summary->stage)+" · "+(summary->result.won?"CLEAR":"未通关");
            if(summary->route=="spell-practice")for(const auto& card:spell::Cards())if(summary->target=="spell-"+std::to_string(card.number)){status+=" · ";status+=card.name;break;}
        } else status="空槽";
    }
    ui::Label{layout.Status(),17,ui::TextAlign::Center}.Draw(paint,status.c_str(),paint.theme.accent);
}
void Game::DrawReplayList() const {DrawReplayTable(false);}

}

namespace koumakyou {
bool Game::OpenReplayFile(const std::filesystem::path& path,int view) {
    replay::Data data;std::string error;
    if(!replay::Load(path,data,error) || !replay::CanPlay(data,error)){replayStatus_=error;return false;}
    PlayReplay(std::move(data));replayView_=std::clamp(view,0,2);return true;
}
}
