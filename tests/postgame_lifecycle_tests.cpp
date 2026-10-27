// 本文件以隐藏图形窗口验证六结局资源交接、真实命名输入、选存失败恢复及多比例页面，不写用户存档。
#include "app/game.hpp"
#include "render/postgame_layout.hpp"
#include "ui/command_bar.hpp"
#include "platform/window_input.hpp"
#include "raylib.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include <cassert>
#include <chrono>
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
    static bool SaveFailed(const Game& game){return !game.scoreSaveError_.empty();}
    static void Menu(Game& game){game.EnterMainMenu();game.titleEntrance_.Complete();}
    /// @brief 模拟已依次通过前五面后的末关出口，排除战斗数值/时间轴，保留真实收尾和异步资源路径。
    static void ClearMainline(Game& game,int shot,bool challenge,int misses,long long value=12345678) {
        Menu(game);game.stageId_=StageId::Stage6;
        game.practiceMode_=game.spellPractice_=game.previewMode_=false;game.challengeMode_=challenge;
        game.player_.ConfigureRun(2,3,challenge,static_cast<ShotType>(shot));
        game.clearedMainlineStages_=0x1f;game.missCount_=misses;game.bombCount_=2;game.graze_=321;game.score_=value;
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
    assert(Probe::Name(game)=="PLAYER");
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
    // 命名页没有可选字符；方向键与左键不改变名字，也不能穿透到保存动作。
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
    std::cout<<"Six endings, resource handoff, localized glyphs, real name input, atomic-save retry and responsive score pages passed\n";
}
} // namespace
/// @brief argv[1] 为截图目录；配置和成绩全部隔离在新建临时目录，需本地图形会话，不初始化音频。
int main(int argc,char** argv) {
    const auto previous=std::filesystem::current_path();
    const auto scratch=std::filesystem::temp_directory_path()/("koumakyou-postgame-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto output=argc>1?std::filesystem::absolute(argv[1]):std::filesystem::path{};
    if(!output.empty())std::filesystem::create_directories(output);
    std::filesystem::create_directory(scratch);std::filesystem::current_path(scratch);
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_UNFOCUSED|FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(1100,760,"Ending and score regression");SetTargetFPS(0);SetExitKey(KEY_NULL);int result=0;
#if defined(__APPLE__) || defined(_WIN32)
    assert(koumakyou::DisableWindowInputMethods(GetWindowHandle()));
#endif
    try {auto game=std::make_unique<koumakyou::Game>();SetTargetFPS(0);Run(*game,output);}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';result=1;}
    CloseWindow();std::filesystem::current_path(previous);std::filesystem::remove_all(scratch);return result;
}
