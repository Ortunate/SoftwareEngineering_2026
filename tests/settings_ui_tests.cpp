// 本文件以隐藏窗口回放键鼠输入，检查全界面统一操作、比例适配、录入防误触和开局记忆。
#include "boss_fixture.hpp"
#include "app/game.hpp"
#include "app/constants.hpp"
#include "gameplay/bosses/configured_boss.hpp"
#include "config/settings.hpp"
#include "render/menu_layout.hpp"
#include "render/start_setup_layout.hpp"
#include "render/postgame_layout.hpp"
#include "render/spell_practice_layout.hpp"
#include "ui/ui.hpp"
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
        for(const auto& card:spell::Cards())if(!contains(card.name) || !contains(card.user))return false;
        for(const auto& character:spell::Characters())if(!contains(character.name) || !contains(character.title))return false;
        return contains("《》未解锁符卡需以当前组合在关卡内遇见；尚未实现符卡仅供浏览。") && contains("按键设置") && contains("恢复默认键位") && contains("请按下新键") && contains("梦幻夜行绘卷") &&
               contains("请选择主人公。博丽神社的巫女小姐移动速度攻击范围攻击力准备好了吗？面向不擅长STG的人面向大多数人的难度一般街机STG的难度面向有些奇怪的人的难度收手吧！会疯的哦？");
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
        game.selectedCharacter_=0;game.startCharacterMotion_.Advance(0,0,1);
        switch(page) {
            case 0: game.titleEntrance_={}; break;
            case 1: break;
            case 2: game.state_=RunState::Options; break;
            case 3: case 4:
                game.state_=RunState::Options; game.optionsPage_=Game::OptionsPage::KeyBindings;
                game.waitingForBinding_=page==4; break;
            case 5: case 6: case 7: case 8:
                game.state_=RunState::StartSetup;
                game.setupStep_=page==5?StartSetupStep::Difficulty:page==6?StartSetupStep::Shot:page==7?StartSetupStep::Mode:StartSetupStep::Spell;
                if(page==8) {game.practiceMode_=game.spellPractice_=true;game.spellSelection_={};} break;
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
                game.graze_=game.stageGraze_=75; game.pointItems_=game.stagePointItems_=25; game.BeginStageClear(); break;
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
                game.state_=RunState::Playing; game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),3); game.boss_->AdvanceSkill();
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
                game.graze_=game.pointItems_=game.stageGraze_=game.stagePointItems_=999999;
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
    /// @brief 检查实际机体，防止菜单显示魔理沙但战斗仍绑定灵梦。
    static ShotType ActualShot(const Game& game) { return game.player_.Shot(); }
    static RunState State(const Game& game) { return game.state_; }
    /// @brief 读取当前页的局部选项编号，供真实输入回放检查悬停与键盘切换。
    static int UiSelection(const Game& game) {
        if(game.state_==RunState::MainMenu)return game.selectedMenuItem_;
        if(game.state_==RunState::Options)return General(game)?game.selectedOption_:game.selectedBinding_;
        if(game.state_==RunState::MusicRoom)return game.selectedTrack_;
        if(game.state_==RunState::Paused)return game.selectedPauseItem_;
        if(game.state_==RunState::Won || game.state_==RunState::GameOver)return game.selectedResultItem_;
        return game.setupStep_==StartSetupStep::Character?game.selectedCharacter_:game.setupStep_==StartSetupStep::Shot?game.selectedShot_:
            game.setupStep_==StartSetupStep::Mode?int(game.challengeMode_):int(game.practiceAttack_==4);
    }
    static spell::Selection SpellChoice(const Game& game) {return game.spellSelection_;}
    static float SpellPageReveal(const Game& game) {return game.menuMotion_.Reveal();}
    /// @brief 仅截图和隔离测试指定浏览页；实际流程另由键鼠驱动，不替代加载或结果回归。
    static void SpellBrowse(Game& game,spell::Selection choice) {
        game.practiceMode_=game.spellPractice_=true;game.challengeMode_=false;
        game.setupStep_=StartSetupStep::Spell;game.spellSelection_=choice;game.state_=RunState::StartSetup;
    }
    static void SpellTitle(Game& game) {game.EnterMainMenu();game.selectedMenuItem_=3;game.titleEntrance_.Complete();}
    static int BossPhase(const Game& game) {return game.boss_?game.boss_->attack:0;}
    static bool SpellCaptured(const Game& game) {return game.spellCaptured_;}
    static spell::Record SpellRecord(const Game& game,int character,int number) {return game.spellRecords_.Get(character,number);}
    static bool SpellAvailable(const Game& game,int character,int group) {return spell::Available(spell::Find(0,group,1),character,game.spellRecords_);}
    /// @brief 清空临时测试配置的符卡记录，以新档验证门禁，不修改生产存档或伪造解锁记录。
    static void ClearSpellProgress(Game& game) {game.spellRecords_={};game.SaveSettings();}
    /// @brief 隔离已经加载的实际关卡遭遇；保留 previewMode_，通过正式阶段交接记录遇见，不代写 Records。
    static void PrepareStageEncounter(Game& game,bool stagePractice,int shot) {
        game.stage_=StageDirector{};game.practiceMode_=stagePractice;game.spellPractice_=false;
        game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
        game.player_.ConfigureRun(game.initialLives_,game.initialBombs_,game.challengeMode_,static_cast<ShotType>(shot));
        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),3);
    }
    /// @brief 隔离实际整关/Extra的目标遭遇，经过角色技能声明解锁；不直接写入遇见记录。
    static void PrepareCardEncounter(Game& game,const spell::Card& card,int shot) {
        game.stage_=StageDirector{};game.player_.SetInvulnerability(1000);
        game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
        game.player_.ConfigureRun(game.initialLives_,game.initialBombs_,false,static_cast<ShotType>(shot));
        game.player_.SetInvulnerability(1000);
        if(card.stage==6) {
            game.BeginStageBoss(testing::BattleAt(*game.stageContent_,card.number<=121?0:1),3);
            game.boss_->ResumeCombat();
            game.Update(1.0F/60); // 让Game消费首次宣告，119本身就是遭遇首技能，不能在首步前检查遇见记录。
            int frame=0;
            for(;frame<180*60&&game.boss_->SpellCardNumber()!=card.number;++frame) {
                game.Update(1.0F/60);
                if(game.boss_->SpellCardNumber()==card.number)break;
                if(game.boss_->CanTakeDamage()) {
                    // 非符必须走正阈值，HP归零会跳过本阶段符卡；生存符等待真实内部时钟超时，不能只改HUD计时。
                    const auto* skill=dynamic_cast<bosses::ConfiguredBoss&>(*game.boss_).CurrentSkill();
                    game.boss_->health=skill->reserveHealth>0?skill->reserveHealth-1:0;game.ResolveCollisions();
                }
            }
            if(frame>=180*60||!game.spellRecords_.Seen(shot,card.number))throw std::runtime_error("formal Extra encounter did not unlock target card");
            game.ReturnToSpellPractice();return;
        }
        const bool whole=card.stage>=3&&card.stage<=5;
        game.BeginStageBoss(card.stage>=4&&card.group==0?testing::BattleAt(*game.stageContent_,0):
            whole?game.stageContent_->PracticeBoss():game.stageContent_->PracticeBoss(card.number),3);
        if(game.boss_->attack==0)game.boss_->ResumeCombat();
        if(card.stage==5&&card.group>0) {game.Update(1.0F/120);game.boss_->ResumeCombat();}
        if(whole) {
            // 四至六面解锁走真实整关遭遇；六面关底两次恢复分别结束入场与Prelude，不能用单符工厂代替遇见。
            int frame=0;for(;frame<40*60&&game.boss_->SpellCardNumber()!=card.number;++frame) {
                game.Update(1.0F/60);
                if(game.boss_->SpellCardNumber()!=card.number&&game.boss_->CanTakeDamage()) {game.boss_->health=0;game.ResolveCollisions();}
            }
            if(frame>=40*60)throw std::runtime_error("whole stage branch did not declare target card");
        } else for(int attack=1;attack<card.phase;++attack)game.boss_->AdvanceSkill();
        for(int frame=0;frame<3*60;++frame)game.Update(1.0F/60);
        if(game.boss_->SpellCardNumber()!=card.number)throw std::runtime_error("encounter declared another difficulty variant");
        if(!game.spellRecords_.Seen(shot,card.number))throw std::runtime_error("actual stage practice declaration did not unlock card");
        game.ReturnToSpellPractice();
    }
    /// @brief 以真实阶段结算入口分别触发击破与超时；不代写成绩，实际记录由 Game 保存。
    static void FinishSpell(Game& game,bool capture) {
        if(capture)game.boss_->health=0;else game.boss_->skillTimer=game.boss_->TimeLimit();
        game.AdvanceBossSkill();
    }
    /// @brief Extra单符的UI身份使用Extra槽4，战斗内部仍为Normal；不得沿用整关Extra的选存结果。
    static bool ExtraSpellIdentity(const Game& game,int card,int shot) {
        return game.stageId_==StageId::Extra&&game.practiceMode_&&game.spellPractice_&&!game.previewMode_&&
            game.runDifficulty_==native_combat::Difficulty::Normal&&game.ScoreDifficultyIndex()==4&&
            game.boss_&&game.boss_->SpellCardNumber()==card&&game.boss_->attack==1&&int(game.player_.Shot())==shot;
    }
    /// @brief 长时UI回放保护自机，输入、弹幕、实际超时与记录路径照常运行，不伪造符卡完成。
    static void ProtectPlayer(Game& game) {game.player_.SetInvulnerability(1000);}
    static bool SpellPortraits(const Game& game) {const auto& p=game.assets_.Common().spellPortraits;return IsTextureValid(p[0]) && IsTextureValid(p[1]) && p[0].id!=p[1].id;}
    /// @brief 前后层必须分别持有不同姿势的纹理，不能以同一前景图放大冒充后层。
    static bool StartPortraitLayers(const Game& game) {
        const auto& a=game.assets_.Common();
        return SpellPortraits(game) && IsTextureValid(a.reimuPortrait) && IsTextureValid(a.marisaBackdrop) &&
            a.reimuPortrait.id!=a.spellPortraits[0].id && a.marisaBackdrop.id!=a.spellPortraits[1].id;
    }
    static float StartCharacterWeight(const Game& game,int role) {return game.startCharacterMotion_.Weight(role);}
    static float DialogueTime(const Game& game) { return game.dialogueTimer_; }
    /// @brief 使用正式入口定位两位自机的对白保护区间，不让道中时间轴干扰输入回放。
    static void DialogueAt(Game& game,int character,bool after,float seconds) {
        game.stage_=StageDirector{};
        game.player_.ConfigureRun(3,3,false,static_cast<ShotType>(character*2));
        game.state_=RunState::Playing;game.BeginStageDialogue(2,after);game.dialogueTimer_=seconds;
    }
    /// @brief 以真实结算入口检查成绩规则；仅替代漫长的整关计分过程，不绕过存档路径。
    static void FinishScoreAttempt(Game& game,long long score,bool challenge,bool spell,bool preview,RunState result) {
        game.practiceMode_=true; game.spellPractice_=spell; game.previewMode_=preview;
        game.challengeMode_=challenge; game.score_=score; game.EndStage(result);
    }
    static long long StageBest(const Game& game,bool challenge,int stage) { return game.stageHighScores_.Best(challenge,stage); }
    static int SelectedStage(const Game& game) { return game.selectedPracticeStage_; }
    /// @brief 每次结算回归从一面预加载，避免前次连续切关改变下一用例的起点。
    static void StartSettlementPreview(Game& game) {game.stageId_=StageId::Stage1;game.StartPreview();}
    static bool CampaignContinuing(const Game& game) {return game.stageId_==StageId::Stage2&&(game.state_==RunState::Loading||game.state_==RunState::Playing);}
    /// @brief 使用截图示例计数触发正式战后入口，按目标模式恢复真实计分资格。
    static void BeginClear(Game& game,bool practice) {
        game.previewMode_=false; game.practiceMode_=practice; game.spellPractice_=false;
        game.challengeMode_=true; game.selectedShot_=1;
        game.player_.AdjustPower(84-game.player_.Power());
        game.graze_=game.stageGraze_=75; game.pointItems_=game.stagePointItems_=25; game.score_=10000;
        game.BeginStageDialogue(4,true);
    }
    /// @brief 定位对白末尾后推进一个真实 120 Hz 固定步；零秒更新只重采样表现，不执行战斗交接。
    static void FinishPostDialogue(Game& game) { game.dialogueTimer_=game.DialogueDuration(); game.Update(1.0F/120); }
    /// @brief 将战后独白定位到最后一句仍未结束的位置，供真实按键验证层间确认不穿透。
    static void LastPostLine(Game& game) { game.dialogueTimer_=game.DialogueDuration()-2; }
    static void ReenterClear(Game& game) { game.BeginStageClear(); }
    static float ClearSeconds(const Game& game) { return game.stageClearSeconds_; }
    static float StageTime(const Game& game) { return game.stage_.Elapsed(); }
    static long long Score(const Game& game) { return game.score_; }
    static bool StageReleased(const Game& game) { return game.assets_.Stage().visuals.empty() && game.actorEffects_.Pool().ActiveCount()==0; }
    /// @brief 正式Extra入口必须与菜单难度记忆、主线练习和脚本Normal上下文分离。
    static bool ExtraIdentity(const Game& game) {
        return game.stageId_==StageId::Extra&&!game.practiceMode_&&!game.spellPractice_&&!game.previewMode_&&
            game.runDifficulty_==native_combat::Difficulty::Normal&&game.ScoreDifficultyIndex()==4&&
            game.stageContent_&&game.stageContent_->CombatData()->stage==7&&game.referenceRank_==18;
    }
    /// @brief 隔离战斗推进至真实失败收尾，保留实际资源释放和结果页输入处理。
    static void FailExtra(Game& game) {game.score_=1234567;game.EndStage(RunState::GameOver);}
    static bool ExtraScorePending(const Game& game,int shot,bool challenge) {
        return game.state_==RunState::ScoreSave&&game.scoreDifficulty_==4&&game.scoreShot_==shot&&game.scoreMode_==int(challenge)&&
            game.pendingScore_.value==1234567&&!game.pendingEnding_&&!game.stageContent_;
    }


    /// @brief 注入加载失败展示，单独验证退出操作，不触发真实磁盘故障。
    static void FailLoading(Game& game) { game.state_=RunState::Loading; game.loadingFailed_=true; }

    /// @brief 读取实际选择页使用的值，不能以配置文件存在代替界面恢复检查。
    static StartSelection Selection(const Game& game) {
        return {game.selectedShot_,game.challengeMode_,game.practiceAttack_,game.selectedPracticeStage_+1,game.selectedCharacter_,game.selectedDifficulty_};
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
/// @brief 保存当前窗口；settleFrames 可为零，filled 检查页顶背景和贴底横条，防止退回居中留边或漏画提示。
void Capture(koumakyou::Game& game, const char* path, int settleFrames = 24, bool filled = false) {
    for (int frame=0;frame<settleFrames;++frame) Frame(game);
    Image image=ReadFrame(game);
    const Color barBottom=GetImageColor(image,image.width/2,image.height-1);
    Check(barBottom.r>=18 && barBottom.r<=32 && barBottom.g>=8 && barBottom.g<=22 && barBottom.b>=15 && barBottom.b<=30,
          "page is missing the full-width bottom command bar");
    if(filled) {
        const Color top=GetImageColor(image,image.width/2,0),bottom=GetImageColor(image,image.width/2,image.height-1);
        Check(std::abs(top.r-18)<=2 && std::abs(top.g-16)<=2 && std::abs(top.b-19)<=2,
              "secondary page top has letterboxing or wrong template");
        Check(std::abs(bottom.r-22)<=2 && std::abs(bottom.g-10)<=2 && std::abs(bottom.b-17)<=2,
              "secondary page bottom command bar is missing");
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
        Key(game,KEY_DOWN); Key(game,KEY_DOWN); Key(game,KEY_DOWN); next(game,StartSetupStep::Spell);
        Check(!GameLifecycleProbe::Selection(game).challenge,"spell practice inherited hidden challenge mode");
        Key(game,KEY_X);
        Check(GameLifecycleProbe::MenuSelection(game)==3,"returning from spell practice lost title selection");
        Key(game,KEY_UP); Key(game,KEY_UP); Key(game,KEY_UP);
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
        Key(restarted,KEY_DOWN); Key(restarted,KEY_DOWN); Key(restarted,KEY_DOWN); next(restarted,StartSetupStep::Spell);
        const auto spellBefore=GameLifecycleProbe::SpellChoice(restarted);
        Key(restarted,KEY_RIGHT); next(restarted,StartSetupStep::Spell);
        Key(restarted,KEY_DOWN); next(restarted,StartSetupStep::Spell);
        Check(GameLifecycleProbe::SpellChoice(restarted).page==spell::Page::Difficulty,"spell flow skipped difficulty selection");
        Key(restarted,KEY_X);Key(restarted,KEY_X);Key(restarted,KEY_X);
        Check(GameLifecycleProbe::MainMenu(restarted),"spell cancel did not return to title");
        Check(GameLifecycleProbe::MenuSelection(restarted)==3,"spell practice title selection was not retained");
        next(restarted,StartSetupStep::Spell);
        const auto restoredSpell=GameLifecycleProbe::SpellChoice(restarted);
        Check(restoredSpell.character==(spellBefore.character+1)%4,"spell character selection was not restored");
        Check(LoadOrCreateSettings(kSettingsFilename).spellSelection.group==restoredSpell.group,"spell group selection was not saved");

    }
    std::puts("Five-step Start flow, confirmation, backtracking, input and persisted selections passed.");
}

/// @brief 正式Extra键鼠入口覆盖单卡页、四御札/双模式、逐级返回、确认防穿透及独立成绩出口。
/// menuOnly仅验证开局菜单，不进入加载与战斗；供卡片改动的专项快速回归。
void TestExtraStart(const std::filesystem::path& screenshots,bool menuOnly=false) {
    using namespace koumakyou;using Probe=GameLifecycleProbe;
    const int width=GetScreenWidth(),height=GetScreenHeight();
    for(int shot=0;shot<4;++shot)for(bool challenge:{false,true}) {
        auto settings=LoadOrCreateSettings(kSettingsFilename);
        settings.lastSelection.difficulty=3;settings.lastSelection.character=shot/2;
        settings.lastSelection.shot=shot%2;settings.lastSelection.challenge=challenge;
        Check(SaveSettingsFile(kSettingsFilename,settings),"Extra setup seed failed");
        Game game;SetTargetFPS(0);Probe::StartupAt(game,3);Event(7,0,0);Frame(game);
        Key(game,KEY_DOWN);Check(Probe::MenuSelection(game)==1,"Extra skipped by title navigation");
        Key(game,KEY_Z);Check(Probe::SetupStep(game)==StartSetupStep::Difficulty,"Extra card page was skipped");
        for(int key:{KEY_UP,KEY_DOWN,KEY_LEFT,KEY_RIGHT})Key(game,key);
        Check(Probe::SetupStep(game)==StartSetupStep::Difficulty&&Probe::Selection(game).difficulty==3,
              "Extra single-card navigation changed mainline difficulty");
        const auto unused=menu::StartSetupLayout(GetScreenWidth(),GetScreenHeight()).Difficulty(0);
        Click(game,unused.x+unused.width/2,unused.y+unused.height/2);
        Check(Probe::SetupStep(game)==StartSetupStep::Difficulty,"Extra accepted a mainline card region");
        if(shot==0&&!challenge)for(Vector2 size:{Vector2{1280,720},{390,844},{320,240}}) {
            SetWindowSize(size.x,size.y);Frame(game);
            Capture(game,(screenshots/("extra-card-"+std::to_string(int(size.x))+".png")).string().c_str());
        }
        SetWindowSize(width,height);Frame(game);
        Key(game,KEY_X);Check(Probe::MainMenu(game)&&Probe::MenuSelection(game)==1,"Extra card back failed");
        const auto extra=menu::TitleLayout(GetScreenWidth(),GetScreenHeight(),menu::kTitleRowCount).Row(1);
        Click(game,extra.x+extra.width/2,extra.y+extra.height/2);
        Check(Probe::SetupStep(game)==StartSetupStep::Difficulty,"Extra mouse entry skipped card page");
        const auto card=menu::StartSetupLayout(GetScreenWidth(),GetScreenHeight()).ExtraDifficulty();
        Click(game,card.x+card.width/2,card.y+card.height/2);
        Check(Probe::SetupStep(game)==StartSetupStep::Character,"Extra card mouse confirmation failed");
        Key(game,KEY_X);Check(Probe::SetupStep(game)==StartSetupStep::Difficulty,"Extra character back skipped card");
        Key(game,KEY_Z);Check(Probe::SetupStep(game)==StartSetupStep::Character,"Extra card key confirmation failed");
        Key(game,KEY_Z);Key(game,KEY_Z);
        Check(Probe::SetupStep(game)==StartSetupStep::Mode,"Extra shot confirmation failed");
        Event(2,KEY_Z);Frame(game);Frame(game);Frame(game);
        Check(!game.IsLoading()&&Probe::SetupStep(game)==StartSetupStep::Ready,"Extra held confirmation bypassed Ready");
        Event(1,KEY_Z);Frame(game);
        Key(game,KEY_X);Check(Probe::SetupStep(game)==StartSetupStep::Mode,"Extra Ready back entered practice stage");
        Key(game,KEY_Z);
        if(shot==0&&!challenge)for(Vector2 size:{Vector2{1280,720},{390,844},{320,240}}) {
            SetWindowSize(size.x,size.y);Frame(game);
            Capture(game,(screenshots/("extra-ready-"+std::to_string(int(size.x))+".png")).string().c_str());
        }
        SetWindowSize(width,height);Frame(game);
        if(menuOnly) {
            Check(LoadOrCreateSettings(kSettingsFilename).lastSelection.difficulty==3,"Extra card changed difficulty memory");
            continue;
        }
        const auto ready=menu::StartSetupLayout(GetScreenWidth(),GetScreenHeight()).ReadyButton();
        Click(game,ready.x+ready.width/2,ready.y+ready.height/2);WaitForStage(game);
        Check(Probe::ExtraIdentity(game)&&int(Probe::ActualShot(game))==shot&&Probe::Selection(game).challenge==challenge,"Extra run identity mismatch");
        Check(LoadOrCreateSettings(kSettingsFilename).lastSelection.difficulty==3,"Extra overwrote mainline difficulty memory");
        Capture(game,(screenshots/("extra-hud-shot"+std::to_string(shot)+"-mode"+std::to_string(challenge)+".png")).string().c_str());
        Probe::FailExtra(game);
        for(int n=0;n<300&&Probe::State(game)==RunState::Finishing;++n)Frame(game);
        Check(Probe::State(game)==RunState::GameOver&&Probe::StageReleased(game),"Extra failure did not release resources");
        Key(game,KEY_R);WaitForStage(game);Check(Probe::ExtraIdentity(game),"Extra retry changed package identity");
        Probe::FailExtra(game);
        for(int n=0;n<300&&Probe::State(game)==RunState::Finishing;++n)Frame(game);
        if(challenge) {
            const auto row=menu::OverlayLayout(GetScreenWidth(),GetScreenHeight()).ResultRow(1);
            Click(game,row.x+row.width/2,row.y+row.height/2);
        } else Key(game,KEY_ESCAPE);
        Check(Probe::ExtraScorePending(game,shot,challenge),"Extra failure save route lost score identity");
        Key(game,KEY_Z);Check(Probe::State(game)==RunState::ScoreName,"Extra score save did not open naming");
        Key(game,KEY_ESCAPE);Key(game,KEY_ESCAPE);Key(game,KEY_Z);
        Check(Probe::MainMenu(game),"Extra score discard did not return to title");
        game.StartPreview();Check(!Probe::ExtraIdentity(game),"mainline preview inherited Extra identity");
    }
    SetWindowSize(width,height);
    std::puts(menuOnly?"Extra single card, keyboard/mouse, four shots/two modes, return flow and difficulty memory passed.":
        "Extra title/card/character/shot/mode/ready, four shots/two modes, restart, failure/save/discard and preview isolation passed.");
}

/// @brief 导出四难度原作文案和单Extra卡片的横竖屏，确认仅选择卡片后才进入主人公页。
void TestStartCards(const std::filesystem::path& screenshots) {
    using namespace koumakyou;using Probe=GameLifecycleProbe;
    const int width=GetScreenWidth(),height=GetScreenHeight();
    Game game;SetTargetFPS(0);Probe::StartupAt(game,3);Event(7,0,0);Frame(game);Key(game,KEY_Z);
    Check(Probe::HasLocalizedGlyphs(game),"original difficulty descriptions have missing glyphs");
    Check(Probe::SetupStep(game)==StartSetupStep::Difficulty,"Start did not open difficulty cards");
    for(Vector2 size:{Vector2{1280,720},{390,844},{320,240}}) {
        SetWindowSize(size.x,size.y);Frame(game);
        Capture(game,(screenshots/("start-cards-"+std::to_string(int(size.x))+".png")).string().c_str());
    }
    for(int difficulty=0;difficulty<4;++difficulty) {
        const auto card=menu::StartSetupLayout(GetScreenWidth(),GetScreenHeight()).Difficulty(difficulty);
        Click(game,card.x+card.width/2,card.y+card.height/2);
        Check(Probe::SetupStep(game)==StartSetupStep::Character&&Probe::Selection(game).difficulty==difficulty,
              "Start card confirmation changed difficulty");
        Key(game,KEY_X);Check(Probe::SetupStep(game)==StartSetupStep::Difficulty,"Start character back skipped cards");
    }
    SetWindowSize(width,height);TestExtraStart(screenshots,true);
}

/// @brief 回放 Start/Practice Start 双角色选择，核验前后层、箭头、切换动画、记忆及魔理沙实际开局。
void TestStartCharacters(const std::filesystem::path& screenshots) {
    using namespace koumakyou;using Probe=GameLifecycleProbe;
    const int width=GetScreenWidth(),height=GetScreenHeight();
    for(bool practice:{false,true}) {
        auto settings=LoadOrCreateSettings(kSettingsFilename);settings.lastSelection={};
        Check(SaveSettingsFile(kSettingsFilename,settings),"character test settings setup failed");
        Game game;SetTargetFPS(0);Probe::StartupAt(game,3);Event(7,0,0);Frame(game);
        Check(Probe::StartPortraitLayers(game),"Start character foreground/background texture ownership is incorrect");
        if(practice){Key(game,KEY_DOWN);Key(game,KEY_DOWN);}
        Key(game,KEY_Z);Key(game,KEY_SPACE);
        Check(Probe::SetupStep(game)==StartSetupStep::Character,"Start did not enter character step");
        const menu::StartSetupLayout layout(GetScreenWidth(),GetScreenHeight());
        const auto click=[&](Rectangle row) {Click(game,row.x+row.width/2,row.y+row.height/2);};
        for(int key:{KEY_W,KEY_UP,KEY_A,KEY_LEFT,KEY_S,KEY_DOWN,KEY_D,KEY_RIGHT}) {
            const int previous=Probe::Selection(game).character;Key(game,key);
            Check(Probe::Selection(game).character==1-previous && Probe::SetupStep(game)==StartSetupStep::Character,
                  "character navigation alias advanced instead of switching");
        }
        Probe::Motion(game,1);Key(game,KEY_D);
        Check(Probe::StartCharacterWeight(game,0)>0 && Probe::StartCharacterWeight(game,1)>0,
              "character change did not smoothly blend the two portrait layers");
        Capture(game,(screenshots/(practice?"practice-character-transition.png":"start-character-transition.png")).string().c_str(),0);
        click(layout.CharacterArrow(-1));
        Check(Probe::Selection(game).character==0 && Probe::SetupStep(game)==StartSetupStep::Character,"left arrow clicked through character step");
        click(layout.CharacterArrow(1));
        Check(Probe::Selection(game).character==1 && Probe::SetupStep(game)==StartSetupStep::Character,"right arrow clicked through character step");
        // 原姓名按钮区域必须彻底失去交互，不能留下不可见的选择或确认区域。
        for(int role:{0,1}) {
            const auto removed=layout.canvas.Rect((layout.compact?180:55)+role*(layout.compact?210:200),
                layout.compact?660:170,layout.compact?190:185,44);
            Event(7,removed.x+removed.width/2,removed.y+removed.height/2);Frame(game);
            click(removed);
            Check(Probe::Selection(game).character==1 && Probe::SetupStep(game)==StartSetupStep::Character,
                  "removed character name button still responds");
        }
        const auto arrow=layout.CharacterArrow(-1);
        Event(7,arrow.x+arrow.width/2,arrow.y+arrow.height/2);Frame(game);
        Check(Probe::Selection(game).character==1,"arrow hover switched character before clicking");
        click(layout.Portrait());
        Check(Probe::Selection(game).character==1 && Probe::SetupStep(game)==StartSetupStep::Shot,"portrait click did not confirm Marisa");
        Key(game,KEY_D);Check(Probe::Selection(game).shot==1,"Marisa shot choice failed");
        Capture(game,(screenshots/(practice?"practice-marisa-shot.png":"start-marisa-shot.png")).string().c_str());
        Key(game,KEY_Z);Key(game,KEY_Z);if(practice)Key(game,KEY_Z);
        Check(Probe::SetupStep(game)==StartSetupStep::Ready,"Marisa browsing lost mode or stage step");
        Capture(game,(screenshots/(practice?"practice-marisa-ready.png":"start-marisa-ready.png")).string().c_str());
        Key(game,KEY_Z);WaitForStage(game);Frame(game);
        Check(Probe::Playing(game) && Probe::ActualShot(game)==ShotType::IllusionLaser,"Marisa selection did not launch the actual laser shot");
        game.Reset();Key(game,KEY_Z);Key(game,KEY_Z);
        Check(Probe::SetupStep(game)==StartSetupStep::Character,"returning from Marisa battle lost setup flow");
        for(Vector2 size:{Vector2{1280,720},{390,844},{900,900},{1600,400},{320,240}}) {
            SetWindowSize(size.x,size.y);Event(7,0,0);Frame(game);
            for(int role:{0,1}) {
                if(Probe::Selection(game).character!=role)Key(game,KEY_RIGHT);
                Event(7,0,0);Frame(game);
                Capture(game,(screenshots/(std::string(practice?"practice-":"start-")+std::to_string(int(size.x))+"x"+
                    std::to_string(int(size.y))+"-character-"+std::to_string(role)+".png")).string().c_str());
            }
        }
        const auto memory=LoadOrCreateSettings(kSettingsFilename).lastSelection;
        Check(memory.character==1 && memory.shot==1,"character/shot preferences were not saved independently");
        game.Reset();Key(game,KEY_Z);Key(game,KEY_Z);
        Check(Probe::Selection(game).character==1,"title reentry forgot chosen character");
        Game restarted;Probe::StartupAt(restarted,3);Event(7,0,0);Frame(restarted);
        if(practice){Key(restarted,KEY_DOWN);Key(restarted,KEY_DOWN);}
        Key(restarted,KEY_Z);Key(restarted,KEY_Z);
        Check(Probe::Selection(restarted).character==1 && Probe::Selection(restarted).shot==1,"restart lost chosen character/shot");
        SetWindowSize(width,height);
    }
    std::puts("Start/Practice Start two-character layers, arrows, blending, persistence and actual Marisa battle passed.");
}

/// @brief 检查所有页面在小窗口、竖屏、方形、超宽屏和常用宽屏下的呈现，并回放缩放后的行内改键与暂停按钮。
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
                    page>=2 && page<=11);
        }
        koumakyou::GameLifecycleProbe::Show(game,3);
        const koumakyou::menu::BindingsLayout bindings(GetScreenWidth(),GetScreenHeight());
        const auto row=bindings.Row(6);
        Click(game,row.x+row.width/2,row.y+row.height/2);
        Check(koumakyou::GameLifecycleProbe::Waiting(game),"resized binding row missed");
        Key(game,KEY_ESCAPE);
        Key(game,KEY_ESCAPE);
        Check(koumakyou::GameLifecycleProbe::General(game),"resized bindings return key missed");
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
    for(const auto spec : {std::array<int,2>{3,11},{6,2},{7,2},{9,3},{15,3},{16,2},{17,2}}) {
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
        // Debug 在 Quit 后还有 Preview；方向别名应进入实际下一项，不能固定假设 Quit 永远是末项。
        Check(Probe::MenuSelection(game)==10%menu::kTitleRowCount,"title navigation alias failed");
    }
    // 删除的返回项不能保留悬停、点击或导航槽位；横条仅展示，不应把左键当成返回。
    for(const int id:{2,3,9,10}) {
        show(id);
        const auto canvas=menu::PageCanvas(GetScreenWidth(),GetScreenHeight());
        const auto removed=id==2?menu::OptionRow(canvas,menu::kOptionsCount):id==3?
            canvas.Rect(canvas.width<1000?390:560,canvas.height-130,canvas.width<1000?322:300,42):
            canvas.Rect(canvas.width<1000?40:70,canvas.width<1000?1010:675,canvas.width<1000?210:170,42);
        const auto before=Probe::State(game);click(removed);
        Check(Probe::State(game)==before && !Probe::MainMenu(game),"removed return button retained an invisible click area");
        const ui::CommandBar bar(GetScreenWidth(),GetScreenHeight());
        click(bar.Bounds());
        Check(Probe::State(game)==before,"command bar unexpectedly handled a left click");
    }
    show(2);Key(game,KEY_W);Check(Probe::UiSelection(game)==menu::kResetIndex,"Option retained a removed Quit item");
    show(3);move(bindings.Row(0));Key(game,KEY_W);
    Check(Probe::UiSelection(game)==10,"bindings retained a removed return item");
    for(const int key : {KEY_Z,KEY_SPACE,KEY_ENTER}) {
        show(5); Key(game,key); Check(Probe::SetupStep(game)==StartSetupStep::Character,"difficulty confirm alias failed");
        Key(game,key); Check(Probe::SetupStep(game)==StartSetupStep::Shot,"character confirm alias failed");
        Key(game,key); Check(Probe::SetupStep(game)==StartSetupStep::Mode,"shot confirm alias failed");
        Key(game,key); Check(Probe::SetupStep(game)==StartSetupStep::Ready && !game.IsLoading(),"mode confirmation skipped Ready");
        show(15); Key(game,key); Check(Probe::Playing(game),"pause confirmation alias failed");
        show(3); Key(game,key); Check(Probe::Waiting(game),"binding confirmation alias failed");
        Key(game,KEY_X); Check(!Probe::Waiting(game),"binding prompt X did not cancel");
        show(9); Key(game,key); Check(Probe::State(game)==RunState::MusicRoom,"music confirmation left page");
        show(10); Key(game,key); Check(Probe::State(game)==RunState::Score,"score confirmation bypassed return key");
        show(11); Key(game,key); Check(Probe::MainMenu(game),"credit confirmation did not return");
        // 上面返回首页会释放关卡；后续对话和结果渲染需要重新加载预览资源。
        game.StartPreview();
        show(13); const float before=Probe::DialogueTime(game); Key(game,key);
        const float advance=Probe::DialogueTime(game)-before;
        Check(advance>0.1F && advance<10,"dialogue confirmation did not advance exactly one line");
    }
    show(5);
    for(int difficulty=0;difficulty<4;++difficulty) {
        click(setup.Difficulty(difficulty));
        Check(Probe::SetupStep(game)==StartSetupStep::Character&&Probe::Selection(game).difficulty==difficulty,"campaign difficulty click lost identity");
        Key(game,KEY_X);Check(Probe::SetupStep(game)==StartSetupStep::Difficulty,"campaign character back skipped difficulty");
    }
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
    for(const int id : {6,7}) {
        show(id); click(setup.Choice(0));
        Check(Probe::SetupStep(game)==(id==6?StartSetupStep::Mode:StartSetupStep::Ready),"choice click did not advance");
    }
    show(3); click(bindings.Row(0)); Check(Probe::Waiting(game),"binding click did not open prompt");
    const auto saved=LoadOrCreateSettings(kSettingsFilename).bindings;
    click(bindings.Row(0),MOUSE_BUTTON_RIGHT);
    Check(!Probe::Waiting(game) && LoadOrCreateSettings(kSettingsFilename).bindings.forward==saved.forward,
          "prompt right click was captured instead of cancelling");
    show(3);
    Event(2,KEY_SPACE); Frame(game); Frame(game);
    Check(Probe::Waiting(game),"held Space was captured as binding");
    Event(1,KEY_SPACE); Frame(game); Key(game,KEY_ESCAPE);
    show(9); Key(game,KEY_W); Check(Probe::UiSelection(game)==2,"music navigation retained a removed return item");
    Key(game,KEY_SPACE); Check(Probe::State(game)==RunState::MusicRoom,"music playback unexpectedly returned");
    Key(game,KEY_X);Check(Probe::MainMenu(game),"music return key failed");
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
    settings.lastSelection={0,false,2,1}; // 从已登记的一面开始回放六个关卡槽与全部方向别名。
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
    Check(Probe::SelectedStage(game)==0,"saved stage was not restored");
    std::array<bool,kStageScoreCount> visited{};
    for(const int key : {KEY_W,KEY_UP,KEY_A,KEY_LEFT,KEY_W,KEY_UP,KEY_S,KEY_DOWN,KEY_D,KEY_RIGHT,KEY_S,KEY_DOWN}) {
        Key(game,key); expect(StartSetupStep::Stage);
        const int selected=Probe::SelectedStage(game);
        Check(selected>=0&&selected<kStageScoreCount&&kPracticeStages[selected].has_value(),"keyboard selected an unavailable stage");
        visited[selected]=true;
    }
    for(int index=0;index<kStageScoreCount;++index)if(kPracticeStages[index])Check(visited[index],"keyboard skipped an available stage");
    for(int index=1;index<kStageScoreCount;++index) {
        const auto row=menu::StartSetupLayout(GetScreenWidth(),GetScreenHeight()).StageRow(index);
        const int previous=Probe::SelectedStage(game);Click(game,row.x+row.width/2,row.y+row.height/2);
        if(kPracticeStages[index]) {
            expect(StartSetupStep::Ready);Check(Probe::SelectedStage(game)==index,"mouse did not select an available stage");
            Key(game,KEY_X);expect(StartSetupStep::Stage);
        } else {
            expect(StartSetupStep::Stage);Check(Probe::SelectedStage(game)==previous,"mouse selected an unavailable stage");
        }
    }
    const auto firstRow=menu::StartSetupLayout(GetScreenWidth(),GetScreenHeight()).StageRow(0);
    Click(game,firstRow.x+firstRow.width/2,firstRow.y+firstRow.height/2);expect(StartSetupStep::Ready);
    Key(game,KEY_X);expect(StartSetupStep::Stage);
    Probe::Motion(game,1);
    Image before=ReadFrame(game);
    Probe::MotionStep(game,StartSetupStep::Ready,0);
    Image after=ReadFrame(game);
    // 横条立即反映 Ready 的可用操作；零时间连续性只约束正文，不能要求旧页面按键提示继续显示。
    const int contentHeight=ui::CommandBar::ContentHeight(before.width,before.height);
    Check(before.width==after.width && before.height==after.height &&
          std::memcmp(before.data,after.data,before.width*contentHeight*4)==0,"practice Ready content transition changed at zero time");
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
    SetTargetFPS(0); // 临时Game构造会恢复存档FPS；回放继续使用显式固定步，不按显示限帧等待。
    // 新增难度只开放已验收的关卡；真实点击、循环导航与加载后分数写入必须保持同一难度。
    for(const int difficulty:{0,2,3}) {
        game.Reset();Probe::StartupAt(game,3);Event(7,0,0);Frame(game);
        const menu::TitleLayout titleLayout(GetScreenWidth(),GetScreenHeight(),menu::kTitleRowCount);
        const auto practiceRow=titleLayout.Row(2);Click(game,practiceRow.x+practiceRow.width/2,practiceRow.y+practiceRow.height/2);
        expect(StartSetupStep::Difficulty);
        const menu::StartSetupLayout setup(GetScreenWidth(),GetScreenHeight());
        const auto difficultyRow=setup.Difficulty(difficulty);
        Click(game,difficultyRow.x+difficultyRow.width/2,difficultyRow.y+difficultyRow.height/2);
        expect(StartSetupStep::Character);
        Key(game,KEY_Z);Key(game,KEY_Z);Key(game,KEY_Z);expect(StartSetupStep::Stage);
        Check(Probe::Selection(game).difficulty==difficulty,"practice difficulty was not preserved through setup");
        for(int key:{KEY_UP,KEY_DOWN,KEY_LEFT,KEY_RIGHT}) {
            Key(game,key);Check(SupportsStage(*kPracticeStages[Probe::SelectedStage(game)],
                *native_combat::DifficultyFromIndex(difficulty),StageEntry::Practice),"difficulty navigation reached an unimplemented stage");
        }
        const int previous=Probe::SelectedStage(game);const auto sixth=setup.StageRow(5);
        Click(game,sixth.x+sixth.width/2,sixth.y+sixth.height/2);
        if(difficulty==0) {
            expect(StartSetupStep::Stage);Check(Probe::SelectedStage(game)==previous,"Easy stage six accepted mouse confirmation");
        } else {
            expect(StartSetupStep::Ready);Check(Probe::SelectedStage(game)==5,"stage six difficulty did not reach Ready");
            Key(game,KEY_X);expect(StartSetupStep::Stage);
        }
        Capture(game,(screenshots/("practice-difficulty-"+std::to_string(difficulty)+".png")).string().c_str());
        const auto second=setup.StageRow(1);Click(game,second.x+second.width/2,second.y+second.height/2);
        expect(StartSetupStep::Ready);Key(game,KEY_Z);WaitForStage(game);
        Check(Probe::Selection(game).difficulty==difficulty,"loading reset practice difficulty");
        Probe::FinishScoreAttempt(game,100+difficulty,false,false,false,RunState::GameOver);
        const auto scores=LoadOrCreateSettings(kSettingsFilename).stageHighScores;
        Check(scores.Best(false,2,difficulty)==100+difficulty&&scores.Best(false,1)==6000000,
              "difficulty stage score used another difficulty or overwrote Normal");
    }
    SetWindowSize(width,height);
    std::puts("Practice stage flow, four difficulties, locked entries, mode records, migration and persistence passed.");
}


