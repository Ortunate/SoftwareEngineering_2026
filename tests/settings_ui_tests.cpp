// 本文件以隐藏窗口回放键鼠输入，检查全界面统一操作、比例适配、录入防误触和开局记忆。
#include "app/game.hpp"
#include "app/constants.hpp"
#include "config/settings.hpp"
#include "render/menu_layout.hpp"
#include "render/start_setup_layout.hpp"
#include "raylib.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <array>
#include <cmath>
#include <cstring>

namespace koumakyou {
/// @brief 仅图形测试使用的友元探针；复用已加载的关卡素材创建各 UI 展示状态，不替代正式流程回归。
struct GameLifecycleProbe {
    /// @brief 检查运行时确实加载三张独立纹理，没有因路径错误回退到旧合成图。
    static bool HasTitleLayers(const Game& game) {
        const auto& common = game.assets_.Common();
        return IsTextureValid(common.titleMoon) && IsTextureValid(common.titleShrine) &&
               IsTextureValid(common.titleReimu) && !IsTextureValid(common.titleBackground);
    }
    /// @brief 检查汉化后的操作与所有合法按键名称都在实际加载字体中，防止更新字库后仍出现缺字。
    static bool HasLocalizedGlyphs(const Game& game) {
        const Font font=game.assets_.Common().uiFont;
        if(font.texture.id==GetFontDefault().texture.id)return false;
        const auto contains=[&](const char* text) {
            int count=0;
            int* codepoints=LoadCodepoints(text,&count);
            if(!codepoints)return false;
            bool valid=true;
            for(int index=0;index<count;++index)
                valid=valid && font.glyphs[GetGlyphIndex(font,codepoints[index])].value==codepoints[index];
            UnloadCodepoints(codepoints);
            return valid;
        };
        for(const char* name:kBindingNames)if(!contains(name))return false;
        for(int key=KEY_SPACE;key<=MouseBinding(MOUSE_BUTTON_BACK);++key)
            if(IsValidBinding(key) && !contains(Game::KeyLabel(key).c_str()))return false;
        return contains("按键设置") && contains("恢复默认键位") && contains("梦幻夜行绘卷") &&
               contains("请选择主人公。博丽神社的巫女小姐移动速度攻击范围攻击力准备好了吗？面向大多数人的难度尚未开放");
    }
    /// @brief 设置 page=0～26 的测试展示；调用前须通过 StartPreview 加载关卡，不保存玩家配置。
    static void Show(Game& game,int page) {
        game.dialogueActive_=false; game.spellCutInTimer_=0; game.messageTimer_=0;
        game.waitingForBinding_=false; game.optionsPage_=Game::OptionsPage::General;
        game.titleEntrance_.Complete();
        game.boss_.reset(); game.state_=RunState::MainMenu;
        game.selectedBinding_=6; game.selectedOption_=0; game.selectedPauseItem_=0;
        game.selectedTrack_=0; game.selectedResultItem_=0;
        game.practiceMode_=game.spellPractice_=false;
        switch(page) {
            case 0: game.titleEntrance_={}; break;
            case 1: break;
            case 2: game.state_=RunState::Options; break;
            case 3: case 4:
                game.state_=RunState::Options; game.optionsPage_=Game::OptionsPage::KeyBindings;
                game.waitingForBinding_=page==4; break;
            case 5: case 6: case 7: case 8:
                game.state_=RunState::StartSetup;
                game.setupStep_=page==5?StartSetupStep::Difficulty:page==6?StartSetupStep::Shot:page==7?StartSetupStep::Mode:StartSetupStep::Spell; break;
            case 21: case 22:
                game.state_=RunState::StartSetup;
                game.setupStep_=page==21?StartSetupStep::Character:StartSetupStep::Ready;
                game.spellPractice_=false; break;
            case 23: case 24:
                game.state_=RunState::StartSetup; game.practiceMode_=true;
                game.setupStep_=page==23?StartSetupStep::Stage:StartSetupStep::Ready;
                game.selectedPracticeStage_=0; break;
            case 25:
                // 该截图不继承前一页的极限分数，避免把压力测试值与新增奖励相加造成溢出。
                game.stageClearShown_=false; game.player_.AdjustPower(84-game.player_.Power());
                game.score_=4965020; game.missCount_=game.bombCount_=0;
                game.graze_=75; game.pointItems_=25; game.BeginStageClear(); break;
            case 26:
                game.state_=RunState::Paused; game.pausedState_=RunState::Playing; game.practiceMode_=true; break;
            case 9: game.state_=RunState::MusicRoom; break;
            case 10: game.state_=RunState::Score; break;
            case 11: game.state_=RunState::Credit; break;
            case 12: game.state_=RunState::Playing; break;
            case 13:
                game.state_=RunState::Playing; game.dialogueActive_=true; game.dialogueAfterBattle_=false;
                game.dialogueTimer_=31; break;
            case 14:
                game.state_=RunState::Playing; game.BeginStageBoss(true,3); game.boss_->AdvancePhase();
                game.spellCutInTimer_=1.5F; break;
            case 15: game.state_=RunState::Paused; game.pausedState_=RunState::Playing; break;
            case 16: game.state_=RunState::Won; break;
            case 17: game.state_=RunState::GameOver; break;
            case 18:
                game.state_=RunState::Playing;
                game.SetMessage("FULL POWER MODE / STAGE 1 / SPELL CARD BONUS 999999999 / SHOT TEST / SETTINGS",3); break;
            case 19: case 20:
                game.state_=RunState::Playing;
                game.score_=9223372036854775807LL; game.bestScore_=2000000000;
                game.graze_=game.pointItems_=999999;
                game.challengeMode_=page==20; game.missCount_=999999;
                game.player_.ConfigureRun(5,3,game.challengeMode_,ShotType::Needle);
                game.player_.AdjustPower(Player::kMaxPower);
                break;
        }
        game.UpdateUiMotion(1.0F);
    }
    /// @brief 设置启动展示的可重复截图时刻；仅推进展示，不执行菜单输入或修改玩家配置。
    static void StartupAt(Game& game,float seconds) {
        game.titleEntrance_={}; game.menuMotion_={}; game.UpdateUiMotion(seconds);
    }
    static bool Setup(const Game& game) { return game.state_==RunState::StartSetup; }
    static StartSetupStep SetupStep(const Game& game) { return game.setupStep_; }
    /// @brief 动画测试只推进展示时钟；换页仍使用正式展示状态，不推进关卡或伪造输入队列。
    static void Motion(Game& game,float seconds) { game.UpdateUiMotion(seconds); }
    static void MotionStep(Game& game,StartSetupStep step,float seconds) {
        game.setupStep_=step;game.UpdateUiMotion(seconds);
    }
    static float CharacterVisibility(const Game& game) { return game.startSetupMotion_.Character(); }
    static bool MainMenu(const Game& game) { return game.state_==RunState::MainMenu; }
    static bool EntranceComplete(const Game& game) { return game.titleEntrance_.Finished(); }
    static int MenuSelection(const Game& game) { return game.selectedMenuItem_; }
    static bool Waiting(const Game& game) { return game.waitingForBinding_; }
    static bool General(const Game& game) { return game.optionsPage_==Game::OptionsPage::General; }
    static bool Playing(const Game& game) { return game.state_==RunState::Playing; }
    static RunState State(const Game& game) { return game.state_; }
    /// @brief 读取当前页的局部选项编号，供真实输入回放检查悬停与键盘切换。
    static int UiSelection(const Game& game) {
        if(game.state_==RunState::MainMenu)return game.selectedMenuItem_;
        if(game.state_==RunState::Options)return General(game)?game.selectedOption_:game.selectedBinding_;
        if(game.state_==RunState::MusicRoom)return game.selectedTrack_;
        if(game.state_==RunState::Paused)return game.selectedPauseItem_;
        if(game.state_==RunState::Won || game.state_==RunState::GameOver)return game.selectedResultItem_;
        return game.setupStep_==StartSetupStep::Shot?game.selectedShot_:
            game.setupStep_==StartSetupStep::Mode?int(game.challengeMode_):int(game.practicePhase_==4);
    }
    static float DialogueTime(const Game& game) { return game.dialogueTimer_; }
    /// @brief 以真实结算入口检查成绩规则；仅替代漫长的整关计分过程，不绕过存档路径。
    static void FinishScoreAttempt(Game& game,long long score,bool challenge,bool spell,bool preview,RunState result) {
        game.practiceMode_=true; game.spellPractice_=spell; game.previewMode_=preview;
        game.challengeMode_=challenge; game.score_=score; game.EndStage(result);
    }
    static long long StageBest(const Game& game,bool challenge,int stage) { return game.stageHighScores_.Best(challenge,stage); }
    static int SelectedStage(const Game& game) { return game.selectedPracticeStage_; }
    /// @brief 使用截图示例计数触发正式战后入口；预览只用于预加载，按目标模式恢复真实计分资格。
    static void BeginClear(Game& game,bool practice) {
        game.previewMode_=false; game.practiceMode_=practice; game.spellPractice_=false;
        game.challengeMode_=true; game.selectedShot_=1;
        game.player_.AdjustPower(84-game.player_.Power());
        game.graze_=75; game.pointItems_=25; game.score_=10000;
        game.BeginStageDialogue(4,true);
    }
    /// @brief 按真实 Update 路径结束战后对话，不直接跳转结算状态。
    static void FinishPostDialogue(Game& game) { game.dialogueTimer_=game.DialogueDuration(); game.Update(0); }
    /// @brief 将战后独白定位到最后一句仍未结束的位置，供真实按键验证层间确认不穿透。
    static void LastPostLine(Game& game) { game.dialogueTimer_=game.DialogueDuration()-1; }
    static void ReenterClear(Game& game) { game.BeginStageClear(); }
    static float ClearSeconds(const Game& game) { return game.stageClearSeconds_; }
    static float StageTime(const Game& game) { return game.stage_.Elapsed(); }
    static long long Score(const Game& game) { return game.score_; }
    static bool StageReleased(const Game& game) { return game.assets_.Stage().visuals.empty() && game.actorEffects_.Pool().ActiveCount()==0; }


