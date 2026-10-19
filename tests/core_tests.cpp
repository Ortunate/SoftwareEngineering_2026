// 本文件负责运行按领域拆分的核心测试。
#include "app/constants.hpp"
#include "app/stage_clear.hpp"
#include "config/settings.hpp"
#include "render/ui_motion.hpp"
#include "render/title_entrance.hpp"
#include "render/menu_layout.hpp"
#include "render/start_setup_layout.hpp"
#include "render/start_setup_motion.hpp"
#include "render/gameplay_layout.hpp"
#include "render/boss_hud.hpp"
#include "render/dialogue_motion.hpp"
#include "resources/dialogue_crop.hpp"
#include <cassert>
#include <cmath>

namespace {
/// @brief 留白、近透明杂点与原裁剪外的实体均不能影响对话锚点；完全透明图片保留清单取景。
void TestDialogueContent() {
    Image image = GenImageColor(100, 120, BLANK);
    ImageDrawPixel(&image, 0, 0, Color{255,255,255,16});
    ImageDrawRectangle(&image, 10, 20, 60, 70, WHITE);
    ImageDrawRectangle(&image, 90, 100, 10, 20, WHITE);
    const auto crop = koumakyou::FindDialogueContent(image, {0,0,0.8F,0.8F});
    assert(std::abs(crop.x - 0.1F) < 0.00001F && std::abs(crop.y - 20.0F/120) < 0.00001F);
    assert(std::abs(crop.width - 0.6F) < 0.00001F && std::abs(crop.height - 70.0F/120) < 0.00001F);
    const auto clipped = koumakyou::FindDialogueContent(image, {0.2F,0.3F,0.3F,0.2F});
    assert(std::abs(clipped.x-0.2F)<0.00001F && std::abs(clipped.width-0.3F)<0.00001F);
    assert(std::abs(clipped.y-0.3F)<0.00001F && std::abs(clipped.height-0.2F)<0.00001F);
    UnloadImage(image);
    image = GenImageColor(10, 10, BLANK);
    const auto empty = koumakyou::FindDialogueContent(image, {0.1F,0.2F,0.5F,0.6F});
    assert(empty.x==0.1F && empty.y==0.2F && empty.width==0.5F && empty.height==0.6F);
    UnloadImage(image);
}
/// @brief 对照原图示例检查 Normal 奖励公式，并覆盖零点数与关卡系数。
void TestStageClearSummary() {
    const koumakyou::StageClearSummary clear{1,84,75,25};
    assert(clear.StagePoints()==1000 && clear.PowerPoints()==8400 && clear.GrazePoints()==750);
    assert(clear.Total()==253750);
    assert((koumakyou::StageClearSummary{6,128,1000,0}.Total()==0));
    assert((koumakyou::StageClearSummary{6,128,1000,1}.Total()==28800));
}
/// @brief 检查从极小窗口到超宽/竖屏的所有交互矩形及 3D 视口都在窗口内，且不会退化为零缩放。
void TestResponsiveLayouts() {
    assert(koumakyou::menu::PauseLabels(false).size()==3 && koumakyou::menu::PauseLabels(true).size()==4);
    for(const Vector2 size : {Vector2{1,1},{320,180},{320,240},{390,844},{900,900},{1600,400},{180,900},{3840,2160}}) {
        const auto fits=[&](Rectangle rect) {
            assert(rect.x>=-0.001F && rect.y>=-0.001F && rect.width>0 && rect.height>0);
            assert(rect.x+rect.width<=size.x+0.001F && rect.y+rect.height<=size.y+0.001F);
        };
        const koumakyou::menu::TitleLayout title(size.x,size.y,koumakyou::menu::kTitleRowCount);
        for(int index=0;index<title.rowCount;++index)fits(title.Row(index));
        const auto logo = title.Logo(2019,779);
        fits(logo);
        assert(std::abs(logo.x + logo.width / 2 - size.x / 2) < 0.001F);
        assert(std::abs(logo.width / logo.height - 2019.0F / 779) < 0.001F);
        assert(logo.y + logo.height < title.Row(0).y || logo.x + logo.width < title.Row(0).x);
        const auto moon = title.MoonFrame(1672,941,logo);
        // 透明画布中心不同于月球中心；缩放后的月球应始终跟随字标中心。
        assert(std::abs(moon.x + moon.width * 0.34F - (logo.x + logo.width / 2)) < 0.001F);
        assert(std::abs(moon.y + moon.height * 0.42F - (logo.y + logo.height / 2)) < 0.001F);
        const auto options=koumakyou::menu::PageCanvas(size.x,size.y);
        const auto full=options.Rect(0,0,options.width,options.height);
        assert(full.x==0 && full.y==0 && std::abs(full.width-size.x)<0.001F && std::abs(full.height-size.y)<0.001F);
        for(int index=0;index<koumakyou::menu::kOptionsCount;++index) {
            fits(koumakyou::menu::OptionRow(options,index));
            if(index<koumakyou::menu::kNumericOptions) {
                fits(koumakyou::menu::OptionArrow(options,index,-1)); fits(koumakyou::menu::OptionArrow(options,index,1));
            }
        }
        const koumakyou::menu::BindingsLayout bindings(size.x,size.y);
        const auto bindingsFull=bindings.canvas.Rect(0,0,bindings.canvas.width,bindings.canvas.height);
        assert(bindingsFull.x==0 && bindingsFull.y==0 && std::abs(bindingsFull.width-size.x)<0.001F && std::abs(bindingsFull.height-size.y)<0.001F);
        for(int index=0;index<static_cast<int>(koumakyou::kBindingNames.size());++index)fits(bindings.Row(index));
        fits(bindings.Button(0)); fits(bindings.Button(1)); fits(bindings.Modal());
        const koumakyou::menu::PageLayout page(size.x,size.y);
        fits(page.Back()); fits(page.Forward());
        const koumakyou::menu::StartSetupLayout setup(size.x,size.y);
        fits(setup.Portrait()); fits(setup.CharacterInfo());
        fits(setup.Badge()); fits(setup.Heading()); fits(setup.ShotSummary()); fits(setup.ModeSummary());
        fits(setup.ReadyBand()); fits(setup.ReadyButton());
        fits(setup.StageHeading()); fits(setup.StageModeSummary()); fits(setup.StageShotSummary());
        for(int index=0;index<koumakyou::kStageScoreCount;++index) {
            fits(setup.StageRow(index));
            assert(!CheckCollisionRecs(setup.StageRow(index),setup.CharacterInfo()));
            assert(!CheckCollisionRecs(setup.StageRow(index),setup.Badge()));
            assert(!CheckCollisionRecs(setup.StageRow(index),setup.StageModeSummary()));
            assert(!CheckCollisionRecs(setup.StageRow(index),setup.ReadyBand()));
        }
        for(int index=0;index<4;++index)fits(setup.Difficulty(index));
        for(int index=0;index<2;++index)fits(setup.Choice(index));
        assert(!CheckCollisionRecs(setup.Portrait(),setup.CharacterInfo()));
        assert(!CheckCollisionRecs(setup.Badge(),setup.ShotSummary()));
        assert(!CheckCollisionRecs(setup.Choice(0),setup.Choice(1)));
        for(int index=0;index<2;++index) {
            const auto r=page.Choice(index);fits(page.canvas.Rect(r.x,r.y,r.width,r.height));
        }
        for(int index=0;index<3;++index)fits(page.Track(index));
        const koumakyou::menu::OverlayLayout overlay(size.x,size.y);
        fits(overlay.PausePanel()); fits(overlay.ClearPanel());
        fits(overlay.PausePanel(static_cast<int>(koumakyou::menu::PauseLabels(true).size())));
        fits(overlay.PauseRow(3));
        for(int index=0;index<koumakyou::menu::kPauseCount;++index)fits(overlay.PauseRow(index));
        for(int index=0;index<koumakyou::menu::kResultCount;++index)fits(overlay.ResultRow(index));
        const koumakyou::GameplayLayout gameplay(size.x,size.y);
        const auto viewport=gameplay.Viewport();
        assert(viewport.x==0 && viewport.y==0 && viewport.width==size.x && viewport.height==size.y);
        fits(gameplay.StagePanel()); fits(gameplay.StatusPanel()); fits(gameplay.BossPanel());
        fits(gameplay.DialoguePanel()); fits(gameplay.SpellBanner());
        const koumakyou::ui::BossHudLayout bossRow(gameplay.BossPanel(),gameplay.scale);
        fits(bossRow.enemy); fits(bossRow.bar); fits(bossRow.timer); fits(bossRow.spell);
        assert(bossRow.enemy.x+bossRow.enemy.width<bossRow.bar.x);
        assert(bossRow.bar.x+bossRow.bar.width<bossRow.timer.x);
        const float center=bossRow.bar.y+bossRow.bar.height/2;
        assert(std::abs(bossRow.enemy.y+bossRow.enemy.height/2-center)<0.001F);
        assert(std::abs(bossRow.timer.y+bossRow.timer.height/2-center)<0.001F);
        assert(bossRow.spell.y>bossRow.enemy.y+bossRow.enemy.height);
        fits(gameplay.Feedback(false)); fits(gameplay.Feedback(true));
        assert(!CheckCollisionRecs(gameplay.StagePanel(),gameplay.StatusPanel()));
        assert(!CheckCollisionRecs(gameplay.BossPanel(),gameplay.StagePanel()));
        assert(!CheckCollisionRecs(gameplay.BossPanel(),gameplay.StatusPanel()));
        for(float aspect : {0.4F,0.7F,1.5F}) {
            for(bool right : {false,true}) {
                const auto portrait=gameplay.DialoguePortrait(aspect,right);
                fits(portrait);
                assert(std::abs(portrait.width/portrait.height-aspect)<0.001F);
                assert(std::abs((right ? portrait.x+portrait.width : portrait.x)-(right ? size.x : 0))<0.001F);
                assert(std::abs(portrait.y+portrait.height-size.y)<0.001F);
                const auto resting=gameplay.DialoguePortrait(aspect,right,0);
                assert(std::abs(resting.width/portrait.width-0.9F)<0.001F);
                assert(resting.y>portrait.y && resting.y+resting.height>size.y);
                assert(right ? resting.x+resting.width>size.x : resting.x<0);
            }
            const auto spell=gameplay.SpellPortrait(aspect);
            fits(spell);
            assert(std::abs(spell.width/spell.height-aspect)<0.001F);
        }
    }
    const koumakyou::menu::Canvas minimized(0,0);
    assert(std::isfinite(minimized.scale) && minimized.scale>0);
}
/// @brief 检查精确的 30% 非发言透明度、切换中间态、快速反转、登场及不同 FPS 的时间一致性。
void TestDialogueMotion() {
    koumakyou::ui::DialogueMotion motion;
    motion.Reset(false,true);
    assert(motion.Sample(false).opacity==1 && motion.Sample(true).opacity==0.3F);
    motion.Advance(true,true,0);
    assert(motion.Sample(false).focus==1 && motion.Sample(true).focus==0);
    motion.Advance(true,true,0.13F);
    assert(std::abs(motion.Sample(false).opacity-0.65F)<0.00001F);
    assert(std::abs(motion.Sample(true).opacity-0.65F)<0.00001F);
    const auto before=motion.Sample(false);
    motion.Advance(false,true,0);
    assert(motion.Sample(false).opacity==before.opacity && motion.Sample(false).focus==before.focus);
    motion.Advance(false,true,0.26F);
    assert(motion.Sample(false).opacity==1 && motion.Sample(true).opacity==0.3F);
    motion.Advance(true,true,0.26F);
    assert(motion.Sample(false).opacity==0.3F && motion.Sample(true).opacity==1);
    motion.Reset(false,false);
    assert(motion.Sample(true).opacity==0);
    motion.Advance(true,true,0);
    assert(motion.Sample(true).opacity==0);
    motion.Advance(true,true,0.13F);
    assert(motion.Sample(true).opacity>0 && motion.Sample(true).opacity<1);
    motion.Advance(true,true,0.13F);
    assert(motion.Sample(true).opacity==1);
    motion.Clear(); motion.Advance(false,false,0);
    assert(motion.Sample(false).opacity==1 && motion.Sample(true).opacity==0);
    koumakyou::ui::DialogueMotion slow, fast;
    slow.Reset(false,true); fast.Reset(false,true);
    for(int frame=0;frame<3;++frame)slow.Advance(true,true,1.0F/30);
    for(int frame=0;frame<24;++frame)fast.Advance(true,true,1.0F/240);
    assert(std::abs(slow.Sample(false).focus-fast.Sample(false).focus)<0.00001F);
    assert(std::abs(slow.Sample(true).opacity-fast.Sample(true).opacity)<0.00001F);
}
/// @brief 检查同一真实时长在 30/240 FPS 下的选择效果一致，以及快速切换不会保留上一页行号。
void TestMenuMotion() {
    koumakyou::ui::MenuMotion slow, fast;
    slow.Advance(0,0,0); fast.Advance(0,0,0);
    for (int frame=0;frame<6;++frame) slow.Advance(0,8,1.0F/30);
    for (int frame=0;frame<48;++frame) fast.Advance(0,8,1.0F/240);
    assert(std::abs(slow.Weight(8)-fast.Weight(8))<0.00001F);
    assert(std::abs(slow.Reveal()-fast.Reveal())<0.00001F);
    assert(slow.Weight(8)>0.95F && slow.Weight(0)<0.05F);
    slow.Advance(0,2,1.0F/240); slow.Advance(0,9,1.0F/240);
    slow.Advance(1,1,0);
    assert(slow.Weight(8)==0 && slow.Weight(9)==0 && slow.Weight(1)==1);
    assert(slow.Reveal()==0);
    slow.Advance(1,1,0.5F);
    assert(slow.Reveal(0.12F)==1);
    slow.Confirm(); slow.Advance(1,1,0.2F);
    assert(slow.Flash()==0);
}
/// @brief 验证错峰、人物连续性及快速反向切换的画面连续；相同真实时长在 30/240 FPS 下保持一致。
void TestStartSetupMotion() {
    using koumakyou::StartSetupStep;
    using koumakyou::ui::StartSetupMotion;
    const int difficulty=static_cast<int>(StartSetupStep::Difficulty),character=static_cast<int>(StartSetupStep::Character);
    StartSetupMotion slow,fast;
    assert(slow.Title()==1 && slow.Setup()==0);
    slow.Advance(difficulty,0.14F);
    assert(slow.Title()>0 && slow.Title()<1);
    assert(slow.CardEntry(0)>slow.CardEntry(1) && slow.CardEntry(2)==0 && slow.CardEntry(3)==0);
    const float previous=slow.Layer(StartSetupStep::Difficulty),title=slow.Title();
    slow.Advance(character,0);
    assert(slow.Layer(StartSetupStep::Difficulty)==previous && slow.Title()==title && slow.Character()==0);
    slow.Advance(character,0.12F);
    const float figure=slow.Character();
    slow.Advance(difficulty,0);
    assert(slow.Character()==figure); // 中途返回不能将已经显现的人物归零。
    slow.Advance(difficulty,1);
    assert(slow.Layer(StartSetupStep::Difficulty)==1);
    for(int index=0;index<4;++index)assert(slow.CardEntry(index)==1);
    slow=StartSetupMotion{};
    for(int frame=0;frame<6;++frame)slow.Advance(character,1.0F/30);
    for(int frame=0;frame<48;++frame)fast.Advance(character,1.0F/240);
    assert(std::abs(slow.Character()-fast.Character())<0.00001F);
    slow.Advance(character,1);
    slow.Advance(static_cast<int>(StartSetupStep::Shot),0.15F);
    assert(slow.Character()==1); // 相邻人物页的权重和恒定，不能重复播放人物入场。
    slow.Advance(static_cast<int>(StartSetupStep::Ready),0.08F);
    float sum=slow.Title();
    for(int step=0;step<static_cast<int>(StartSetupStep::Count);++step)sum+=slow.Layer(static_cast<StartSetupStep>(step));
    assert(std::abs(sum-1)<0.00001F);
    slow.Advance(static_cast<int>(StartSetupStep::Stage),0);
    assert(slow.Character()==1);
    slow.Advance(static_cast<int>(StartSetupStep::Stage),0.15F);
    assert(slow.Layer(StartSetupStep::Stage)>0 && slow.Layer(StartSetupStep::Stage)<1);
    const float stages=slow.Layer(StartSetupStep::Stage);
    slow.Advance(static_cast<int>(StartSetupStep::Ready),0);
    assert(slow.Layer(StartSetupStep::Stage)==stages && slow.Character()==1);
    slow.Advance(-1,1);
    assert(slow.Title()==1 && slow.Setup()==0);
    slow.Advance(character,0.1F);slow.Advance(-2,0);
    assert(slow.Title()==1 && slow.Setup()==0);
}

/// @brief 验证四层及菜单严格顺序、时间推进与帧率无关，菜单完成后才解锁。
void TestTitleEntrance() {
    using koumakyou::ui::TitleElement;
    koumakyou::ui::TitleEntrance entrance;
    entrance.Advance(0.275F);
    assert(entrance.Progress(TitleElement::Moon)>0 && entrance.Progress(TitleElement::Moon)<1);
    assert(entrance.Progress(TitleElement::Title)==0 && entrance.Progress(TitleElement::Shrine)==0);
    entrance.Advance(0.5F);
    assert(entrance.Progress(TitleElement::Moon)==1 && entrance.Progress(TitleElement::Title)>0);
    assert(entrance.Progress(TitleElement::Shrine)==0 && entrance.Progress(TitleElement::Reimu)==0);
    entrance.Advance(0.5F);
    assert(entrance.Progress(TitleElement::Title)==1 && entrance.Progress(TitleElement::Shrine)>0);
    assert(entrance.Progress(TitleElement::Reimu)==0);
    entrance.Advance(0.575F);
    assert(entrance.Progress(TitleElement::Shrine)==1 && entrance.Progress(TitleElement::Reimu)>0);
    assert(entrance.Progress(TitleElement::Menu)==0 && !entrance.Finished());
    entrance.Advance(0.5F);
    assert(entrance.Progress(TitleElement::Reimu)==1 && entrance.Progress(TitleElement::Menu)>0);
    assert(entrance.MenuProgress(0)>entrance.MenuProgress(10) && !entrance.Finished());
    koumakyou::ui::TitleEntrance slow,fast;
    for(int frame=0;frame<54;++frame)slow.Advance(1.0F/30);
    for(int frame=0;frame<432;++frame)fast.Advance(1.0F/240);
    assert(std::abs(slow.Progress(TitleElement::Reimu)-fast.Progress(TitleElement::Reimu))<0.0001F);
    entrance.Complete(); entrance.Advance(-1);
    for(const auto element : {TitleElement::Moon,TitleElement::Title,TitleElement::Shrine,TitleElement::Reimu,TitleElement::Menu})
        assert(entrance.Progress(element)==1);
    assert(entrance.Finished() && entrance.MenuProgress(10)==1);
}
}