/// @brief 独立回放两位自机的对白按键，验证确认别名、名牌保护及退场保护。
void TestDialogueInputs() {
    using namespace koumakyou;
    using Probe=GameLifecycleProbe;
    Game game;SetTargetFPS(0);game.StartPreview();
    for(int character=0;character<2;++character) {
        const float intro=(character==0?1745.0F:1564.0F)/60;
        for(int key:{KEY_Z,KEY_SPACE,KEY_ENTER}) {
            Probe::DialogueAt(game,character,false,intro+1.0F/60);
            const float locked=Probe::DialogueTime(game);Key(game,key);
            Check(Probe::DialogueTime(game)-locked<0.1F,"dialogue confirmation bypassed native introduction lock");
            Probe::DialogueAt(game,character,false,intro+31.0F/60);
            const float enabled=Probe::DialogueTime(game);Key(game,key);
            Check(Probe::DialogueTime(game)-enabled>1,"dialogue confirmation alias did not advance after native lock");
        }
        Probe::DialogueAt(game,character,false,intro+31.0F/60);
        const float enabled=Probe::DialogueTime(game);Click(game,1,1);
        Check(Probe::DialogueTime(game)-enabled>1,"dialogue click did not advance after native lock");
        Probe::DialogueAt(game,character,false,(character==0?4653.0F:3168.0F)/60);
        const float exit=Probe::DialogueTime(game);Key(game,KEY_Z);
        Check(Probe::Playing(game) && Probe::DialogueTime(game)-exit<0.1F,"dialogue confirmation bypassed native portrait exit");
    }
    std::puts("Native dialogue confirmation aliases, introduction lock and exit lock passed.");
}