    /// @brief 注入加载失败展示，单独验证退出操作，不触发真实磁盘故障。
    static void FailLoading(Game& game) { game.state_=RunState::Loading; game.loadingFailed_=true; }

    /// @brief 读取实际选择页使用的值，不能以配置文件存在代替界面恢复检查。
    static StartSelection Selection(const Game& game) {
        return {game.selectedShot_,game.challengeMode_,game.practicePhase_};
    }
};
}

namespace {
/// @brief 检查可观察结果；失败抛出异常，由 main 输出错误并返回非零值。
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
/// @brief 回放 raylib 自动化事件；编号沿用 raylib 5.5/6.0 的事件格式。
void Event(unsigned int type, int a, int b = 0) {
    PlayAutomationEvent(AutomationEvent{0, type, {a, b, 0, 0}});
}
/// @brief 推进一帧，EndDrawing 更新输入的上一帧状态；窗口保持隐藏。
void Frame(koumakyou::Game& game) {
    game.Update(1.0F / 60.0F);
    BeginDrawing();
    ClearBackground(BLACK);
    game.Draw();
    EndDrawing();
}
/// @brief 设置/暂停测试先等待异步加载结束，避免把加载页上的按键误当成战斗输入。
void WaitForStage(koumakyou::Game& game) {
    const double deadline = GetTime() + 60;
    while (game.IsLoading()) {
        Check(GetTime() < deadline && !game.HasLoadingFailed(), "stage loading failed or timed out");
        Frame(game);
    }
    Check(koumakyou::GameLifecycleProbe::Playing(game), "stage did not enter Playing");
}
/// @brief 模拟一次完整键盘按下和松开。
void Key(koumakyou::Game& game, int key) {
    Event(2, key); Frame(game);
    Event(1, key); Frame(game);
}
/// @brief 点击一个窗口坐标并释放，检验实际鼠标命中区域而非直接调用调整函数。
void Click(koumakyou::Game& game, int x, int y, int button = MOUSE_BUTTON_LEFT) {
    Event(7, x, y); Event(6, button); Frame(game);
    Event(5, button); Frame(game);
}
/// @brief 从正式绘制入口读取当前画面，不推进时钟；返回的 Image 由调用方 UnloadImage。
Image ReadFrame(koumakyou::Game& game) {
    RenderTexture2D target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    BeginTextureMode(target);
    ClearBackground(BLACK);
    game.Draw();
    EndTextureMode();
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    UnloadRenderTexture(target);
    return image;
}
/// @brief 保存当前窗口；settleFrames 可为零，filled 检查页顶/页底渐变抵达窗口边缘，防止退回居中留边。
void Capture(koumakyou::Game& game, const char* path, int settleFrames = 24, bool filled = false) {
    for (int frame=0;frame<settleFrames;++frame) Frame(game);
    Image image=ReadFrame(game);
    if(filled) {
        const Color top=GetImageColor(image,image.width/2,0),bottom=GetImageColor(image,image.width/2,image.height-1);
        Check(std::abs(top.r-18)<=2 && std::abs(top.g-16)<=2 && std::abs(top.b-19)<=2,
              "secondary page top has letterboxing or wrong template");
        Check(std::abs(bottom.r-62)<=2 && std::abs(bottom.g-17)<=2 && std::abs(bottom.b-25)<=2,
              "secondary page bottom has letterboxing or wrong template");
    }
    Check(ExportImage(image, path), "screenshot export failed");
    UnloadImage(image);
}

/// @brief 导出实际首页到难度及后续步骤的连续帧，验证卡片真实淡入、零时间换页无闪变和人物连续性。
void TestStartAnimations(const std::filesystem::path& screenshots) {
    using namespace koumakyou;
    const int width=GetScreenWidth(),height=GetScreenHeight();
    SetWindowSize(1280,720);
    Game game;
    GameLifecycleProbe::StartupAt(game,3);
    SetTargetFPS(0);Event(7,0,0);Frame(game);
    Key(game,KEY_Z);
    int number=0;
    const auto capture=[&] {
        const auto name=screenshots/(std::string(TextFormat("start-transition-%03d.png",number++)));
        Capture(game,name.string().c_str(),0);
    };
    Color middle{};
    const menu::StartSetupLayout layout(1280,720);
    const auto normal=layout.Difficulty(1);
    const int x=normal.x+normal.width*0.25F,y=normal.y+normal.height*0.82F;
    for(int frame=0;frame<22;++frame) {
        capture();
        if(frame==9) { Image image=ReadFrame(game);middle=GetImageColor(image,x,y);UnloadImage(image); }
        GameLifecycleProbe::Motion(game,1.0F/30);
    }
    Image before=ReadFrame(game);
    const auto full=GetImageColor(before,x,y);
    Check(full.r>middle.r+20 && full.g>middle.g+20 && full.b>middle.b+20,"difficulty card did not fade in");
    GameLifecycleProbe::MotionStep(game,StartSetupStep::Character,0);
    Image boundary=ReadFrame(game);
    Check(before.width==boundary.width && before.height==boundary.height && before.format==boundary.format &&
          std::memcmp(before.data,boundary.data,GetPixelDataSize(before.width,before.height,before.format))==0,
          "zero-time page transition changed rendered pixels");
    UnloadImage(before);UnloadImage(boundary);
    for(int frame=0;frame<12;++frame) { capture();GameLifecycleProbe::Motion(game,1.0F/30); }
    for(const auto step:{StartSetupStep::Shot,StartSetupStep::Mode,StartSetupStep::Ready}) {
        Key(game,KEY_Z);
        Check(GameLifecycleProbe::SetupStep(game)==step,"animation changed input sequence");
        for(int frame=0;frame<12;++frame) {
            Check(std::abs(GameLifecycleProbe::CharacterVisibility(game)-1)<0.00001F,"character replayed entry between setup steps");
            capture();GameLifecycleProbe::Motion(game,1.0F/30);
        }
    }
    Key(game,KEY_X);Key(game,KEY_X);GameLifecycleProbe::Motion(game,1);
    Key(game,KEY_RIGHT); // 行强调仍在变化时确认，退出页也必须保留当前亮度。
    before=ReadFrame(game);
    GameLifecycleProbe::MotionStep(game,StartSetupStep::Mode,0);
    boundary=ReadFrame(game);
    Check(std::memcmp(before.data,boundary.data,GetPixelDataSize(before.width,before.height,before.format))==0,
          "confirming during selection reset outgoing highlight");
    UnloadImage(before);UnloadImage(boundary);
    Key(game,KEY_Z);Key(game,KEY_X);GameLifecycleProbe::Motion(game,0.04F);
    SetWindowSize(390,844);Frame(game);
    Check(std::abs(GameLifecycleProbe::CharacterVisibility(game)-1)<0.00001F,"resizing reset shared character visibility");
    Capture(game,(screenshots/"start-animation-resize-mid.png").string().c_str(),0);
    Key(game,KEY_Z);Key(game,KEY_X);Key(game,KEY_Z);
    Check(GameLifecycleProbe::SetupStep(game)==StartSetupStep::Ready,"interrupted transition lost latest input");
    GameLifecycleProbe::Motion(game,1);
    Capture(game,(screenshots/"start-animation-interrupted-ready.png").string().c_str(),0);
    SetWindowSize(width,height);
    std::puts("Start card fade, zero-time rendered continuity, shared character, resize and interrupted transitions passed.");
}

/// @brief 按真实键鼠流程检查五步开局、确认防穿透、逐级返回、选择保存、预览隔离及重启恢复。
void TestStartSelectionMemory(const std::filesystem::path& screenshots) {
    using namespace koumakyou;
    const auto expect=[](const Game& game,StartSetupStep step) {
        Check(GameLifecycleProbe::Setup(game) && GameLifecycleProbe::SetupStep(game)==step,"wrong Start setup step");
    };
    const auto next=[&](Game& game,StartSetupStep step) { Key(game,KEY_Z); expect(game,step); };
    const auto back=[&](Game& game,StartSetupStep step) { Key(game,KEY_X); expect(game,step); };
    auto settings=LoadOrCreateSettings(kSettingsFilename);
    settings.lastSelection={};
    Check(SaveSettingsFile(kSettingsFilename,settings),"selection test setup failed");
    {
        Game game;
        GameLifecycleProbe::StartupAt(game,3);
        SetTargetFPS(0); Event(7,0,0); Frame(game);
        next(game,StartSetupStep::Difficulty);
        next(game,StartSetupStep::Character);
        next(game,StartSetupStep::Shot);
        Key(game,KEY_RIGHT); next(game,StartSetupStep::Mode); Key(game,KEY_DOWN);
        auto selected=GameLifecycleProbe::Selection(game);
        Check(selected.shot==1 && selected.challenge,"keyboard selection failed");
        // 按住模式页的确认键不能穿过最终确认页；确认前不能开始异步加载。
        Event(2,KEY_Z); Frame(game);
        for(int frame=0;frame<5;++frame)Frame(game);
        expect(game,StartSetupStep::Ready);
        Check(!game.IsLoading(),"mode confirmation started loading before Ready confirmation");
        Event(1,KEY_Z); Frame(game);
        Capture(game,(screenshots/"start-ready-challenge.png").string().c_str());
        back(game,StartSetupStep::Mode); back(game,StartSetupStep::Shot);
        back(game,StartSetupStep::Character); back(game,StartSetupStep::Difficulty);
        Key(game,KEY_X); Check(GameLifecycleProbe::MainMenu(game),"difficulty cancel did not return to title");
        next(game,StartSetupStep::Difficulty); next(game,StartSetupStep::Character); next(game,StartSetupStep::Shot);
        next(game,StartSetupStep::Mode); next(game,StartSetupStep::Ready);
        selected=GameLifecycleProbe::Selection(game);
        Check(selected.shot==1 && selected.challenge,"returning to setup forgot selections");
        Key(game,KEY_Z); WaitForStage(game);
        Check(GameLifecycleProbe::Selection(game).challenge,"starting run reset challenge");
        game.Reset(); game.StartPreview(); game.OpenOptions();
        Key(game,KEY_RIGHT); SetTargetFPS(0);
        const auto saved=LoadOrCreateSettings(kSettingsFilename).lastSelection;
        Check(saved.shot==1 && saved.challenge,"preview or options save overwrote selection memory");
        game.Reset();
        Key(game,KEY_DOWN); Key(game,KEY_DOWN); next(game,StartSetupStep::Spell);
        Check(!GameLifecycleProbe::Selection(game).challenge,"spell practice inherited hidden challenge mode");
        Key(game,KEY_X);
        Check(GameLifecycleProbe::MenuSelection(game)==3,"returning from spell practice lost title selection");
        Key(game,KEY_UP); Key(game,KEY_UP);
        next(game,StartSetupStep::Difficulty); next(game,StartSetupStep::Character);
        next(game,StartSetupStep::Shot); next(game,StartSetupStep::Mode);
        Check(GameLifecycleProbe::Selection(game).challenge,"spell practice reset saved challenge selection");
    }
    {
        Game restarted;
        GameLifecycleProbe::StartupAt(restarted,3);
        SetTargetFPS(0); Event(7,0,0); Frame(restarted);
        next(restarted,StartSetupStep::Difficulty); next(restarted,StartSetupStep::Character); next(restarted,StartSetupStep::Shot);
        auto selected=GameLifecycleProbe::Selection(restarted);
        Check(selected.shot==1 && selected.challenge,"restart did not restore visible selections");
        const menu::StartSetupLayout layout(GetScreenWidth(),GetScreenHeight());
        const auto click=[&](Rectangle row) { Click(restarted,row.x+row.width/2,row.y+row.height/2); };
        click(layout.Choice(0)); expect(restarted,StartSetupStep::Mode);
        click(layout.Choice(0)); expect(restarted,StartSetupStep::Ready);
        const auto saved=LoadOrCreateSettings(kSettingsFilename).lastSelection;
        Check(saved.shot==0 && !saved.challenge,"mouse selections were not saved");
        back(restarted,StartSetupStep::Mode);
        next(restarted,StartSetupStep::Ready); click(layout.ReadyButton()); WaitForStage(restarted);
        restarted.Reset(); Event(7,0,0); Frame(restarted);
        Key(restarted,KEY_DOWN); Key(restarted,KEY_DOWN); next(restarted,StartSetupStep::Spell);
        Key(restarted,KEY_RIGHT); next(restarted,StartSetupStep::Ready); back(restarted,StartSetupStep::Spell);
        Key(restarted,KEY_X); Check(GameLifecycleProbe::MainMenu(restarted),"spell cancel did not return to title");
        Check(GameLifecycleProbe::MenuSelection(restarted)==3,"spell practice title selection was not retained");
        next(restarted,StartSetupStep::Spell);
        Check(GameLifecycleProbe::Selection(restarted).practicePhase==4,"spell practice selection was not restored");
        Check(LoadOrCreateSettings(kSettingsFilename).lastSelection.practicePhase==4,"spell practice selection was not saved");
    }
    std::puts("Five-step Start flow, confirmation, backtracking, input and persisted selections passed.");
}

/// @brief 检查所有页面在小窗口、竖屏、方形、超宽屏和常用宽屏下的呈现，并回放缩放后的弹窗与暂停按钮。
void AllPageSizes(koumakyou::Game& game,const std::filesystem::path& screenshots) {
    constexpr std::array<const char*,27> names{"startup","title","options","bindings","binding-modal","difficulty","shot","mode",
        "spell-practice","music","score","credit","playing","dialogue","spell-cutin","pause","won","gameover","message",
        "hud-max-values","hud-challenge","character","ready","practice-stage","practice-ready","stage-clear","practice-pause"};
    for(const Vector2 size : {Vector2{320,240},{390,844},{900,900},{1600,400},{1280,720}}) {
        SetWindowSize(size.x,size.y); Event(7,0,0); Frame(game);
        game.StartPreview();
        const std::string dimensions=std::to_string(GetScreenWidth())+"x"+std::to_string(GetScreenHeight());
        for(int page=0;page<static_cast<int>(names.size());++page) {
            koumakyou::GameLifecycleProbe::Show(game,page);
            Capture(game,(screenshots/(dimensions+"-"+names[page]+".png")).string().c_str(),0,
                    page>=2 && page<=11 && page!=4);
        }
        koumakyou::GameLifecycleProbe::Show(game,3);
        const koumakyou::menu::BindingsLayout bindings(GetScreenWidth(),GetScreenHeight());
        const auto row=bindings.Row(6);
        Click(game,row.x+row.width/2,row.y+row.height/2);
        Check(koumakyou::GameLifecycleProbe::Waiting(game),"resized binding row missed");
        Key(game,KEY_ESCAPE);
        const auto back=bindings.Button(1);
        Click(game,back.x+back.width/2,back.y+back.height/2);
        Check(koumakyou::GameLifecycleProbe::General(game),"resized bindings Back missed");
        game.StartPreview(); koumakyou::GameLifecycleProbe::Show(game,15);
        const koumakyou::menu::OverlayLayout overlay(GetScreenWidth(),GetScreenHeight());
        const auto resume=overlay.PauseRow(0);
        Click(game,resume.x+resume.width/2,resume.y+resume.height/2);
        Check(koumakyou::GameLifecycleProbe::Playing(game),"resized pause Resume missed");
    }
    SetWindowSize(900,620); Event(7,0,0); Frame(game);
}

/// @brief 通过正式更新入口逐页验证键鼠等价、返回优先级、静止指针及确认不穿透。
void TestUnifiedUiInputs() {
    using namespace koumakyou;
    using Probe=GameLifecycleProbe;
    Game game;
    SetTargetFPS(0);
    game.StartPreview();
    const menu::StartSetupLayout setup(GetScreenWidth(),GetScreenHeight());
    const menu::PageLayout page(GetScreenWidth(),GetScreenHeight());
    const menu::OverlayLayout overlay(GetScreenWidth(),GetScreenHeight());
    const menu::BindingsLayout bindings(GetScreenWidth(),GetScreenHeight());
    const auto click=[&](Rectangle bounds,int button=MOUSE_BUTTON_LEFT) {
        Click(game,bounds.x+bounds.width/2,bounds.y+bounds.height/2,button);
    };
    const auto move=[&](Rectangle bounds) {
        Event(7,bounds.x+bounds.width/2,bounds.y+bounds.height/2); Frame(game);
    };
    const auto show=[&](int id) { Probe::Show(game,id); Event(7,0,0); Frame(game); };
    // 每个有多项选择的页面都检查八个方向别名；设置数值页的左右键单独用于调值。
    for(const auto spec : {std::array<int,2>{3,12},{6,2},{7,2},{8,2},{9,4},{15,3},{16,2},{17,2}}) {
        for(const auto key : {KEY_W,KEY_UP,KEY_A,KEY_LEFT,KEY_S,KEY_DOWN,KEY_D,KEY_RIGHT}) {
            show(spec[0]);
            const int before=Probe::UiSelection(game);
            const int direction=key==KEY_W || key==KEY_UP || key==KEY_A || key==KEY_LEFT?-1:1;
            Key(game,key);
            Check(Probe::UiSelection(game)==(before+spec[1]+direction)%spec[1],"UI direction alias failed");
        }
        show(spec[0]);
        const auto target=spec[0]==3?bindings.Row(1):spec[0]==9?page.Track(1):
            spec[0]==15?overlay.PauseRow(1):spec[0]>=16?overlay.ResultRow(1):setup.Choice(1);
        move(target);
        Check(Probe::UiSelection(game)==1,"mouse movement did not select UI item");
        Key(game,KEY_A);
        Check(Probe::UiSelection(game)==0,"stationary mouse overrode keyboard selection");
        Click(game,1,1);
        Check(Probe::UiSelection(game)==0,"blank click changed UI selection");
    }
    for(const int key : {KEY_D,KEY_RIGHT,KEY_S,KEY_DOWN}) {
        show(1); Key(game,KEY_X); // 首页返回只选中退出，不能直接退出游戏。
        Check(Probe::MenuSelection(game)==9 && !game.ShouldQuit(),"title back did not select Quit safely");
        Key(game,key);
        Check(Probe::MenuSelection(game)==0,"title navigation alias failed");
    }
    for(const int key : {KEY_Z,KEY_SPACE,KEY_ENTER}) {
        show(5); Key(game,key); Check(Probe::SetupStep(game)==StartSetupStep::Character,"difficulty confirm alias failed");
        Key(game,key); Check(Probe::SetupStep(game)==StartSetupStep::Shot,"character confirm alias failed");
        Key(game,key); Check(Probe::SetupStep(game)==StartSetupStep::Mode,"shot confirm alias failed");
        Key(game,key); Check(Probe::SetupStep(game)==StartSetupStep::Ready && !game.IsLoading(),"mode confirmation skipped Ready");
        show(15); Key(game,key); Check(Probe::Playing(game),"pause confirmation alias failed");
        show(3); Key(game,key); Check(Probe::Waiting(game),"binding confirmation alias failed");
        Key(game,KEY_X); Check(!Probe::Waiting(game),"binding prompt X did not cancel");
        show(9); Key(game,key); Check(Probe::State(game)==RunState::MusicRoom,"music confirmation left page");
        show(10); Key(game,key); Check(Probe::MainMenu(game),"score confirmation did not return");
        show(11); Key(game,key); Check(Probe::MainMenu(game),"credit confirmation did not return");
        // 上面返回首页会释放关卡；后续对话和结果渲染需要重新加载预览资源。
        game.StartPreview();
        show(13); const float before=Probe::DialogueTime(game); Key(game,key);
        const float advance=Probe::DialogueTime(game)-before;
        Check(advance>0.1F && advance<10,"dialogue confirmation did not advance exactly one line");
    }
    show(5); click(setup.Difficulty(0));
    Check(Probe::SetupStep(game)==StartSetupStep::Difficulty,"disabled difficulty accepted click");
    click(setup.Difficulty(1)); Check(Probe::SetupStep(game)==StartSetupStep::Character,"Normal click did not confirm");
    click(setup.Portrait()); Check(Probe::SetupStep(game)==StartSetupStep::Shot,"character click did not confirm");
    // 持续按住左键只能推进一次；释放并重新点击才能确认后续页面。
    const auto choice=setup.Choice(1);
    Event(7,choice.x+choice.width/2,choice.y+choice.height/2); Event(6,MOUSE_BUTTON_LEFT);
    Frame(game); Frame(game); Frame(game);
    Check(Probe::SetupStep(game)==StartSetupStep::Mode,"held click skipped setup step");
    Event(5,MOUSE_BUTTON_LEFT); Frame(game);
    click(setup.Choice(1)); Check(Probe::SetupStep(game)==StartSetupStep::Ready,"mode click did not confirm");
    click(setup.ReadyButton()); WaitForStage(game);
    for(const int id : {6,7,8}) {
        show(id); click(setup.Choice(0));
        Check(Probe::SetupStep(game)==(id==6?StartSetupStep::Mode:StartSetupStep::Ready),"choice click did not advance");
    }
    show(3); click(bindings.Row(0)); Check(Probe::Waiting(game),"binding click did not open prompt");
    const auto saved=LoadOrCreateSettings(kSettingsFilename).bindings;
    click(bindings.Modal(),MOUSE_BUTTON_RIGHT);
    Check(!Probe::Waiting(game) && LoadOrCreateSettings(kSettingsFilename).bindings.forward==saved.forward,
          "prompt right click was captured instead of cancelling");
    show(3);
    Event(2,KEY_SPACE); Frame(game); Frame(game);
    Check(Probe::Waiting(game),"held Space was captured as binding");
    Event(1,KEY_SPACE); Frame(game); Key(game,KEY_ESCAPE);
    show(9); move(page.Back()); Check(Probe::UiSelection(game)==3,"music Back hover did not select");
    Key(game,KEY_SPACE); Check(Probe::MainMenu(game),"music Back keyboard confirmation failed");
    game.StartPreview();
    show(13); const float before=Probe::DialogueTime(game); Click(game,1,1);
    Check(Probe::DialogueTime(game)-before>0.1F,"dialogue left click did not advance");
    for(const int button : {MOUSE_BUTTON_LEFT,MOUSE_BUTTON_RIGHT}) {
        show(16); click(overlay.ResultRow(1),button);
        Check(Probe::MainMenu(game),"result mouse return failed");
        game.StartPreview();
    }
    for(const int id : {16,17}) {
        show(id); click(overlay.ResultRow(0)); WaitForStage(game);
        show(id); Key(game,KEY_SPACE); WaitForStage(game);
    }
    for(const int cancel : std::array<int,3>{KEY_X,KEY_ESCAPE,-1}) {
        for(const int id : {2,3,5,6,7,8,9,10,11,13,15,16,17,21,22}) {
            game.StartPreview(); show(id);
            if(cancel<0)Click(game,1,1,MOUSE_BUTTON_RIGHT); else Key(game,cancel);
            if(id==2 || id==5 || id==8 || (id>=9 && id<=11) || id==16 || id==17)
                Check(Probe::MainMenu(game),"UI return did not reach title");
            else if(id==3)Check(Probe::General(game),"binding return did not reach options");
            else if(id==13)Check(Probe::State(game)==RunState::Paused,"dialogue return did not pause");
            else if(id==15)Check(Probe::Playing(game),"pause return did not resume");
            else Check(Probe::SetupStep(game)==(id==6?StartSetupStep::Character:id==7?StartSetupStep::Shot:
                        id==21?StartSetupStep::Difficulty:StartSetupStep::Mode),"setup return skipped a step");
        }
        game.StartPreview(); show(3); Key(game,KEY_Z);
        if(cancel<0)Click(game,1,1,MOUSE_BUTTON_RIGHT); else Key(game,cancel);
        Check(!Probe::Waiting(game),"binding modal return failed");
        Probe::FailLoading(game);
        if(cancel<0)Click(game,1,1,MOUSE_BUTTON_RIGHT); else Key(game,cancel);
        Check(Probe::MainMenu(game),"loading failure return failed");
    }
    game.StartPreview(); show(15);
    Event(2,KEY_SPACE); Event(6,MOUSE_BUTTON_RIGHT); Frame(game);
    Check(Probe::Playing(game),"return did not take priority over confirmation");
    Event(1,KEY_SPACE); Event(5,MOUSE_BUTTON_RIGHT); Frame(game);
    std::puts("Unified UI keyboard, hover, click, cancel and held-input regression passed.");
}


/// @brief 回放 Practice Start 的关卡确认/返回及分模式记录，导出带真实最高分的宽窄屏展示。
void TestPracticeStages(const std::filesystem::path& screenshots) {
    using namespace koumakyou;
    using Probe=GameLifecycleProbe;
    const int width=GetScreenWidth(),height=GetScreenHeight();
    auto settings=LoadOrCreateSettings(kSettingsFilename);
    settings.lastSelection={0,false,2,6}; // 未开放的记忆槽必须回退到已开放的第一面。
    settings.stageHighScores={};
    settings.stageHighScores.Record(false,1,5176180);
    settings.stageHighScores.Record(true,1,8754321);
    Check(SaveSettingsFile(kSettingsFilename,settings),"practice records seed failed");
    Game game;
    SetTargetFPS(0); SetWindowSize(1280,720);
    Probe::StartupAt(game,3); Event(7,0,0); Frame(game);
    const menu::TitleLayout title(GetScreenWidth(),GetScreenHeight(),menu::kTitleRowCount);
    const auto practice=title.Row(2);
    Click(game,practice.x+practice.width/2,practice.y+practice.height/2);
    const auto expect=[&](StartSetupStep step) {
        Check(Probe::Setup(game) && Probe::SetupStep(game)==step,"wrong practice setup step");
    };
    expect(StartSetupStep::Difficulty);
    Key(game,KEY_SPACE); expect(StartSetupStep::Character);
    Key(game,KEY_Z); expect(StartSetupStep::Shot);
    Key(game,KEY_Z); expect(StartSetupStep::Mode);
    Event(2,KEY_SPACE); Frame(game); Frame(game); Frame(game);
    expect(StartSetupStep::Stage);
    Check(!game.IsLoading(),"held mode confirmation skipped stage selection");
    Event(1,KEY_SPACE); Frame(game);
    Check(Probe::SelectedStage(game)==0,"unavailable saved stage was selected");
    for(const int key : {KEY_W,KEY_UP,KEY_A,KEY_LEFT,KEY_S,KEY_DOWN,KEY_D,KEY_RIGHT}) {
        Key(game,key); expect(StartSetupStep::Stage);
        Check(Probe::SelectedStage(game)==0,"keyboard selected an unavailable stage");
    }
    for(int index=1;index<kStageScoreCount;++index) {
        const auto row=menu::StartSetupLayout(GetScreenWidth(),GetScreenHeight()).StageRow(index);
        Click(game,row.x+row.width/2,row.y+row.height/2); expect(StartSetupStep::Stage);
        Check(Probe::SelectedStage(game)==0,"mouse selected an unavailable stage");
    }
    Probe::Motion(game,1);
    Image before=ReadFrame(game);
    Probe::MotionStep(game,StartSetupStep::Ready,0);
    Image after=ReadFrame(game);
    Check(before.width==after.width && before.height==after.height &&
          std::memcmp(before.data,after.data,before.width*before.height*4)==0,"practice Ready transition changed at zero time");
    UnloadImage(before); UnloadImage(after);
    Probe::MotionStep(game,StartSetupStep::Stage,0);
    for(const Vector2 size : {Vector2{1280,720},{390,844},{900,900},{1600,400},{320,240}}) {
        SetWindowSize(size.x,size.y); Event(7,0,0); Frame(game);
        const std::string dimensions=std::to_string(GetScreenWidth())+"x"+std::to_string(GetScreenHeight());
        Capture(game,(screenshots/(dimensions+"-practice-stage-standard.png")).string().c_str());
        const auto row=menu::StartSetupLayout(GetScreenWidth(),GetScreenHeight()).StageRow(0);
        Click(game,row.x+row.width/2,row.y+row.height/2); expect(StartSetupStep::Ready);
        Check(!game.IsLoading(),"stage row click skipped Ready");
        Capture(game,(screenshots/(dimensions+"-practice-confirm.png")).string().c_str());
        Click(game,1,1,MOUSE_BUTTON_RIGHT); expect(StartSetupStep::Stage);
    }
    SetWindowSize(1280,720); Event(7,0,0); Frame(game);
    Key(game,KEY_X); expect(StartSetupStep::Mode);
    Key(game,KEY_D); Check(Probe::Selection(game).challenge,"practice challenge selection failed");
    Key(game,KEY_SPACE); expect(StartSetupStep::Stage);
    Check(Probe::StageBest(game,false,1)==5176180 && Probe::StageBest(game,true,1)==8754321,"mode records were mixed");
    Capture(game,(screenshots/"practice-stage-challenge.png").string().c_str());
    Key(game,KEY_ENTER); expect(StartSetupStep::Ready);
    const auto button=menu::StartSetupLayout(GetScreenWidth(),GetScreenHeight()).ReadyButton();
    Click(game,button.x+button.width/2,button.y+button.height/2); WaitForStage(game);
    Check(Probe::Selection(game).challenge,"selected practice mode was lost during loading");
    // 每次调用真实结算并重新开局，检查低分、符卡练习和预览不能污染整关记录。
    Probe::FinishScoreAttempt(game,9000000,true,false,false,RunState::GameOver);
    Check(LoadOrCreateSettings(kSettingsFilename).stageHighScores.Best(true,1)==9000000,"challenge GameOver record was not saved");
    const auto attempt=[&](long long score,bool challenge,bool spell,bool preview,RunState result) {
        game.Start(false); WaitForStage(game);
        Probe::FinishScoreAttempt(game,score,challenge,spell,preview,result);
    };
    attempt(6000000,false,false,false,RunState::Won);
    attempt(4000000,false,false,false,RunState::GameOver);
    attempt(99000000,false,true,false,RunState::Won);
    attempt(99000000,true,false,true,RunState::Won);
    auto saved=LoadOrCreateSettings(kSettingsFilename);
    Check(saved.stageHighScores.Best(false,1)==6000000 && saved.stageHighScores.Best(true,1)==9000000,
          "lower, spell or preview score overwrote stage records");
    game.OpenOptions();
    const auto reset=menu::OptionRow(menu::PageCanvas(GetScreenWidth(),GetScreenHeight()),menu::kResetIndex);
    Click(game,reset.x+reset.width/2,reset.y+reset.height/2);
    Check(LoadOrCreateSettings(kSettingsFilename).stageHighScores.scores==saved.stageHighScores.scores,"reset options erased stage records");
    {
        Game restarted;
        Check(Probe::StageBest(restarted,false,1)==6000000 && Probe::StageBest(restarted,true,1)==9000000,
              "stage records did not survive restart");
    }
    SetWindowSize(width,height);
    std::puts("Practice stage flow, locked entries, transitions, mode records, migration and persistence passed.");
}


/// @brief 验证结算快照、精确超时、确认不穿透、暂停冻结与整关练习返回选关的完整生命周期。
void TestStageSettlement(const std::filesystem::path& screenshots) {
    using namespace koumakyou;
    using Probe=GameLifecycleProbe;
    Game game;
    SetTargetFPS(0);
    const auto start=[&](bool practice) {
        game.StartPreview(); Event(7,0,0); Frame(game); Probe::BeginClear(game,practice);
        Check(Probe::Playing(game) && Probe::DialogueTime(game)==0 && Probe::Score(game)==10000,
              "clear report interrupted the start of post-battle dialogue");
        game.Update(0.1F);
        Check(Probe::Playing(game) && Probe::Score(game)==10000,"clear report interrupted post-battle dialogue");
        Probe::FinishPostDialogue(game);
        Check(Probe::State(game)==RunState::StageClear,"clear report did not open");
        Check(Probe::Score(game)==263750,"clear formula or bonus application is incorrect");
    };
    const auto waitForSelection=[&] {
        for(int frame=0;frame<90 && !Probe::Setup(game);++frame)Frame(game);
        Check(Probe::Setup(game) && Probe::SetupStep(game)==StartSetupStep::Stage,"practice clear did not return to Stage");
        Check(Probe::Selection(game).challenge && Probe::Selection(game).shot==1 && Probe::SelectedStage(game)==0,
              "returning to Stage lost run selections");
        Check(Probe::StageReleased(game) && !IsCursorHidden(),"returning to Stage retained resources or captured cursor");
    };
    for(const bool practice : {false,true}) {
        game.StartPreview(); Event(7,0,0); Frame(game); Probe::BeginClear(game,practice);
        Probe::LastPostLine(game); Key(game,KEY_Z);
        Check(Probe::State(game)==RunState::StageClear && Probe::ClearSeconds(game)<0.1F,
              "confirming the final post-battle line also dismissed the clear report");
    }
    start(false);
    const auto time=Probe::StageTime(game), dialogueEnd=Probe::DialogueTime(game);
    game.Update(9.999F);
    Check(Probe::State(game)==RunState::StageClear && Probe::DialogueTime(game)==dialogueEnd && Probe::StageTime(game)==time,
          "clear report did not freeze later events for ten seconds");
    game.Update(0.002F);
    Check(Probe::State(game)==RunState::Finishing,"clear timeout did not continue to stage finishing");
    Check(Probe::Score(game)==263750,"clear timeout awarded bonus twice");
    for(const int key : {KEY_Z,KEY_SPACE,KEY_ENTER}) {
        start(false); Key(game,key);
        Check(Probe::State(game)==RunState::Finishing || Probe::State(game)==RunState::Won,"clear confirmation bypassed stage finishing");
    }
    start(false); Click(game,1,1);
    Check(Probe::State(game)==RunState::Finishing || Probe::State(game)==RunState::Won,"left click did not close just the report");
    start(false); game.Update(3);
    Key(game,KEY_ESCAPE); const float frozen=Probe::ClearSeconds(game);
    game.Update(12);
    Check(Probe::State(game)==RunState::Paused && Probe::ClearSeconds(game)==frozen,"pause did not freeze clear timeout");
    Key(game,KEY_X);
    Check(Probe::State(game)==RunState::StageClear && !IsCursorHidden(),"resuming report captured cursor or lost state");
    start(true);
    game.Update(0.25F);
    const int width=GetScreenWidth(),height=GetScreenHeight();
    for(const Vector2 size : {Vector2{1280,720},{390,844},{900,900},{1600,400},{320,240}}) {
        SetWindowSize(size.x,size.y);
        Capture(game,(screenshots/(std::to_string(GetScreenWidth())+"x"+std::to_string(GetScreenHeight())+"-clear-breakdown.png")).string().c_str(),0);
    }
    SetWindowSize(width,height);
    // 正式入口重入不能重复加奖励或重置倒计时。
    const float elapsed=Probe::ClearSeconds(game);
    Probe::Motion(game,0); Probe::ReenterClear(game);
    Check(Probe::Score(game)==263750 && Probe::ClearSeconds(game)==elapsed,"clear reentry mutated the fixed reward");
    Key(game,KEY_SPACE); waitForSelection();
    start(true); game.Update(10); waitForSelection();
    start(true); Click(game,1,1); waitForSelection();
    // 结算期间的暂停返回和普通练习暂停返回都走同一资源清理入口。
    start(true); Key(game,KEY_ESCAPE);
    const menu::OverlayLayout overlay(GetScreenWidth(),GetScreenHeight());
    const auto back=overlay.PauseRow(3);
    Event(7,back.x+back.width/2,back.y+back.height/2); Frame(game);
    Check(Probe::UiSelection(game)==3,"practice pause hover missed Stage return");
    Click(game,back.x+back.width/2,back.y+back.height/2); waitForSelection();
    Key(game,KEY_Z); Key(game,KEY_SPACE); WaitForStage(game);
    Key(game,KEY_ESCAPE); Key(game,KEY_W);
    Check(Probe::UiSelection(game)==3,"practice pause keyboard did not select Stage return");
    Key(game,KEY_Z); waitForSelection();
    Key(game,KEY_X); Check(Probe::SetupStep(game)==StartSetupStep::Mode,"returned Stage did not backtrack to mode");
    std::puts("Stage clear breakdown, ten-second timeout, confirmation, pause and practice return passed.");
}

}