/// @brief 运行设置读写和键位校验用例，断言失败时终止进程。
void RunSettingsTests();
/// @brief 运行战斗、弹幕和 Boss 行为用例，断言失败时终止进程。
void RunGameplayTests();
/// @brief 运行道具分布、生成与拾取结算用例，断言失败时终止进程。
void RunDropTests();
/// @brief 运行玩家封装后的移动、射击与受击用例，断言失败时终止进程。
void RunPlayerTests();
/// @brief 运行时间轴及关卡内容用例，断言失败时终止进程。
void RunStageTests();
/// @brief 运行入退场曲线、快照与回收用例，断言失败时终止进程。
void RunActorEffectTests();
/// @brief 运行真实 DDS/ANM、固定帧序及全目录的兼容性验证。
void RunAnimationTests();

int main() {
    assert(koumakyou::ui::BossSeconds(35,0)==35 && koumakyou::ui::BossSeconds(35,0.01F)==34);
    assert(koumakyou::ui::BossSeconds(25,5)==20 && koumakyou::ui::BossSeconds(25,25)==0);
    assert(koumakyou::ui::BossSeconds(25,26)==0 && koumakyou::ui::BossSeconds(-1,0)==-1);
    assert(koumakyou::ui::BossSeconds(150,0)==99);
    assert(koumakyou::ui::BossSeconds(0,0)==0);
    TestDialogueContent();
    TestStageClearSummary();
    TestResponsiveLayouts();
    TestMenuMotion();
    TestStartSetupMotion();
    TestTitleEntrance();
    TestDialogueMotion();
    RunSettingsTests();
    RunGameplayTests();
    RunDropTests();
    RunPlayerTests();
    RunStageTests();
    RunActorEffectTests();
    RunAnimationTests();
    return 0;
}