/// @brief 验证结算快照、精确超时、确认不穿透、暂停冻结与整关练习返回选关的完整生命周期。
void TestStageSettlement(const std::filesystem::path& screenshots) {
    using namespace koumakyou;
    using Probe=GameLifecycleProbe;
    Game game;
    SetTargetFPS(0);
    const auto start=[&](bool practice) {
        Probe::StartSettlementPreview(game); Event(7,0,0); Frame(game); Probe::BeginClear(game,practice);
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
        Probe::StartSettlementPreview(game); Event(7,0,0); Frame(game); Probe::BeginClear(game,practice);
        Probe::LastPostLine(game); Key(game,KEY_Z);
        Check(Probe::Playing(game),"confirming the final line skipped the native settlement delay");
        for(int frame=0;frame<61 && Probe::Playing(game);++frame)Frame(game);
        Check(Probe::State(game)==RunState::StageClear && Probe::ClearSeconds(game)<0.1F,
              "confirming the final post-battle line also dismissed the clear report");
    }
    start(false);
    const auto time=Probe::StageTime(game), dialogueEnd=Probe::DialogueTime(game);
    game.Update(9.999F);
    Check(Probe::State(game)==RunState::StageClear && Probe::DialogueTime(game)==dialogueEnd && Probe::StageTime(game)==time,
          "clear report did not freeze later events for ten seconds");
    game.Update(0.002F);
    Check(Probe::CampaignContinuing(game),"clear timeout did not load the next stage");
    Check(Probe::Score(game)==263750,"clear timeout awarded bonus twice");
    for(const int key : {KEY_Z,KEY_SPACE,KEY_ENTER}) {
        start(false); Key(game,key);
        Check(Probe::CampaignContinuing(game),"clear confirmation did not load the next stage");
    }
    start(false); Click(game,1,1);
    Check(Probe::CampaignContinuing(game),"left click did not close just the report");
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

/// @brief 不创建 Game，使用默认字体验证组件公共绘制接口的透明度、可见输出与无状态副作用。
void TestUiModuleDrawing(const std::filesystem::path& screenshots) {
    using namespace koumakyou::ui;
    const auto pointer=GetMousePosition();
    MenuMotion motion;motion.Advance(0,0,1);
    const float phase=motion.Phase(),weight=motion.Weight(0);
    const Color background{11,22,33,255};
    const auto target=LoadRenderTexture(320,240);
    const auto draw=[&](float opacity) {
        const DrawContext context{GetFontDefault(),1,opacity,motion.Phase(),motion.Pulse()};
        Modal{{0,0,320,240},{8,8,304,224},{160,14}}.Draw(context,"UI MODULE",WHITE,20);
        Label{{20,40,280,20},18}.Draw(context,"Reusable controls",WHITE);
        Panel{{20,65,280,25},0.1F,1}.Draw(context);
        Button{{20,95,135,32}}.Draw(context,"Button",{weight,1});
        Button{{165,95,135,32},true,ButtonStyle::Text}.Draw(context,"Confirm",{weight,1});
        const auto color=SelectableRow{{20,135,200,30},0}.Draw(context,weight);
        Label{{25,135,190,30},16}.Draw(context,"Selectable row",color);
        ArrowButton{{250,130,44,40},1}.Draw(context);
        ValueSelector{{20,180,280,30},{20,180,32,30},{268,180,32,30}}.DrawArrows(context,1,0,2,{weight,1});
        const CommandBar bar(320,240);
        auto barContext=context;barContext.scale=bar.Scale();
        const std::array<KeyHint,3> hints{{{"W/S","Select"},{"Z","Confirm"},{"X/Esc","Back"}}};
        bar.Draw(barContext,hints);
    };
    BeginTextureMode(target);ClearBackground(background);draw(0);EndTextureMode();
    Image transparent=LoadImageFromTexture(target.texture);
    const auto pixels=LoadImageColors(transparent);
    for(int index=0;index<transparent.width*transparent.height;++index)
        Check(pixels[index].r==background.r && pixels[index].g==background.g && pixels[index].b==background.b,
              "zero-opacity component leaked fill, text or glow");
    UnloadImageColors(pixels);UnloadImage(transparent);
    BeginTextureMode(target);ClearBackground(background);draw(1);EndTextureMode();
    Image visible=LoadImageFromTexture(target.texture);ImageFlipVertical(&visible);
    Check(ExportImage(visible,(screenshots/"ui-components.png").string().c_str()),"UI component preview export failed");
    const auto center=GetImageColor(visible,160,80);
    Check(center.r!=background.r || center.g!=background.g || center.b!=background.b,"UI components did not render");
    UnloadImage(visible);UnloadRenderTexture(target);
    Check(motion.Phase()==phase && motion.Weight(0)==weight && GetMousePosition().x==pointer.x && GetMousePosition().y==pointer.y,
          "component drawing changed motion or pointer");
    std::puts("Independent UI component drawing, zero opacity and read-only presentation passed.");
}

/// @brief 回放同页两栏的键盘递进与鼠标直接跨栏；只浏览和点击未解锁项，不伪造进度或启动战斗。
void TestSpellColumnInputs(koumakyou::Game& game,const std::filesystem::path& screenshots) {
    using namespace koumakyou;using Probe=GameLifecycleProbe;
    const int width=GetScreenWidth(),height=GetScreenHeight();
    for(Vector2 size:{Vector2{1280,720},{390,844},{900,900},{1600,400},{320,240}}) {
        SetWindowSize(size.x,size.y);Event(7,0,0);Frame(game);
        const menu::SpellPracticeLayout layout(GetScreenWidth(),GetScreenHeight());
        const auto move=[&](Rectangle r) {Event(7,r.x+r.width/2,r.y+r.height/2);Frame(game);};
        const auto click=[&](Rectangle r) {Click(game,r.x+r.width/2,r.y+r.height/2);};
        const auto expect=[&](spell::Page focus,int group) {
            const auto choice=Probe::SpellChoice(game);
            Check(Probe::Setup(game) && choice.page==focus && choice.group==group,"same-page spell column focus is incorrect");
            Check(Probe::SpellPageReveal(game)==1,"changing spell column focus replayed page entrance");
        };
        Probe::SpellBrowse(game,{spell::Page::Group,0,1,1,0});Probe::Motion(game,1);
        for(int confirm:{KEY_Z,KEY_SPACE,KEY_ENTER}) {
            Key(game,confirm);expect(spell::Page::Difficulty,1);
            const int previous=Probe::SpellChoice(game).difficulty;
            Key(game,KEY_S);expect(spell::Page::Difficulty,1);
            Check(Probe::SpellChoice(game).difficulty!=previous,"difficulty navigation changed the parent instead of the child");
            Key(game,KEY_X);expect(spell::Page::Group,1);
            Key(game,KEY_W);expect(spell::Page::Group,0);
            Key(game,KEY_DOWN);expect(spell::Page::Group,1);
        }
        move(layout.GroupRow(2));expect(spell::Page::Group,2);
        move(layout.Difficulty(0));expect(spell::Page::Difficulty,2);
        Check(Probe::SpellChoice(game).difficulty==0 && !game.IsLoading(),"hover did not enter child column without confirmation");
        Capture(game,(screenshots/(std::to_string(int(size.x))+"x"+std::to_string(int(size.y))+"-spell-linked-columns.png")).string().c_str());
        Key(game,KEY_X);expect(spell::Page::Group,2);
        Frame(game);expect(spell::Page::Group,2);
        Key(game,KEY_S);expect(spell::Page::Group,0);
        Check(Probe::SpellChoice(game).difficulty==2,"parent change retained a nonexistent difficulty");
        move(layout.Difficulty(1));click(layout.Difficulty(1));expect(spell::Page::Group,0);
        Check(!game.IsLoading(),"empty difficulty slot accepted cross-column confirmation");
        move(layout.GroupRow(1));expect(spell::Page::Group,1);
        move(layout.Difficulty(1));expect(spell::Page::Difficulty,1);
        Key(game,KEY_DOWN);expect(spell::Page::Difficulty,1);
        Check(Probe::SpellChoice(game).difficulty==2,"stationary pointer overrode keyboard difficulty selection");
        Key(game,KEY_ESCAPE);expect(spell::Page::Group,1);
        click(layout.Difficulty(1));expect(spell::Page::Difficulty,1);
        Check(!game.IsLoading(),"direct mouse confirmation bypassed unseen-spell guard");
        move(layout.GroupRow(2));expect(spell::Page::Group,2);
        Click(game,1,1);expect(spell::Page::Group,2);
    }
    SetWindowSize(width,height);Event(7,0,0);Frame(game);
    std::puts("Spell same-page column focus, keyboard progression, direct hover/click, empty slots and five window ratios passed.");
}

/// @brief Extra十三卡经正式遭遇声明解锁，再由键鼠启动；覆盖四御札门禁、翻页、重试、返回和生存取得。
void TestExtraSpellPractice(const std::filesystem::path& screenshots) {
    using namespace koumakyou;using Probe=GameLifecycleProbe;
    Game game;SetTargetFPS(0);Probe::ClearSpellProgress(game);
    const auto memory=LoadOrCreateSettings(kSettingsFilename).lastSelection;
    const menu::SpellPracticeLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto click=[&](Rectangle r) {Click(game,r.x+r.width/2,r.y+r.height/2);};
    for(int shot=0;shot<4;++shot)for(int group=0;group<13;++group) {
        Probe::SpellBrowse(game,{spell::Page::Difficulty,6,group,4,shot});Key(game,KEY_Z);
        Check(Probe::Setup(game)&&!game.IsLoading(),"unseen Extra spell launched");
    }
    Probe::SpellBrowse(game,{spell::Page::Group,6,0,4,0});click(layout.GroupPage(1));
    Check(Probe::SpellChoice(game).group==8,"Extra group next page lost last five cards");
    for(int i=0;i<4;++i)Key(game,KEY_DOWN);
    Check(Probe::SpellChoice(game).group==12,"Extra final spell is unreachable by keyboard");
    Key(game,KEY_Z);Check(Probe::SpellChoice(game).difficulty==4,"Extra difficulty did not stay in slot 4");
    Key(game,KEY_UP);Key(game,KEY_DOWN);
    Check(Probe::SpellChoice(game).difficulty==4,"Extra exposed a mainline difficulty");
    Capture(game,(screenshots/"extra-spell-last-locked.png").string().c_str());
    for(int scenario=0;scenario<16;++scenario) {
        const int number=scenario<13?119+scenario:119;
        const int shot=scenario<13?scenario%4:scenario-12;
        const auto& card=spell::Cards()[number-1];
        game.StartStage(StageId::Extra,false,native_combat::Difficulty::Normal,StageEntry::Extra);WaitForStage(game);
        Probe::PrepareCardEncounter(game,card,shot);
        if(scenario==0)for(int other=1;other<4;++other) {
            Probe::SpellBrowse(game,{spell::Page::Difficulty,6,0,4,other});Key(game,KEY_Z);
            Check(!game.IsLoading()&&Probe::Setup(game),"Extra unlock leaked to another shot");
        }
        const auto before=Probe::SpellRecord(game,shot,number);
        Probe::SpellBrowse(game,{spell::Page::Difficulty,6,card.group,4,shot});
        // UI难度身份为4，但Extra只有第一张可见卡片；命中矩形必须使用可见行0和Extra高度。
        if(scenario%2)click(layout.Difficulty(0,true));else Key(game,KEY_Z);
        Check(game.IsLoading()||Probe::Playing(game),TextFormat("Extra spell %d confirmation did not start loading",number));
        WaitForStage(game);Frame(game);
        Check(Probe::ExtraSpellIdentity(game,number,shot),"Extra practice selected wrong role, card or difficulty");
        Probe::ProtectPlayer(game);
        for(int frame=0;frame<3*60;++frame)Frame(game);
        if(scenario<13)Capture(game,(screenshots/("extra-spell-entry-"+std::to_string(number)+".png")).string().c_str());
        Key(game,KEY_ESCAPE);Key(game,KEY_X);
        Check(Probe::Playing(game),"Extra spell pause did not resume");
        if(number==130) {
            for(int frame=0;frame<90*60&&Probe::Playing(game);++frame)Frame(game);
        } else Probe::FinishSpell(game,true);
        for(int frame=0;frame<90&&Probe::State(game)==RunState::Finishing;++frame)Frame(game);
        Check(Probe::State(game)==RunState::Won&&Probe::SpellCaptured(game),"Extra spell won route entered full-run score flow or lost capture");
        Check(Probe::StageReleased(game),"Extra spell result retained stage resources");
        const auto after=Probe::SpellRecord(game,shot,number);
        Check(after.attempts==before.attempts+1&&after.captured==before.captured+1&&after.mainAttempts==before.mainAttempts,
            "Extra practice changed encounter counts or duplicated capture");
        Key(game,KEY_R);WaitForStage(game);Frame(game);
        Check(Probe::ExtraSpellIdentity(game,number,shot),"Extra retry lost practice identity");
        if(scenario==0||number==131)Capture(game,(screenshots/("extra-spell-retry-best-"+std::to_string(number)+".png")).string().c_str());
        if(scenario==0) {
            Probe::FailExtra(game);
            for(int frame=0;frame<90&&Probe::State(game)==RunState::Finishing;++frame)Frame(game);
            Check(Probe::State(game)==RunState::GameOver,"Extra spell failure did not use practice result");Key(game,KEY_X);
        } else {
            Key(game,KEY_ESCAPE);Key(game,KEY_UP);Key(game,KEY_ENTER);
        }
        const auto choice=Probe::SpellChoice(game);
        Check(Probe::Setup(game)&&choice.page==spell::Page::Group&&choice.stage==6&&choice.group==card.group&&choice.difficulty==4&&choice.character==shot,
            "Extra return lost selection or entered named-score flow");
        Check(Probe::StageReleased(game),"Extra retry/return retained resources");
        const auto saved=LoadOrCreateSettings(kSettingsFilename);
        Check(saved.spellRecords.Get(shot,number).attempts==before.attempts+2&&saved.spellRecords.Get(shot,number).captured==after.captured,
            "Extra retry/abort record did not persist exactly once");
        Check(saved.lastSelection.difficulty==memory.difficulty&&saved.lastSelection.shot==memory.shot,
            "Extra practice overwrote mainline preference");
        std::printf("Extra spell UI %d shot %d formal unlock, capture, retry and return passed.\n",number,shot);std::fflush(stdout);
    }
    game.Reset();
    Game restarted;
    for(int shot=0;shot<4;++shot)Check(Probe::SpellRecord(restarted,shot,119).mainAttempts>0&&Probe::SpellRecord(restarted,shot,119).attempts==2,
        "Extra four-shot record lost on restart");
}

/// @brief 回放关卡页与同页符卡/难度，验证灰色门禁、四组合箭头、暂停返回、取得记录和响应式显示。
void TestSpellPractice(const std::filesystem::path& screenshots) {
    using namespace koumakyou;using Probe=GameLifecycleProbe;
    Game game;SetTargetFPS(0);
    Check(Probe::SpellPortraits(game) && Probe::HasLocalizedGlyphs(game),"spell portraits or complete localized glyphs missing");
    const auto normalMemory=LoadOrCreateSettings(kSettingsFilename).lastSelection;
    const auto click=[&](Rectangle r) {Click(game,r.x+r.width/2,r.y+r.height/2);};
    const auto move=[&](Rectangle r) {Event(7,r.x+r.width/2,r.y+r.height/2);Frame(game);};
    const auto expect=[&](spell::Page page) {Check(Probe::Setup(game) && Probe::SpellChoice(game).page==page,"incorrect spell practice hierarchy");};
    const auto waitForResult=[&] {for(int index=0;index<90 && Probe::State(game)==RunState::Finishing;++index)Frame(game);
        Check(Probe::State(game)==RunState::Won,"spell did not reach its result menu");};
    Probe::ClearSpellProgress(game);
    const menu::SpellPracticeLayout lockedLayout(GetScreenWidth(),GetScreenHeight());
    // 实现状态不代表已解锁；键盘和鼠标都不能从新档直接启动下面两张 Normal 符卡。
    for(int character:{0,1})for(int group:{1,2}) {
        Probe::SpellBrowse(game,{spell::Page::Difficulty,0,group,1,character});
        Key(game,KEY_Z);Key(game,KEY_SPACE);click(lockedLayout.Difficulty(1));expect(spell::Page::Difficulty);
        Check(!game.IsLoading() && !Probe::SpellAvailable(game,character,group),"unseen implemented spell was playable");
    }
    Capture(game,(screenshots/"spell-locked.png").string().c_str());
    TestSpellColumnInputs(game,screenshots);
    game.StartPreview();Probe::PrepareStageEncounter(game,false,0);Probe::FinishSpell(game,true);Frame(game);
    Check(Probe::SpellRecord(game,0,3).mainAttempts==0 && !Probe::SpellAvailable(game,0,1),"preview unlocked spell practice");
    game.Start(false);WaitForStage(game);Probe::PrepareStageEncounter(game,false,0);
    Check(!Probe::SpellAvailable(game,0,1),"nonspell unlocked future spell");
    Probe::FinishSpell(game,true); // 非符交接到夜符，尚未运行下一帧或击破符卡。
    const auto encountered=LoadOrCreateSettings(kSettingsFilename);
    Check(encountered.spellRecords.Get(0,3).mainAttempts==1 && encountered.spellRecords.Get(0,3).mainCaptured==0,
          "spell declaration did not persist encounter immediately");
    Check(Probe::SpellAvailable(game,0,1) && !Probe::SpellAvailable(game,1,1) && !Probe::SpellAvailable(game,0,2),
          "encounter unlocked another shot combination or a later spell");
    game.Reset();
    {
        Game restarted;Check(Probe::SpellAvailable(restarted,0,1) && !Probe::SpellAvailable(restarted,1,1) &&
            !Probe::SpellAvailable(restarted,0,2),"independent encounter unlock lost on restart");
    }
    SetTargetFPS(0); // 临时Game读取设置会改变全局显示限帧，恢复回放速率不改变每步逻辑时间。
    Probe::SpellBrowse(game,{spell::Page::Difficulty,0,1,1,0});
    Key(game,KEY_D);Check(Probe::SpellChoice(game).character==1,"shot switch failed before locked practice check");
    Key(game,KEY_Z);Key(game,KEY_SPACE);click(lockedLayout.Difficulty(1));expect(spell::Page::Difficulty);
    Check(!game.IsLoading(),"switching shot reused another combination's unlock");
    Capture(game,(screenshots/"spell-other-shot-locked.png").string().c_str());
    game.Start(false);WaitForStage(game);Probe::PrepareStageEncounter(game,true,1);
    Probe::FinishSpell(game,true);Probe::FinishSpell(game,false);Probe::FinishSpell(game,true);
    Check(Probe::BossPhase(game)==4 && !Probe::SpellAvailable(game,0,2) && Probe::SpellAvailable(game,1,2) &&
          Probe::SpellRecord(game,1,7).mainCaptured==0,
          "whole-stage encounter did not independently unlock the selected combination");
    game.Reset();
    Probe::SpellBrowse(game,{});Probe::SpellTitle(game);Key(game,KEY_Z);expect(spell::Page::Stage);
    const menu::SpellPracticeLayout layout(GetScreenWidth(),GetScreenHeight());
    for(int key:{KEY_A,KEY_LEFT,KEY_D,KEY_RIGHT}) {
        const auto before=Probe::SpellChoice(game);
        Key(game,key);
        Check(Probe::SpellChoice(game).character==(before.character+(key==KEY_A || key==KEY_LEFT?3:1))%4,"character direction alias failed");
        expect(spell::Page::Stage);
    }
    for(int character=0;character<4;++character) {
        const auto previous=Probe::SpellChoice(game).character;
        click(layout.Arrow(1));
        Check(Probe::SpellChoice(game).character==(previous+1)%4,"portrait right arrow failed or clicked through");
        click(layout.Arrow(-1));
        Check(Probe::SpellChoice(game).character==previous,"portrait left arrow failed");
        Key(game,KEY_D);
    }
    for(int key:{KEY_W,KEY_UP,KEY_S,KEY_DOWN}) {
        const int previous=Probe::SpellChoice(game).stage;
        Key(game,key);
        Check(Probe::SpellChoice(game).stage==(previous+(key==KEY_W || key==KEY_UP?6:1))%7,"stage navigation alias failed");
    }
    move(layout.StageRow(1));Check(Probe::SpellChoice(game).stage==1,"disabled stage could not be browsed by hover");
    click(layout.StageRow(1));expect(spell::Page::Group);
    Key(game,KEY_SPACE);expect(spell::Page::Difficulty);
    Key(game,KEY_ENTER);expect(spell::Page::Difficulty);
    Check(!game.IsLoading(),"unimplemented spell started a replacement battle");
    Key(game,KEY_X);expect(spell::Page::Group);Key(game,KEY_ESCAPE);expect(spell::Page::Stage);
    click(layout.StageRow(3));expect(spell::Page::Group);
    for(int index=0;index<9;++index)Key(game,KEY_S);
    Check(Probe::SpellChoice(game).group==9,"16-group stage lost entries beyond first page");
    click(layout.GroupPage(-1));Check(Probe::SpellChoice(game).group==1,"group previous page failed");
    click(layout.GroupPage(1));Check(Probe::SpellChoice(game).group==9,"group next page failed");
    Key(game,KEY_Z);expect(spell::Page::Difficulty);
    click(layout.GroupPage(-1));expect(spell::Page::Group);
    Check(Probe::SpellChoice(game).group==1,"visible group paging did not work from difficulty view");
    click(layout.GroupPage(1));
    Key(game,KEY_X);click(layout.StageRow(6));Key(game,KEY_Z);expect(spell::Page::Difficulty);
    Check(Probe::SpellChoice(game).difficulty==4,"Extra exposed ordinary difficulty slots");
    Key(game,KEY_Z);Check(!game.IsLoading(),"unseen Extra spell bypassed encounter guard");
    Key(game,KEY_X);Key(game,KEY_X);expect(spell::Page::Stage);
    click(layout.StageRow(0));expect(spell::Page::Group);
    // 新关卡从第一组开始；月符只有 Hard/Lunatic，第二组 Normal 对应实际夜符。
    move(layout.GroupRow(1));expect(spell::Page::Group);
    click(layout.Arrow(1));expect(spell::Page::Group);
    Check(Probe::SpellChoice(game).character==1,"difficulty arrow advanced the page");
    click(layout.Arrow(-1));
    const auto recordBefore=Probe::SpellRecord(game,0,3);
    for(int index=0;index<4;++index) {
        if(Probe::SpellChoice(game).character==0)break;
        Key(game,KEY_D);
    }
    // 只悬停父项后直接点击难度启动，不能要求先点击符卡或按一次确认。
    click(layout.Difficulty(1));WaitForStage(game);Frame(game);
    Check(Probe::BossPhase(game)==2 && !Probe::Selection(game).challenge,"Normal Night Bird did not start correct phase");
    Key(game,KEY_ESCAPE);
    Capture(game,(screenshots/"spell-pause.png").string().c_str());
    const menu::OverlayLayout overlay(GetScreenWidth(),GetScreenHeight());
    click(overlay.PauseRow(3));expect(spell::Page::Group);
    Check(Probe::StageReleased(game) && !IsCursorHidden(),"spell pause return leaked resources or captured cursor");
    Key(game,KEY_Z);Key(game,KEY_Z);WaitForStage(game);Frame(game);
    Probe::FinishSpell(game,true);waitForResult();
    Check(Probe::SpellCaptured(game) && Probe::UiSelection(game)==2,"captured spell result or default Retry is incorrect");
    Capture(game,(screenshots/"spell-result.png").string().c_str());
    const auto captured=Probe::SpellRecord(game,0,3);
    Check(captured.attempts==recordBefore.attempts+2 && captured.captured==recordBefore.captured+1 && captured.bestScore>0,
          "spell attempt, aborted run, capture or best score was not recorded once");
    move(overlay.SpellResultRow(1));click(overlay.SpellResultRow(1));
    Check(Probe::State(game)==RunState::Won && Probe::UiSelection(game)==2,"disabled Save Replay accepted interaction");
    Key(game,KEY_S);Check(Probe::UiSelection(game)==0,"spell result keyboard navigation did not skip Replay");
    move(overlay.SpellResultRow(2));Check(Probe::UiSelection(game)==2,"spell result hover missed pause-style row");
    click(overlay.SpellResultRow(0));expect(spell::Page::Group);
    Key(game,KEY_Z);Key(game,KEY_Z);WaitForStage(game);Frame(game);
    Probe::FinishSpell(game,false);waitForResult();
    Check(!Probe::SpellCaptured(game) && Probe::SpellRecord(game,0,3).captured==captured.captured,"timeout was counted as capture");
    Capture(game,(screenshots/"spell-failed.png").string().c_str());
    const int resultWidth=GetScreenWidth(),resultHeight=GetScreenHeight();
    SetWindowSize(390,844);Event(7,0,0);Frame(game);
    Capture(game,(screenshots/"390x844-spell-failed.png").string().c_str());
    SetWindowSize(resultWidth,resultHeight);Event(7,0,0);Frame(game);
    Key(game,KEY_X);expect(spell::Page::Group);
    Key(game,KEY_X);Key(game,KEY_X);Check(Probe::MainMenu(game),"spell backtracking did not reach title");
    const auto saved=LoadOrCreateSettings(kSettingsFilename);
    Check(saved.lastSelection.shot==normalMemory.shot && saved.lastSelection.challenge==normalMemory.challenge,
          "spell browsing overwrote normal Start preferences");
    Check(saved.spellRecords.Get(0,3).captured==captured.captured,"spell statistics did not persist");
    {
        Game restarted;Check(Probe::SpellRecord(restarted,0,3).captured==captured.captured,"spell records lost on restart");
    }
    SetTargetFPS(0);
    // 第二种灵梦御札必须接到暗符阶段；未遇见的魔理沙符卡仍必须被独立记录门禁阻止。
    Probe::SpellBrowse(game,{spell::Page::Group,0,2,1,1});
    Key(game,KEY_Z);Key(game,KEY_Z);WaitForStage(game);Frame(game);
    Check(Probe::BossPhase(game)==4 && Probe::Selection(game).shot==1,"Reimu B Demarcation mapped to a different phase or shot");
    const auto darkBefore=Probe::SpellRecord(game,1,7);
    Probe::FinishSpell(game,true);waitForResult();Key(game,KEY_X);
    Check(Probe::SpellRecord(game,1,7).captured==darkBefore.captured+1,"Reimu B capture was not isolated");
    for(int character:{2,3}) {
        Probe::SpellBrowse(game,{spell::Page::Difficulty,0,1,1,character});
        Key(game,KEY_Z);expect(spell::Page::Difficulty);
        Check(!game.IsLoading(),"unseen Marisa combination bypassed its own encounter record");
    }
    // 新增入口经过实际整关声明、键盘确认和暂停返回，覆盖道中、Easy 编号缩短、二面难度换卡与末符。
    for(const auto [number,shot]:std::array<std::pair<int,int>,45>{{
        {1,1},{6,2},{13,1},{14,2},{20,0},{21,1},{24,0},{25,1},{29,1},{31,3},{34,2},{37,1},
        {38,0},{44,2},{48,1},{55,1},{62,0},{65,3},{69,0},{73,2},{77,1},{80,0},{84,3},
        {85,0},{88,2},{89,1},{91,2},{92,3},{93,2},{95,0},{96,0},{97,3},{99,1},{100,1},
        {101,0},{103,3},{104,1},{105,2},{108,0},{110,3},{111,1},{114,2},{116,0},{117,3},{118,1},
    }}) {
        const auto& card=spell::Cards()[number-1];
        const auto difficulty=*native_combat::DifficultyFromIndex(card.difficulty);
        game.StartStage(*kPracticeStages[card.stage],false,difficulty,StageEntry::Practice);WaitForStage(game);
        Probe::PrepareCardEncounter(game,card,shot);
        Probe::SpellBrowse(game,{spell::Page::Difficulty,card.stage,card.group,card.difficulty,shot});
        Key(game,KEY_Z);WaitForStage(game);
        for(int frame=0;frame<3*60;++frame)Frame(game);
        Check(Probe::BossPhase(game)==card.phase&&int(Probe::ActualShot(game))==shot,"difficulty card launched another encounter or shot");
        Capture(game,(screenshots/("spell-entry-"+std::to_string(number)+".png")).string().c_str());
        Key(game,KEY_ESCAPE);Key(game,KEY_X);
        // 恢复后再经暂停菜单第四项回到选卡；不能借助重置掩盖资源释放或选择丢失。
        Key(game,KEY_ESCAPE);Key(game,KEY_UP);Key(game,KEY_ENTER);
        expect(spell::Page::Group);
        const auto choice=Probe::SpellChoice(game);
        Check(choice.stage==card.stage&&choice.group==card.group&&choice.difficulty==card.difficulty&&choice.character==shot,
              "pause return lost difficulty card selection");
        Check(Probe::StageReleased(game),"spell pause return kept borrowed stage resources");
    }
    const int width=GetScreenWidth(),height=GetScreenHeight();
    for(Vector2 size:{Vector2{1280,720},{390,844},{900,900},{1600,400},{320,240}}) {
        SetWindowSize(size.x,size.y);Event(7,0,0);Frame(game);
        for(int character=0;character<4;++character) {
            if(character!=0 && size.x!=1280)continue;
            Probe::SpellBrowse(game,{spell::Page::Stage,0,1,1,character});
            Capture(game,(screenshots/(std::to_string(int(size.x))+"x"+std::to_string(int(size.y))+"-spell-character-"+std::to_string(character)+".png")).string().c_str());
        }
        for(int stage:{0,3,6}) {
            Probe::SpellBrowse(game,{spell::Page::Difficulty,stage,stage==3?15:stage==6?12:1,stage==6?4:1,0});
            Capture(game,(screenshots/(std::to_string(int(size.x))+"x"+std::to_string(int(size.y))+"-spell-stage-"+std::to_string(stage)+".png")).string().c_str());
        }
    }
    SetWindowSize(width,height);
    std::puts("Spell Practice hierarchy, 131 variants, four combinations, arrows, unavailable guards, statistics and results passed.");
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
        TestUiModuleDrawing(screenshots);
        if(argc>2 && std::string(argv[2])=="--components-only") {
            // 组件绘制可单独回归，不初始化 Game 或加载生产素材。
        } else if(argc>2 && std::string(argv[2])=="--start-characters-only") {
            TestStartCharacters(screenshots);
        } else if(argc>2 && std::string(argv[2])=="--start-memory-only") {
            TestStartSelectionMemory(screenshots);
        } else if(argc>2 && std::string(argv[2])=="--start-cards-only") {
            TestStartCards(screenshots);
        } else if(argc>2 && std::string(argv[2])=="--extra-only") {
            TestExtraStart(screenshots);
        } else if(argc>2 && std::string(argv[2])=="--inputs-only") {
            TestUnifiedUiInputs();
        } else if(argc>2 && std::string(argv[2])=="--practice-only") {
            TestPracticeStages(screenshots);
        } else if(argc>2 && std::string(argv[2])=="--spell-only") {
            TestSpellPractice(screenshots);
        } else if(argc>2 && std::string(argv[2])=="--extra-spells-only") {
            TestExtraSpellPractice(screenshots);
        } else if(argc>2 && std::string(argv[2])=="--dialogue-only") {
            TestDialogueInputs();
            TestStageSettlement(screenshots);
        } else {
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
                const Vector2 optionsPointer{static_cast<float>(static_cast<int>(options.x+options.width/2)),
                                             static_cast<float>(static_cast<int>(options.y+options.height/2))};
                Click(game, optionsPointer.x, optionsPointer.y);
                Check(GetMousePosition().x==optionsPointer.x && GetMousePosition().y==optionsPointer.y,
                      "entering Options moved mouse away from title selection");
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
            const auto pointerBeforeOptions=GetMousePosition();
            game.OpenOptions();
            Check(GetMousePosition().x==pointerBeforeOptions.x && GetMousePosition().y==pointerBeforeOptions.y,
                  "opening Options from setup reset mouse position");
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
            Key(game, KEY_DOWN); Key(game, KEY_LEFT);
            Check(!koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).aimAssistEnabled,"assist off not saved");
            Key(game, KEY_RIGHT);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).aimAssistEnabled,"assist on not saved");
            Key(game, KEY_DOWN); Key(game, KEY_RIGHT);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).soundVolume == 65, "SE volume not saved");
            Key(game, KEY_DOWN); Key(game, KEY_RIGHT);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).initialLives == 3, "stock not saved");
            Key(game, KEY_DOWN); Key(game, KEY_LEFT);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).initialBombs == 2, "bomb count not saved");
            Key(game, KEY_DOWN); Key(game, KEY_Z);
            // 点击 Focus 行进入行内录入；持续按住的激活点击不能立即变成新绑定。
            const koumakyou::menu::BindingsLayout bindingLayout(GetScreenWidth(),GetScreenHeight());
            const auto focus=bindingLayout.Row(6);
            Event(7,focus.x+focus.width/2,focus.y+focus.height/2); Event(6,MOUSE_BUTTON_LEFT); Frame(game); Frame(game);
            Check(koumakyou::LoadOrCreateSettings(koumakyou::kSettingsFilename).bindings.focus == koumakyou::MouseBinding(MOUSE_BUTTON_RIGHT), "opening click captured");
            Capture(game, (screenshots / "binding-prompt.png").string().c_str());
            Event(5, MOUSE_BUTTON_LEFT); Frame(game);
            Capture(game, (screenshots / "binding-ready.png").string().c_str(),0,true);
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
            // 恢复默认作为十个动作后的列表末行；恢复后写入新的单键默认布局。
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
        TestSpellPractice(screenshots);
        TestExtraSpellPractice(screenshots);
        TestStartCharacters(screenshots);
        TestExtraStart(screenshots);
        }
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
