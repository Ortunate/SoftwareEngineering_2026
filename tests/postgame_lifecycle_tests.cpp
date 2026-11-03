// 本文件以隐藏图形窗口验证六结局资源交接、真实命名输入、选存失败恢复及多比例页面，不写用户存档。
#include "app/game.hpp"
#include "render/postgame_layout.hpp"
#include "render/score_layout.hpp"
#include "render/replay_layout.hpp"
#include "ui/command_bar.hpp"
#include "platform/window_input.hpp"
#include "raylib.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace koumakyou {
/// @brief 只供此图形目标的探针；构造真实完整通关出口，输入和绘制仍经过正式 Game 接口。
struct GameLifecycleProbe {
    static RunState State(const Game& game){return game.state_;}
    static const score::Profile& Profile(const Game& game){return game.scoreProfile_;}
    static std::string Name(const Game& game){return game.scoreName_;}
    static std::string ReplayName(const Game& game){return game.replayName_;}
    static bool NamingReplay(const Game& game){return game.replaySaveState_==Game::ReplaySaveState::Naming || game.replaySaveState_==Game::ReplaySaveState::Failed;}
    static bool ReplaySlots(const Game& game){return game.replaySaveState_==Game::ReplaySaveState::Slots;}
    static bool ReplayOverwrite(const Game& game){return game.replaySaveState_==Game::ReplaySaveState::Overwrite;}
    static int ReplaySelection(const Game& game){return game.selectedReplay_;}
    static void ReplayList(Game& game){game.OpenReplayList();}
    /// @brief 建立可存录像与失败成绩的共同出口，验证分数先登记、Demo后选槽，绕过战斗不改变生产流程。
    static void ScoreBeforeDemo(Game& game) {
        OpenName(game,true);game.replaySaveState_=Game::ReplaySaveState::Ready;
        game.stageId_=StageId::Stage3;game.finishResult_=RunState::GameOver;
        game.score_=200000;game.BeginScoreSave();
    }
    static bool PlayingReplay(const Game& game){return game.replayMode_==replay::Mode::Playback;}
    static bool ReplaySaved(const Game& game){return game.replaySaveState_==Game::ReplaySaveState::Saved;}
    /// @brief 从真实选存入口打开成绩或Replay命名，数据均在测试临时目录内，避免依赖完整结局演出。
    static void OpenName(Game& game,bool replay) {
        Menu(game);game.replayStatus_.clear();
        if(!replay) {
            game.stageId_=StageId::Stage1;game.runDifficulty_=native_combat::Difficulty::Normal;
            game.player_.ConfigureRun(2,3,false,ShotType::HomingAmulet);game.score_=123456;game.challengeMode_=false;
            game.BeginScoreSave();return;
        }
        replay::BeginSession(game.replayData_,{.seed=123});
        replay::Segment segment;segment.seed=123;segment.rank=32;
        replay::BeginStage(game.replayData_,segment);game.replayData_.frames.push_back({.dt=1.0F/120});
        replay::FinishSession(game.replayData_,{.score=100,.lives=3,.power=8,.won=true});
        game.replayMode_=replay::Mode::Idle;game.replayEnded_=true;game.replaySaveState_=Game::ReplaySaveState::Ready;
        game.RequestReplayAction(Game::ReplayAction::Stay);
    }
    static bool ScoreDifficultyMenu(const Game& game){return game.state_==RunState::Score && game.scoreViewStep_==Game::ScoreViewStep::Difficulty;}
    static std::array<int,3> ScoreFilters(const Game& game){return {game.scoreDifficulty_,game.scoreShot_,game.scoreMode_};}
    static StartSelection LastSelection(const Game& game){return game.lastSelection_;}
    static void ScoreTitle(Game& game){Menu(game);game.selectedMenuItem_=5;}
    /// @brief 为40个查询分组建立不同的前十记录；数值独立编码身份，防止筛选切换后误用其他分组。
    static void SeedQueryScores(Game& game) {
        game.scoreProfile_={};
        for(int mode=0;mode<2;++mode)for(int shot=0;shot<4;++shot)for(int difficulty=0;difficulty<5;++difficulty)
            for(int row=0;row<10;++row) {
                const int last=difficulty==4?7:difficulty==0?5:6;
                const auto name="P"+std::to_string(mode)+std::to_string(difficulty)+std::to_string(shot)+std::to_string(row);
                assert(game.scoreProfile_.Insert(mode!=0,shot,difficulty,
                    {9000000LL+mode*10000000+difficulty*1000000+shot*100000-row*10000,name,row,row,100,1700000000,
                        row==0 || difficulty==4?last:1+row%last,row==0}));
            }
        assert(score::Save(score::kFilename,game.scoreProfile_));
    }
    static bool SaveFailed(const Game& game){return !game.scoreSaveError_.empty();}
    static void Menu(Game& game){game.EnterMainMenu();game.titleEntrance_.Complete();}
    /// @brief 模拟已按难度依次通过前置关卡后的出口，隔离战斗，保留真实收尾/异步资源及成绩写入。
    static void ClearMainline(Game& game,int shot,bool challenge,int misses,long long value=12345678,int difficultyIndex=1) {
        Menu(game);game.stageId_=difficultyIndex==0?StageId::Stage5:StageId::Stage6;
        game.runDifficulty_=*native_combat::DifficultyFromIndex(difficultyIndex);game.selectedDifficulty_=(difficultyIndex+1)%4;
        // 结算从内容读取源关卡号；隔离战斗仍须提供真实内容身份，不能仅写StageId后用空指针模拟通关。
        game.stageContent_=CreateStageContent(game.stageId_,game.runDifficulty_);
        game.practiceMode_=game.spellPractice_=game.previewMode_=false;game.challengeMode_=challenge;
        game.player_.ConfigureRun(2,3,challenge,static_cast<ShotType>(shot));
        game.clearedMainlineStages_=difficultyIndex==0?0x0f:0x1f;game.runInitialLives_=2;game.missCount_=misses;game.bombCount_=2;game.graze_=321;game.score_=value;
        game.stageGraze_=game.stagePointItems_=0;game.stageClearShown_=false;
        game.BeginStageClear();game.ContinueStageClear();
        assert(game.state_==RunState::Finishing && game.pendingEnding_);
        int actors=0;game.actors_.ForEachActive([&](const behavior::Actor&){++actors;});
        assert(actors==0 && game.bullets_.Pool().ActiveCount()==0);
    }
    static void CheckEnding(const Game& game,ending::Id id,bool seen) {
        assert(game.state_==RunState::Ending && !game.stageContent_ && game.endingPlayer_.Variant()==id);
        assert(game.endingPlayer_.SeenBefore()==seen && game.stage_.Elapsed()==0);
        const auto texture=game.assets_.GetVisual(game.endingPlayer_.Background()).texture;
        assert(IsTextureValid(texture) && texture.width==3840 && texture.height==2160);
    }
    static void CompleteEnding(Game& game) {
        const auto value=game.score_;const auto graze=game.graze_;
        game.endingPlayer_.Update(1000);game.UpdateEnding(0);
        assert(game.state_==RunState::ScoreSave && game.assets_.Stage().visuals.empty());
        assert(game.score_==value && game.graze_==graze && !game.pendingEnding_);
        assert(game.scoreProfile_.endingsSeen[static_cast<std::size_t>(game.endingPlayer_.Variant())]);
    }
    /// @brief 保留该分支同时显示文字最多的一页，供窄屏换行与遮挡的人工截图复核。
    static void DensestPage(Game& game) {
        auto player=game.endingPlayer_,densest=player;std::size_t maximum=0;
        while(!player.Finished()) {
            std::size_t bytes=0;for(const auto& line:player.Lines())bytes+=line.text.size();
            if(bytes>maximum){maximum=bytes;densest=player;}
            player.Update(1.0/60);
        }
        game.endingPlayer_=std::move(densest);
    }
    static void CheckGlyphs(const Game& game) {
        const auto font=game.assets_.Common().uiFont;
        assert(font.texture.id!=GetFontDefault().texture.id);
        for(int id=0;id<6;++id)for(const auto& event:ending::Events(static_cast<ending::Id>(id))) {
            if(event.op!=ending::Op::Text)continue;
            int count=0;auto* codepoints=LoadCodepoints(event.text.data(),&count);
            for(int i=0;i<count;++i)if(codepoints[i]!='\n' && codepoints[i]!='\r')
                assert(font.glyphs[GetGlyphIndex(font,codepoints[i])].value==codepoints[i]);
            UnloadCodepoints(codepoints);
        }
    }
    /// @brief 将已结束的本局成绩按实际难度命名保存；此前键鼠回放另行覆盖输入与失败恢复。
    static void SaveDifficultyScore(Game& game,const std::string& name) {
        assert(game.state_==RunState::ScoreSave&&game.scoreDifficulty_==native_combat::DifficultyIndex(game.runDifficulty_));
        game.scoreName_=name;game.SaveNamedScore();assert(game.state_==RunState::Score);
    }
    static void FillHighScores(Game& game) {
        for(int i=0;i<10;++i)assert(game.scoreProfile_.Insert(false,1,1,{90000000+i,"EXISTING",0,0,0,0}));
    }
};
} // namespace koumakyou
namespace {
using namespace koumakyou;
using Probe=GameLifecycleProbe;
void Event(unsigned type,int a,int b=0){PlayAutomationEvent({0,type,{a,b,0,0}});}
void Frame(Game& game) {
    game.Update(1.0F/60);BeginDrawing();ClearBackground(BLACK);game.Draw();EndDrawing();
}
void Key(Game& game,int key){Event(2,key);Frame(game);Event(1,key);Frame(game);}
void Click(Game& game,Rectangle bounds) {
    Event(7,static_cast<int>(bounds.x+bounds.width/2),static_cast<int>(bounds.y+bounds.height/2));
    Event(6,MOUSE_BUTTON_LEFT);Frame(game);Event(5,MOUSE_BUTTON_LEFT);Frame(game);
}
/// @brief 把码点交给 raylib 注册的真实 GLFW 字符回调；与按键事件分开，验证 GetCharPressed 的实际消费。
void Type(Game& game,int key,unsigned character) {
    auto* window=glfwGetCurrentContext();assert(window);
    const auto callback=glfwSetCharCallback(window,nullptr);assert(callback);glfwSetCharCallback(window,callback);
    Event(2,key);callback(window,character);Frame(game);Event(1,key);Frame(game);
}
void WaitEnding(Game& game) {
    const double deadline=GetTime()+60;
    while(Probe::State(game)==RunState::Finishing || game.IsLoading()) {
        assert(GetTime()<deadline && !game.HasLoadingFailed());Frame(game);
    }
    assert(Probe::State(game)==RunState::Ending);
}
void Capture(Game& game,const std::filesystem::path& output,const std::string& name) {
    if(output.empty())return;
    // 双缓冲窗口在交换后读取 back buffer；两次只绘制不更新，避免截图滞后一帧或推进等待。
    for(int i=0;i<2;++i){BeginDrawing();game.Draw();EndDrawing();}
    auto image=LoadImageFromScreen();assert(IsImageValid(image));
    assert(ExportImage(image,(output/(name+".png")).string().c_str()));UnloadImage(image);
}
/// @brief 实际Score入口覆盖难度层级、四页双模式成绩、键鼠翻页、逐级返回和横竖屏布局；查询不得改档。
void RunScoreQuery(Game& game,const std::filesystem::path& output) {
    Probe::SeedQueryScores(game);const auto remembered=Probe::LastSelection(game);
    const auto bytes=[](){std::ifstream stream(score::kFilename);return std::string(std::istreambuf_iterator<char>(stream),{});};
    const auto original=bytes();
    const auto layout=[](){return menu::ScoreLayout(GetScreenWidth(),GetScreenHeight());};
    const auto check=[&](int difficulty,int shot) {
        assert(!Probe::ScoreDifficultyMenu(game) && Probe::State(game)==RunState::Score);
        const auto filters=Probe::ScoreFilters(game);
        assert(filters[0]==difficulty && filters[1]==shot && filters[2]==int(remembered.challenge));
        for(int mode=0;mode<2;++mode)for(int row=0;row<score::kSlots;++row) {
            const auto& entry=Probe::Profile(game).Get(mode!=0,shot,difficulty)[row];
            assert(entry && entry->value==9000000LL+mode*10000000+difficulty*1000000+shot*100000-row*10000);
        }
        assert(!game.IsLoading());
    };
    Probe::ScoreTitle(game);Key(game,KEY_ENTER);assert(Probe::ScoreDifficultyMenu(game));
    for(int difficulty=0;difficulty<5;++difficulty) {
        Click(game,layout().Difficulty(difficulty));
        const int first=Probe::ScoreFilters(game)[1];
        for(int page=0;page<score::kShots;++page) {
            const int shot=(first+page)%score::kShots;check(difficulty,shot);
            Key(game,KEY_LEFT);check(difficulty,(shot+3)%4);
            Key(game,KEY_RIGHT);check(difficulty,shot);
            Key(game,KEY_A);check(difficulty,(shot+3)%4);
            Key(game,KEY_D);check(difficulty,shot);
            Click(game,layout().Arrow(-1));check(difficulty,(shot+3)%4);
            Click(game,layout().Arrow(1));check(difficulty,shot);
            // 旧模式快捷键、纵向输入及标题点击均不能切页或切换模式。
            for(int key:{KEY_UP,KEY_DOWN,KEY_Q,KEY_E}){Key(game,key);check(difficulty,shot);}
            Click(game,layout().ShotLabel(0));Click(game,layout().ModeLabel(1));check(difficulty,shot);
            if(difficulty==1){game.Update(0.5F);Capture(game,output,"score-machine-"+std::to_string(shot));}
            Key(game,KEY_RIGHT);check(difficulty,(shot+1)%4);
        }
        Key(game,KEY_ESCAPE);assert(Probe::ScoreDifficultyMenu(game) && Probe::ScoreFilters(game)[0]==difficulty);
        Key(game,KEY_ENTER);check(difficulty,first);Key(game,KEY_ESCAPE);
    }
    Key(game,KEY_DOWN);assert(Probe::ScoreFilters(game)[0]==0);Key(game,KEY_UP);assert(Probe::ScoreFilters(game)[0]==4);
    // 两模式始终并排，箭头夹在列表外侧且垂直居中；包含窄屏、超宽和最小尺寸。
    for(const auto size:std::array<Vector2,6>{{{390,844},{800,600},{1100,760},{900,900},{1600,400},{320,240}}}) {
        SetWindowSize(size.x,size.y);Frame(game);game.Update(.7F);Capture(game,output,"score-difficulty-"+std::to_string(int(size.x)));
        const auto page=layout();const auto standard=page.Table(0),challenge=page.Table(1);
        const float bottom=ui::CommandBar::ContentHeight(size.x,size.y);
        assert(standard.x+standard.width<challenge.x && standard.y+standard.height<bottom);
        const auto inside=[&](Rectangle r){assert(r.x>=0 && r.y>=0 && r.x+r.width<=size.x+.01F && r.y+r.height<=bottom+.01F);};
        inside(standard);inside(challenge);inside(page.Page());
        for(int i=0;i<5;++i)inside(page.Difficulty(i));
        for(int mode=0;mode<2;++mode) {
            inside(page.ShotLabel(mode));inside(page.ModeLabel(mode));
            assert(page.ModeLabel(mode).y+page.ModeLabel(mode).height<page.Table(mode).y);
        }
        for(int direction:{-1,1}) {
            const auto arrow=page.Arrow(direction);inside(arrow);
            assert(std::abs((arrow.y+arrow.height/2)-(standard.y+standard.height/2))<.01F);
            if(direction<0)assert(arrow.x+arrow.width<standard.x);
            else assert(arrow.x>challenge.x+challenge.width);
        }
        Click(game,page.Difficulty(4));
        int shot=Probe::ScoreFilters(game)[1];
        for(;shot!=3;shot=(shot+1)%4)Key(game,KEY_RIGHT);
        check(4,3);game.Update(0.5F);
        Capture(game,output,"score-records-"+std::to_string(int(size.x)));
        Click(game,page.Arrow(-1));check(4,2);Click(game,page.Arrow(1));check(4,3);
        Click(game,page.Arrow(1));check(4,0); // 第四页继续向右回到第一页。
        Key(game,KEY_ENTER);check(4,0); // 查询确认键不能退出或写入成绩。
        Key(game,KEY_ESCAPE);assert(Probe::ScoreDifficultyMenu(game));
    }
    Key(game,KEY_ESCAPE);assert(Probe::State(game)==RunState::MainMenu);
    assert(bytes()==original);
    const auto after=Probe::LastSelection(game);
    assert(after.shot==remembered.shot && after.character==remembered.character && after.difficulty==remembered.difficulty &&
        after.challenge==remembered.challenge && after.practiceStage==remembered.practiceStage && after.practicePhase==remembered.practicePhase);
    Probe::ScoreTitle(game);Key(game,KEY_ENTER);assert(Probe::ScoreDifficultyMenu(game));Key(game,KEY_ESCAPE);
    std::cout<<"Score query: five difficulties, four pages, side-by-side modes, keyboard/click paging, hierarchy, readonly storage and six layouts passed\n";
}

/// @brief 共用命名界面实机回归：两入口ASCII过滤、键鼠编辑、取消保留、写入失败重试及横竖屏。
void RunNames(Game& game,const std::filesystem::path& output) {
    const auto selectAll=[&](){Event(2,KEY_LEFT_CONTROL);Key(game,KEY_A);Event(1,KEY_LEFT_CONTROL);Frame(game);};
    const auto layout=[](){return ui::NameEntryLayout(GetScreenWidth(),GetScreenHeight());};
    Probe::OpenName(game,true);Key(game,KEY_ENTER);assert(Probe::ReplaySlots(game));
    Key(game,KEY_ENTER);assert(Probe::NamingReplay(game) && Probe::ReplayName(game).empty());
    Key(game,KEY_ESCAPE);Key(game,KEY_ESCAPE);Key(game,KEY_ESCAPE);
    Probe::OpenName(game,false);Key(game,KEY_ENTER);assert(Probe::State(game)==RunState::ScoreName);
    assert(Probe::Name(game).empty());
    Type(game,KEY_W,'W');Type(game,KEY_Z,'Z');Type(game,KEY_X,'X');Type(game,KEY_SPACE,' ');
    Type(game,KEY_U,0x4e2d);assert(Probe::Name(game)=="WZX ");
    for(int i=0;i<12;++i)Type(game,KEY_A,'A');assert(Probe::Name(game)=="WZX AAAA");
    selectAll();Click(game,layout().Character(0));assert(Probe::Name(game)=="A");
    Click(game,layout().Action(2));assert(Probe::Name(game).empty());
    Click(game,layout().Action(0));assert(Probe::SaveFailed(game) && Probe::State(game)==RunState::ScoreName);
    Type(game,KEY_SPACE,' ');Key(game,KEY_ENTER);assert(Probe::SaveFailed(game));
    Click(game,layout().Action(1));assert(Probe::State(game)==RunState::ScoreSave);
    Key(game,KEY_ENTER);assert(Probe::Name(game)==" ");
    Key(game,KEY_ENTER);assert(Probe::State(game)==RunState::ScoreName && Probe::SaveFailed(game));
    // 取消只保留当前尝试草稿；重新开始一次命名仍采用最近成功保存的名字（此时尚为空）。
    assert(Probe::Profile(game).lastName.empty());Probe::OpenName(game,false);Key(game,KEY_ENTER);
    assert(Probe::Name(game).empty());
    selectAll();Type(game,KEY_N,'N');
    Type(game,KEY_A,'A');Type(game,KEY_M,'M');Type(game,KEY_E,'E');
    assert(std::filesystem::create_directory(std::string(score::kFilename)+".tmp"));
    Key(game,KEY_KP_ENTER);assert(Probe::SaveFailed(game) && Probe::Name(game)=="NAME");
    assert(Probe::Profile(game).lastName.empty() && score::Load(score::kFilename).lastName.empty());
    for(const auto size:std::array<Vector2,3>{{{390,844},{800,600},{1100,760}}}) {
        SetWindowSize(size.x,size.y);Frame(game);Capture(game,output,"shared-score-name-"+std::to_string(int(size.x)));
    }
    assert(std::filesystem::remove(std::string(score::kFilename)+".tmp"));
    Click(game,layout().Action(0));assert(Probe::State(game)==RunState::Score && !Probe::ScoreDifficultyMenu(game));
    assert(score::Load(score::kFilename).Get(false,0)[0]->name=="NAME");
    Probe::OpenName(game,false);Key(game,KEY_ENTER);assert(Probe::Name(game)=="NAME");
    Type(game,KEY_N,'N');Type(game,KEY_E,'E');Type(game,KEY_X,'X');Type(game,KEY_T,'T');assert(Probe::Name(game)=="NEXT");
    assert(std::filesystem::create_directory(std::string(score::kFilename)+".tmp"));
    Key(game,KEY_ENTER);assert(Probe::SaveFailed(game) && Probe::Profile(game).lastName=="NAME");
    assert(score::Load(score::kFilename).lastName=="NAME");
    assert(std::filesystem::remove(std::string(score::kFilename)+".tmp"));
    Key(game,KEY_ESCAPE);Probe::OpenName(game,false);Key(game,KEY_ENTER);assert(Probe::Name(game)=="NAME");
    {Game restarted;Probe::OpenName(restarted,false);Key(restarted,KEY_ENTER);assert(Probe::Name(restarted)=="NAME");}
    SetTargetFPS(0);
    Probe::OpenName(game,true);Key(game,KEY_ENTER);assert(Probe::ReplaySlots(game));
    Key(game,KEY_ENTER);assert(Probe::NamingReplay(game));
    const auto initial=Probe::ReplayName(game);assert(initial=="NAME" && ui::NameEntry::Valid(initial,{8}));
    selectAll();Type(game,KEY_U,0x4e2d);assert(Probe::ReplayName(game)==initial);
    Type(game,KEY_W,'W');Type(game,KEY_Z,'Z');Type(game,KEY_X,'X');Type(game,KEY_SPACE,' ');
    assert(Probe::ReplayName(game)=="WZX ");
    for(int i=0;i<40;++i)Type(game,KEY_A,'A');assert(Probe::ReplayName(game).size()==8);
    Click(game,layout().Action(2));assert(Probe::ReplayName(game).size()==7);
    selectAll();Click(game,layout().Character(0));assert(Probe::ReplayName(game)=="A");
    Click(game,layout().Character(ui::NameEntryState::kKeyCount-1));Type(game,KEY_D,'D');assert(Probe::ReplayName(game)=="A D");
    Click(game,layout().Action(1));assert(!Probe::NamingReplay(game));
    Key(game,KEY_ENTER);assert(Probe::NamingReplay(game) && Probe::ReplayName(game)=="A D");
    {std::ofstream block("replays");block<<"block directory creation";}
    Key(game,KEY_ENTER);assert(Probe::NamingReplay(game) && Probe::ReplayName(game)=="A D");
    for(const auto size:std::array<Vector2,3>{{{390,844},{800,600},{1100,760}}}) {
        SetWindowSize(size.x,size.y);Frame(game);Capture(game,output,"shared-replay-name-"+std::to_string(int(size.x)));
    }
    assert(std::filesystem::remove("replays"));Click(game,layout().Action(0));assert(Probe::ReplaySaved(game));
    replay::Data saved;std::string error;int files=0;
    for(const auto& entry:std::filesystem::directory_iterator("replays")) {
        assert(replay::Load(entry.path(),saved,error) && saved.name=="A D");++files;
    }
    assert(files==1 && std::filesystem::exists(replay::SlotPath("replays",1)));
    assert(score::Load(score::kFilename).lastName=="A D");
    // Demo单独命名与成绩共用持久化默认名；覆盖确认默认选择否，取消命名不能破坏旧槽。
    {Game restarted;Probe::OpenName(restarted,false);Key(restarted,KEY_ENTER);assert(Probe::Name(restarted)=="A D");}
    SetTargetFPS(0);Probe::OpenName(game,true);Key(game,KEY_ENTER);
    assert(Probe::ReplaySlots(game) && Probe::ReplaySelection(game)==1);
    Key(game,KEY_UP);Key(game,KEY_ENTER);assert(Probe::ReplayOverwrite(game));
    Capture(game,output,"replay-overwrite");Key(game,KEY_ENTER);assert(Probe::ReplaySlots(game));
    assert(replay::Load(replay::SlotPath("replays",1),saved,error) && saved.name=="A D");
    Key(game,KEY_ENTER);Key(game,KEY_UP);Key(game,KEY_ENTER);assert(Probe::NamingReplay(game));
    selectAll();Type(game,KEY_R,'R');Type(game,KEY_E,'E');Type(game,KEY_D,'D');Type(game,KEY_O,'O');
    Key(game,KEY_ESCAPE);assert(Probe::ReplaySlots(game));
    assert(replay::Load(replay::SlotPath("replays",1),saved,error) && saved.name=="A D");
    Key(game,KEY_ENTER);Key(game,KEY_UP);Key(game,KEY_ENTER);Key(game,KEY_ENTER);
    assert(Probe::ReplaySaved(game) && score::Load(score::kFilename).lastName=="REDO");
    assert(replay::Load(replay::SlotPath("replays",1),saved,error) && saved.name=="REDO");
    Probe::ScoreBeforeDemo(game);Key(game,KEY_ENTER);assert(Probe::Name(game)=="REDO");
    selectAll();Type(game,KEY_S,'S');Type(game,KEY_C,'C');Type(game,KEY_O,'O');Type(game,KEY_R,'R');Type(game,KEY_E,'E');
    Key(game,KEY_ENTER);assert(Probe::State(game)==RunState::Score);
    assert(Probe::Profile(game).Get(false,0)[0]->stage==3 && !Probe::Profile(game).Get(false,0)[0]->cleared);
    for(const auto size:std::array<Vector2,2>{{{390,844},{1100,760}}}) {
        SetWindowSize(size.x,size.y);Frame(game);Capture(game,output,"score-table-"+std::to_string(int(size.x)));
    }
    Key(game,KEY_ENTER);assert(!Probe::NamingReplay(game));Key(game,KEY_ENTER);
    assert(Probe::ReplaySlots(game) && Probe::ReplayName(game)=="SCORE");Key(game,KEY_ENTER);
    assert(Probe::NamingReplay(game));Key(game,KEY_ENTER);assert(Probe::State(game)==RunState::MainMenu);
    assert(replay::Load(replay::SlotPath("replays",2),saved,error) && saved.name=="SCORE");
    // 历史非槽位文件保留为附加页；旧UTF-8长名称不因新编辑限制而丢失。
    saved.name="历史录像名称";std::filesystem::path legacy;
    assert(replay::Save("replays",saved,legacy,error));
    {std::ofstream invalid(replay::SlotPath("replays",3));invalid<<"invalid";}
    Probe::ReplayList(game);
    for(const auto size:std::array<Vector2,2>{{{390,844},{1100,760}}}) {
        SetWindowSize(size.x,size.y);Frame(game);Capture(game,output,"replay-table-"+std::to_string(int(size.x)));
        const int step=menu::ReplayLayout(size.x,size.y).PageSize();
        Key(game,KEY_RIGHT);assert(Probe::ReplaySelection(game)==step);
        Key(game,KEY_LEFT);assert(Probe::ReplaySelection(game)==0);
        const menu::ReplayLayout table(size.x,size.y);
        Click(game,table.Arrow(-1));assert(Probe::ReplaySelection(game)==0);
        Click(game,table.Arrow(1));assert(Probe::ReplaySelection(game)==step);
        Click(game,table.Arrow(-1));assert(Probe::ReplaySelection(game)==0);
    }
    Key(game,KEY_DOWN);Key(game,KEY_DOWN);Key(game,KEY_ENTER);assert(Probe::State(game)==RunState::ReplayList);
    for(int i=0;i<3;++i)Key(game,KEY_RIGHT);assert(Probe::ReplaySelection(game)==45);
    Key(game,KEY_RIGHT);assert(Probe::ReplaySelection(game)==60);Capture(game,output,"replay-legacy");
    Key(game,KEY_LEFT);assert(Probe::ReplaySelection(game)==45);
    Probe::Menu(game);saved.name="OLDVER";saved.simulation="old-version";std::filesystem::path oldVersion;
    assert(replay::SaveSlot("replays",saved,4,false,oldVersion,error));Probe::ReplayList(game);
    for(int i=0;i<3;++i)Key(game,KEY_DOWN);Key(game,KEY_ENTER);assert(Probe::State(game)==RunState::ReplayList);
    for(int i=0;i<3;++i)Key(game,KEY_UP);Key(game,KEY_ENTER);assert(Probe::PlayingReplay(game));Probe::Menu(game);
    std::cout<<"Shared ASCII naming: empty first name, saved-name reuse/restart, score/Replay input, cancellation and save retries passed\n";
}

/// @brief 可见的手动测试入口，F1/F2重置到分数/Demo命名，F12截图，F10退出；存档仍位于临时目录。
/// smoke只用于入口自检，依次展示两个真实命名场景并截图后退出，不要求人工操作。
void PreviewNames(Game& game,const std::filesystem::path& output,bool smoke) {
    SetTargetFPS(60);bool replay=false;int frame=0,captures=0;
    const auto open=[&](bool useReplay) {replay=useReplay;Probe::OpenName(game,replay);Key(game,KEY_ENTER);if(replay)Key(game,KEY_ENTER);};
    open(false);
    while(!WindowShouldClose() && !IsKeyPressed(KEY_F10)) {
        if(IsKeyPressed(KEY_F1))open(false);
        if(IsKeyPressed(KEY_F2))open(true);
        const bool capture=IsKeyPressed(KEY_F12);
        Frame(game);++frame;
        if(capture)Capture(game,output,"name-preview-"+std::to_string(++captures));
        if(smoke && frame==20) {Capture(game,output,"name-preview-score");open(true);}
        if(smoke && frame==40) {Capture(game,output,"name-preview-demo");break;}
    }
}

void Run(Game& game,const std::filesystem::path& output) {
    Probe::CheckGlyphs(game);
    for(int id=0;id<6;++id) {
        const int shot=id<4?id:id==4?0:2;
        Probe::ClearMainline(game,shot,id>=4,id>=4?6:2);WaitEnding(game);
        Probe::CheckEnding(game,static_cast<ending::Id>(id),false);
        Key(game,KEY_ESCAPE);assert(Probe::State(game)==RunState::Ending);
        game.Update(5);Capture(game,output,"ending-"+std::to_string(id));
        if(id==3) {
            Probe::DensestPage(game);
            for(const Vector2 size:std::array<Vector2,3>{{{390,844},{800,600},{1100,760}}}) {
                SetWindowSize(size.x,size.y);
                Capture(game,output,"ending-dense-"+std::to_string(int(size.x)));
            }
        }
        Probe::CompleteEnding(game);
        Capture(game,output,"score-choice-"+std::to_string(id));
    }
    // 再次观看同一结局可跳过，但必须进入选存，而不是直接返回菜单。
    Probe::ClearMainline(game,0,false,2);WaitEnding(game);Probe::CheckEnding(game,ending::Id::ReimuA,true);
    Key(game,KEY_ESCAPE);assert(Probe::State(game)==RunState::ScoreSave);
    Key(game,KEY_Z);assert(Probe::State(game)==RunState::ScoreName);
    assert(Probe::Name(game).empty());
    Type(game,KEY_W,'W');Type(game,KEY_Z,'Z');Type(game,KEY_X,'X');Type(game,KEY_SPACE,' ');
    Type(game,KEY_A,'A');Type(game,KEY_B,'B');Type(game,KEY_C,'C');Type(game,KEY_D,'D');Type(game,KEY_E,'E');
    assert(Probe::State(game)==RunState::ScoreName && Probe::Name(game)=="WZX ABCD");
    const auto before=Probe::Profile(game).Get(false,0)[0];assert(!before);
    assert(std::filesystem::create_directory(std::string(score::kFilename)+".tmp"));
    Key(game,KEY_ENTER);
    assert(Probe::State(game)==RunState::ScoreName && Probe::SaveFailed(game) && Probe::Name(game)=="WZX ABCD");
    assert(!score::Load(score::kFilename).Get(false,0)[0]);
    Capture(game,output,"score-save-failed");
    assert(std::filesystem::remove(std::string(score::kFilename)+".tmp"));
    Key(game,KEY_ENTER);assert(Probe::State(game)==RunState::Score);
    assert(score::Load(score::kFilename).Get(false,0)[0]->name=="WZX ABCD");
    assert(!score::Load(score::kFilename).Get(true,0)[0]);
    Capture(game,output,"score-saved");
    for(const Vector2 size:std::array<Vector2,5>{{{390,844},{800,600},{1100,760},{2560,1440},{3440,1440}}}) {
        const int width=size.x,height=size.y;SetWindowSize(width,height);Frame(game);
        Capture(game,output,"ranking-"+std::to_string(width));
    }
    Key(game,KEY_ENTER);assert(Probe::State(game)==RunState::MainMenu);
    Probe::ClearMainline(game,1,false,0);WaitEnding(game);Key(game,KEY_ESCAPE);
    Key(game,KEY_Z);assert(Probe::State(game)==RunState::ScoreName);
    Type(game,KEY_N,'N');assert(Probe::Name(game)=="N");
    Type(game,KEY_U,0x4e2d);assert(Probe::Name(game)=="N");
    Key(game,KEY_BACKSPACE);assert(Probe::Name(game).empty());
    Type(game,KEY_N,'N');assert(Probe::Name(game)=="N");
    Key(game,KEY_ESCAPE);assert(Probe::State(game)==RunState::ScoreSave);
    Key(game,KEY_Z);assert(Probe::State(game)==RunState::ScoreName && Probe::Name(game)=="N");
    // 公用命名页支持字符按钮；空白点击与只移动焦点仍不能追加字符或提交。
    for(const Vector2 size:std::array<Vector2,3>{{{390,844},{800,600},{1100,760}}}) {
        SetWindowSize(size.x,size.y);Frame(game);
        Capture(game,output,"name-"+std::to_string(int(size.x)));
    }
    Key(game,KEY_LEFT);Key(game,KEY_DOWN);
    Click(game,menu::PostgameLayout(GetScreenWidth(),GetScreenHeight()).canvas.Rect(890,623,40,30));
    assert(Probe::State(game)==RunState::ScoreName && Probe::Name(game)=="N");
    Event(2,KEY_LEFT_CONTROL);Key(game,KEY_A);Event(1,KEY_LEFT_CONTROL);Frame(game);
    Type(game,KEY_N,'N');assert(Probe::Name(game)=="N");
    Key(game,KEY_KP_ENTER);
    assert(Probe::State(game)==RunState::Score && score::Load(score::kFilename).Get(false,1)[0]->name=="N");
    Probe::FillHighScores(game);Probe::ClearMainline(game,1,false,0,1);WaitEnding(game);Key(game,KEY_ESCAPE);
    assert(Probe::State(game)==RunState::ScoreSave);
    Click(game,menu::PostgameLayout(GetScreenWidth(),GetScreenHeight()).Choice(0));
    assert(Probe::State(game)==RunState::ScoreSave);
    Key(game,KEY_Z);assert(Probe::State(game)==RunState::MainMenu);
    assert(Probe::Profile(game).Get(false,1)[0]->value==90000009);
    // 四难度×四御札×两模式分别经过真实末关出口、结局加载和命名保存；不能复用Normal成绩槽。
    for(int d=0;d<4;++d)for(int shot=0;shot<4;++shot)for(bool challenge:{false,true}) {
        const auto before=score::Load(score::kFilename);
        const auto expected=d==0?(shot<2?ending::Id::ReimuBad:ending::Id::MarisaBad):static_cast<ending::Id>(shot);
        Probe::ClearMainline(game,shot,challenge,2,100000000LL+d*1000000+shot*10000,d);WaitEnding(game);
        Probe::CheckEnding(game,expected,before.endingsSeen[static_cast<std::size_t>(expected)]);Probe::CompleteEnding(game);
        const auto name="D"+std::to_string(d)+"S"+std::to_string(shot)+(challenge?"C":"N");
        Probe::SaveDifficultyScore(game,name);const auto after=score::Load(score::kFilename);
        assert(after.Get(challenge,shot,d)[0]&&after.Get(challenge,shot,d)[0]->name==name);
        for(int mode=0;mode<2;++mode)for(int otherShot=0;otherShot<4;++otherShot)for(int otherDifficulty=0;otherDifficulty<5;++otherDifficulty) {
            if(mode==int(challenge)&&otherShot==shot&&otherDifficulty==d)continue;
            const auto& old=before.Get(mode!=0,otherShot,otherDifficulty);const auto& now=after.Get(mode!=0,otherShot,otherDifficulty);
            for(int index=0;index<score::kSlots;++index) {
                assert(bool(old[index])==bool(now[index]));if(!old[index])continue;
                const auto& a=*old[index];const auto& b=*now[index];
                assert(a.value==b.value&&a.name==b.name&&a.misses==b.misses&&a.bombs==b.bombs&&a.graze==b.graze&&a.completedAt==b.completedAt);
            }
        }
    }
    std::cout<<"Six endings, four difficulties, isolated score groups, resource handoff, real name input and save retry passed\n";
}
} // namespace
/// @brief argv[1]为截图目录，argv[2]可选--score-only查询回归、--names-only自动命名回归或--names-preview可见预览。
/// --names-preview-smoke依次预览两个场景后退出；所有模式均隔离临时存档，不初始化音频。
int main(int argc,char** argv) {
    const auto previous=std::filesystem::current_path();
    const auto scratch=std::filesystem::temp_directory_path()/("koumakyou-postgame-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto output=argc>1?std::filesystem::absolute(argv[1]):std::filesystem::path{};
    if(!output.empty())std::filesystem::create_directories(output);
    std::filesystem::create_directory(scratch);std::filesystem::current_path(scratch);
    const std::string mode=argc>2?argv[2]:"";
    const bool preview=mode=="--names-preview" || mode=="--names-preview-smoke";
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN | (preview?0:FLAG_WINDOW_HIDDEN|FLAG_WINDOW_UNFOCUSED));
    InitWindow(1100,760,preview?"Name Entry Test | F1 Score | F2 Demo | F12 Capture | F10 Exit":"Ending and score regression");
    SetTargetFPS(0);SetExitKey(KEY_NULL);int result=0;
#if defined(__APPLE__) || defined(_WIN32)
    assert(koumakyou::DisableWindowInputMethods(GetWindowHandle()));
#endif
    try {auto game=std::make_unique<koumakyou::Game>();SetTargetFPS(0);
        if(preview)PreviewNames(*game,output,mode=="--names-preview-smoke");
        else if(mode=="--names-only")RunNames(*game,output);else if(mode=="--score-only")RunScoreQuery(*game,output);else Run(*game,output);
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';result=1;}
    CloseWindow();std::filesystem::current_path(previous);std::filesystem::remove_all(scratch);return result;
}