/// @brief 运行图形/音频集成测试；参数可指定截图目录，需支持 raylib 自动化事件和 OpenGL。
int main(int argc, char** argv) {
    const auto original = std::filesystem::current_path();
    const auto scratch = std::filesystem::temp_directory_path() /
        ("koumakyou_ui_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(scratch);
    const auto screenshots = argc > 1 ? std::filesystem::absolute(argv[1]) : scratch;
    std::filesystem::create_directories(screenshots);
    std::filesystem::current_path(scratch);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNFOCUSED);
    InitWindow(900, 620, "Settings integration test");
    InitAudioDevice();
    SetMasterVolume(0.0F); // 不向用户播放测试音频。
    SetExitKey(KEY_NULL);
    int result = 0;
    try {
        {
            Event(7,83,147);
            koumakyou::Game game;
            Check(GetMousePosition().x==83 && GetMousePosition().y==147,"title startup recentered mouse");
            Check(koumakyou::GameLifecycleProbe::HasLocalizedGlyphs(game),"localized binding label glyph missing");
            Check(koumakyou::GameLifecycleProbe::HasTitleLayers(game),"separate title layer loading failed");
            SetTargetFPS(0);
            // 初始确认被动画门禁忽略；不能把旧等待页替换成一个可提前误触的隐藏菜单。
            Key(game, KEY_Z);
            Check(koumakyou::GameLifecycleProbe::MainMenu(game) && !koumakyou::GameLifecycleProbe::EntranceComplete(game),
                  "startup accepted early confirmation");
            // 菜单已部分显示时仍屏蔽键盘选择、返回和鼠标点击。
            koumakyou::GameLifecycleProbe::StartupAt(game,2.3F);
            const koumakyou::menu::TitleLayout locked(GetScreenWidth(),GetScreenHeight(),koumakyou::menu::kTitleRowCount);
            const auto options=locked.Row(8);
            Key(game,KEY_DOWN); Key(game,KEY_ESCAPE);
            Click(game,options.x+options.width/2,options.y+options.height/2);
            Check(koumakyou::GameLifecycleProbe::MainMenu(game) && koumakyou::GameLifecycleProbe::MenuSelection(game)==0,
                  "incomplete menu accepted input");
            Event(7,0,0); Frame(game);
            constexpr std::array<float,7> times{0,0.275F,0.775F,1.275F,1.85F,2.375F,2.6F};
            constexpr std::array<const char*,7> shots{"startup-00","startup-moon","startup-title","startup-shrine","startup-reimu","startup-menu","title"};
            for(std::size_t index=0;index<times.size();++index) {
                koumakyou::GameLifecycleProbe::StartupAt(game,times[index]);
                Capture(game,(screenshots/(std::string(shots[index])+".png")).string().c_str(),0);
            }
            Key(game,KEY_Z);
            Check(koumakyou::GameLifecycleProbe::Setup(game),"completed startup did not accept confirmation");
            Key(game,KEY_ESCAPE);
            Check(koumakyou::GameLifecycleProbe::MainMenu(game) && koumakyou::GameLifecycleProbe::EntranceComplete(game),
                  "returning to title replayed startup");
            Check(GetMousePosition().x==0 && GetMousePosition().y==0,"returning to title recentered mouse");
            Capture(game,(screenshots / "menu-enter.png").string().c_str(),0);
            Capture(game,(screenshots / "menu-enter-mid.png").string().c_str(),6);
            Capture(game, (screenshots / "menu.png").string().c_str());
            Key(game,KEY_DOWN);
            Capture(game,(screenshots / "menu-selection-mid.png").string().c_str(),0);
            Capture(game,(screenshots / "menu-selection.png").string().c_str());
            Key(game,KEY_UP);
            // 比例变化必须同时移动绘制和鼠标命中；仅保证原画布在窗口内不能算适配。
            constexpr std::array<Vector2, 6> titleSizes{{{1100, 760}, {1280, 720}, {1440, 1080},
                                                       {1920, 820}, {900, 900}, {900, 1400}}};
            for (const auto size : titleSizes) {
                SetWindowSize(static_cast<int>(size.x), static_cast<int>(size.y)); Frame(game);
                const koumakyou::menu::TitleLayout layout(GetScreenWidth(), GetScreenHeight(), koumakyou::menu::kTitleRowCount);
                for (int index = 0; index < layout.rowCount; ++index) {
                    const auto row = layout.Row(index);
                    Check(row.x >= 0 && row.y >= 0 && row.x + row.width <= GetScreenWidth() &&
                          row.y + row.height < (layout.height - 84) * layout.canvas.scale,
                          "responsive title row clipped or entered bottom margin");
                }
                const auto source = layout.BackgroundSource(1536, 1024);
                Check(source.x >= 0 && source.y >= 0 && source.x + source.width <= 1536.01F &&
                      source.y + source.height <= 1024.01F, "title background samples outside texture");
                Check(std::abs(source.width / source.height -
                               GetScreenWidth() / static_cast<float>(GetScreenHeight())) < 0.001F,
                      "title background aspect ratio distorted");
                const auto file = "menu-" + std::to_string(GetScreenWidth()) + "x" + std::to_string(GetScreenHeight()) + ".png";
                Capture(game, (screenshots / file).string().c_str());
                const auto options = layout.Row(8);
                Click(game, static_cast<int>(options.x + options.width / 2), static_cast<int>(options.y + options.height / 2));
                const int fps = koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).fpsLimit;
                Key(game, KEY_RIGHT);
                Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).fpsLimit != fps,
                      "responsive title Options click missed");
                Key(game, KEY_LEFT); Key(game, KEY_ESCAPE);
                Check(koumakyou::GameLifecycleProbe::MenuSelection(game)==8,"returning from options lost title selection");
            }
            SetWindowSize(900, 620); Frame(game);
            const koumakyou::menu::TitleLayout title(GetScreenWidth(), GetScreenHeight(), koumakyou::menu::kTitleRowCount);
            const auto start = title.Row(0);
            Click(game, static_cast<int>(start.x + start.width / 2), static_cast<int>(start.y + start.height / 2));
            Capture(game, (screenshots / "normal.png").string().c_str());
            Key(game, KEY_Z);
            Capture(game, (screenshots / "character.png").string().c_str());
            Key(game, KEY_Z);
            Capture(game, (screenshots / "shot.png").string().c_str());
            Key(game, KEY_Z);
            Capture(game, (screenshots / "mode.png").string().c_str());
            game.OpenOptions();
            Event(7, 0, 0); Frame(game);
            // 数值行点击遵守统一确认：左箭头减小，标签/数值/右箭头增大，行外空白无效。
            for (int width : {900, 1100}) {
                SetWindowSize(width, 620); Frame(game);
                const auto canvas=koumakyou::menu::PageCanvas(GetScreenWidth(),GetScreenHeight());
                for (int row = 0; row < 3; ++row) {
                    const auto bounds = koumakyou::menu::OptionRow(canvas, row);
                    const auto left = koumakyou::menu::OptionArrow(canvas, row, -1);
                    const auto right = koumakyou::menu::OptionArrow(canvas, row, 1);
                    const auto before = koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename);
                    Click(game,bounds.x-1,bounds.y+bounds.height/2);
                    Click(game,bounds.x+bounds.width+1,bounds.y+bounds.height/2);
                    auto after = koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename);
                    Check(after.fpsLimit==before.fpsLimit && after.musicVolume==before.musicVolume &&
                          after.mouseSensitivity==before.mouseSensitivity,"blank click changed setting");
                    for(float x : {bounds.x+20,(left.x+right.x)/2,right.x+10}) {
                        Click(game,x,bounds.y+bounds.height/2);
                        after=koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename);
                        Check(row==0?after.fpsLimit==120:row==1?after.musicVolume==55:after.mouseSensitivity==110,
                              "row confirmation did not increase setting");
                        Click(game,left.x+10,left.y+10);
                        after=koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename);
                        Check(after.fpsLimit==before.fpsLimit && after.musicVolume==before.musicVolume &&
                              after.mouseSensitivity==before.mouseSensitivity,"left arrow did not decrease setting");
                    }
                }
            }
            SetWindowSize(900, 620); Frame(game);
            game.OpenOptions(); Event(7, 0, 0); Frame(game);
            Key(game, KEY_LEFT); Key(game, KEY_LEFT);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).fpsLimit == 30, "FPS lower boundary wrapped");
            for (int i = 0; i < 7; ++i) Key(game, KEY_RIGHT);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).fpsLimit == 0, "FPS upper boundary wrapped");
            game.OpenOptions(); Event(7, 0, 0); Frame(game);
            Key(game, KEY_DOWN);
            Key(game, KEY_RIGHT);
            auto settings = koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename);
            Check(settings.musicVolume == 55, "music volume not saved");
            for (int i = 0; i < 25; ++i) Key(game, KEY_LEFT);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).musicVolume == 0, "mute boundary failed");
            for (int i = 0; i < 25; ++i) Key(game, KEY_RIGHT);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).musicVolume == 100, "volume upper boundary failed");
            Key(game, KEY_DOWN);
            for (int i = 0; i < 35; ++i) Key(game, KEY_RIGHT);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).mouseSensitivity == 300, "sensitivity maximum failed");
            for (int i = 0; i < 35; ++i) Key(game, KEY_LEFT);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).mouseSensitivity == 10, "sensitivity minimum failed");
            for (int i = 0; i < 9; ++i) Key(game, KEY_RIGHT);
            Capture(game, (screenshots / "options.png").string().c_str());
            Key(game, KEY_DOWN); Key(game, KEY_RIGHT);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).soundVolume == 65, "SE volume not saved");
            Key(game, KEY_DOWN); Key(game, KEY_RIGHT);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).initialLives == 3, "stock not saved");
            Key(game, KEY_DOWN); Key(game, KEY_LEFT);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).initialBombs == 2, "bomb count not saved");
            Key(game, KEY_DOWN); Key(game, KEY_Z);
            // 点击 Focus 行打开录入；持续按住的激活点击不能立即变成新绑定。
            const koumakyou::menu::BindingsLayout bindingLayout(GetScreenWidth(),GetScreenHeight());
            const auto focus=bindingLayout.Row(6);
            Event(7,focus.x+focus.width/2,focus.y+focus.height/2); Event(6,MOUSE_BUTTON_LEFT); Frame(game); Frame(game);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).bindings.focus == koumakyou::MouseBinding(MOUSE_BUTTON_RIGHT), "opening click captured");
            Capture(game, (screenshots / "binding-prompt.png").string().c_str());
            Event(5, MOUSE_BUTTON_LEFT); Frame(game);
            Event(6, MOUSE_BUTTON_EXTRA); Frame(game);
            Event(5, MOUSE_BUTTON_EXTRA); Frame(game);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).bindings.focus == koumakyou::MouseBinding(MOUSE_BUTTON_EXTRA), "mouse side binding failed");
            // 将 Focus 改成左键后，射击主键应交换为刚才的侧键，不能保留硬编码左键。
            Event(6, MOUSE_BUTTON_LEFT); Frame(game);
            Event(5, MOUSE_BUTTON_LEFT); Frame(game);
            Event(6, MOUSE_BUTTON_LEFT); Frame(game);
            Event(5, MOUSE_BUTTON_LEFT); Frame(game);
            settings = koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename);
            Check(settings.bindings.focus == koumakyou::MouseBinding(MOUSE_BUTTON_LEFT), "left binding failed");
            Check(settings.bindings.fire == koumakyou::MouseBinding(MOUSE_BUTTON_EXTRA), "mouse conflict swap failed");
            Capture(game, (screenshots / "bindings.png").string().c_str());
            Event(7, 0, 0); Frame(game);
            // Esc 取消不会修改绑定；静止鼠标不应抢走键盘列表选择。
            Key(game, KEY_ENTER); Key(game, KEY_ESCAPE);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).bindings.focus == settings.bindings.focus, "cancel altered binding");
            Key(game, KEY_DOWN); Key(game, KEY_ENTER); Key(game, KEY_F1);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).bindings.fire == KEY_F1, "keyboard rebind failed");
            // 列表只有十个动作，恢复按钮必须紧随暂停；恢复后写入新的单键默认布局。
            for (int i=0;i<3;++i) Key(game,KEY_DOWN);
            Key(game,KEY_ENTER);
            const auto reset=koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).bindings;
            Check(reset.focus == koumakyou::MouseBinding(MOUSE_BUTTON_RIGHT) &&
                  reset.fire == koumakyou::MouseBinding(MOUSE_BUTTON_LEFT) &&
                  reset.bomb == KEY_F && reset.pause == KEY_P, "default binding reset failed");
            Capture(game,(screenshots / "bindings-defaults.png").string().c_str());
            AllPageSizes(game,screenshots);
            game.Start(false); WaitForStage(game); Frame(game);
            // 统一编码必须正确分发到鼠标查询；按住/首次按下分别用于移动和 Bomb。
            Event(6, MOUSE_BUTTON_EXTRA);
            Check(koumakyou::BindingDown(settings.bindings.fire), "mouse held dispatch failed");
            Check(koumakyou::BindingPressed(settings.bindings.fire), "mouse press dispatch failed");
            Frame(game);
            Check(!koumakyou::BindingPressed(settings.bindings.fire), "mouse press repeated while held");
            Event(5, MOUSE_BUTTON_EXTRA); Frame(game);
            Check(!koumakyou::BindingDown(settings.bindings.fire), "mouse release failed");
            // 从捕获鼠标的战斗返回仍须释放光标；取消居中不能变成无法点击首页。
            game.Start(true); WaitForStage(game);
            Check(IsCursorHidden(),"gameplay did not capture cursor");
            game.Reset();
            Check(!IsCursorHidden(),"returning to title left cursor captured");
        }
        // 第二个实例模拟重启：读取配置后仍可加载关卡音乐与进入游戏。
        {
            koumakyou::Game restarted;
            const auto loaded = koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename);
            Check(loaded.musicVolume == 100 && loaded.mouseSensitivity == 100, "restart persistence failed");
            restarted.Start(false); WaitForStage(restarted); Frame(restarted);
        }
        TestStartSelectionMemory(screenshots);
        TestStartAnimations(screenshots);
        TestUnifiedUiInputs();
        TestPracticeStages(screenshots);
        TestStageSettlement(screenshots);
        std::puts("Settings UI integration passed.");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Settings UI integration failed: %s\n", error.what());
        result = 1;
    }
    if (IsAudioDeviceReady()) CloseAudioDevice();
    CloseWindow();
    std::filesystem::current_path(original);
    std::filesystem::remove(scratch / koumakyou::kSettingsFilename);
    if (scratch != screenshots) std::filesystem::remove(scratch);
    return result;
}
