// 本文件负责运行按领域拆分的核心测试。
#include "app/constants.hpp"
#include "app/stage_clear.hpp"
#include "stage/catalog.hpp"
#include "render/spell_practice_layout.hpp"
#include "config/settings.hpp"
#include "ui/motion.hpp"
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
#include <string_view>

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
/// @brief 校验完整新典目录的原始编号、七关总数、难度空槽及独立成绩，避免只补列表外观。
void TestSpellPracticeCatalog() {
    using namespace koumakyou;
    constexpr std::array<int,7> totals{9,11,17,47,16,18,13},groups{3,3,5,16,4,6,13};
    assert(spell::Cards().size()==131 && spell::Characters().size()==4);
    std::array<int,7> counts{};int playable=0;
    for(std::size_t index=0;index<spell::Cards().size();++index) {
        const auto& card=spell::Cards()[index];assert(card.number==int(index)+1);
        assert(card.power==(index<2?32:index<9?64:128));
        assert(card.group>=0 && card.group<groups[card.stage] && card.difficulty>=0 && card.difficulty<5 && (card.stage==6?card.difficulty==4:card.difficulty<4));
        assert(spell::Find(card.stage,card.group,card.difficulty)==&card);++counts[card.stage];
        playable+=spell::Playable(&card,0);
        assert(spell::Playable(&card,0)==spell::Playable(&card,1));
        assert(spell::Playable(&card,0)==spell::Playable(&card,2));
        assert(spell::Playable(&card,0)==spell::Playable(&card,3));
    }
    assert(counts==totals && playable==131);
    // UI 归属独立于启动关卡；逐项覆盖防止跨关卡归并后漏卡或重复显示。
    constexpr std::array<int,7> bossGroups{3,3,5,19,5,5,10},bossTotals{9,11,17,50,19,15,10};
    std::array<int,131> coverage{};
    assert(spell::Bosses().size()==7);
    for(int boss=0;boss<7;++boss) {
        const auto& definition=spell::Bosses()[boss];int total=0;
        assert(static_cast<int>(definition.groups.size())==bossGroups[boss]);
        for(int index=0;index<bossGroups[boss];++index) {
            const auto entry=definition.groups[index];
            assert(spell::BossIndex(entry.stage,entry.group)==boss);
            assert(spell::BossGroupIndex(entry.stage,entry.group)==index);
            for(int difficulty=0;difficulty<5;++difficulty)if(const auto* card=spell::Find(entry.stage,entry.group,difficulty)) {
                assert(std::string_view(card->user)==definition.name);
                ++coverage[card->number-1];++total;
            }
        }
        assert(total==bossTotals[boss]);
    }
    for(int count:coverage)assert(count==1);
    assert(spell::BossIndex(5,0)==4 && spell::BossGroupIndex(5,0)==4);
    assert(spell::BossIndex(6,0)==3 && spell::BossGroupIndex(6,0)==16);
    assert(spell::BossIndex(6,3)==6 && spell::BossGroupIndex(6,3)==0);
    assert(spell::BossIndex(-1,0)==-1 && spell::BossGroupIndex(6,13)==-1);
    assert(!SpellPracticeStage(-1)&&!SpellPracticeStage(7)&&SpellPracticeStage(6)==StageId::Extra);
    for(int stage=0;stage<6;++stage)assert(SpellPracticeStage(stage)==kPracticeStages[stage]);
    auto invalidExtra=*spell::Find(6,0,4);
    for(int difficulty=0;difficulty<4;++difficulty) {invalidExtra.difficulty=difficulty;assert(!spell::Playable(&invalidExtra,0));}
    assert(!spell::Find(0,0,0) && !spell::Find(0,0,1));
    assert(spell::Find(0,1,1)->number==3 && spell::Find(0,2,1)->number==7);
    assert(spell::Stages()[3].groups==16 && spell::Find(6,12,4)->number==131);
    spell::Records records;
    const auto* night=spell::Find(0,1,1);const auto* dark=spell::Find(0,2,1);
    assert(!spell::Available(night,0,records) && !records.Seen(0,3));
    records.Begin(0,3,true);records.Finish(0,3,true,true,1000);
    records.Begin(0,3,true);records.Finish(0,3,true,false,500);
    assert(!records.Seen(0,3) && !spell::Available(night,0,records));
    records.Begin(1,7,false);records.Finish(1,7,false,true,2000);
    assert(records.Seen(1,7) && !spell::Available(dark,0,records) && spell::Available(dark,1,records));
    assert(!spell::Available(night,0,records) && !spell::Available(dark,2,records));
    records.Begin(1,3,false); // 遇见即解锁，不要求取得、最高分或结束这次尝试。
    assert(!spell::Available(night,0,records) && spell::Available(night,1,records));
    assert(!records.Seen(1,4) && !records.Seen(1,0) && !records.Seen(1,132));
    records.Begin(0,4,false);
    assert(records.Seen(0,4) && spell::Available(spell::Find(0,1,2),0,records));
    assert(!spell::Available(nullptr,0,records) && !spell::Available(night,-1,records));
    assert(records.Get(0,3).attempts==2 && records.Get(0,3).captured==1 && records.Get(0,3).bestScore==1000);
    assert(records.Get(1,3).attempts==0 && records.Get(1,7).mainCaptured==1 && records.Get(1,7).bestScore==0);
    assert(records.Get(-1,0).bestScore==0);
    // 追加三符使用独立遭遇容量；其记录既不能越界写入，也不能凭空生成练习入口。
    for(int character=0;character<4;++character)for(int number=132;number<=134;++number) {
        records.Begin(character,number,true);records.Finish(character,number,true,true,1234);
        assert(!records.Seen(character,number)&&records.Get(character,number).attempts==0&&records.Get(character,number).bestScore==0);
        records.Begin(character,number,false);records.Finish(character,number,false,true,1234);
        assert(records.Seen(character,number)&&records.Get(character,number).mainCaptured==1);
        assert(records.Get(character,number).attempts==0&&records.Get(character,number).bestScore==0);
    }
    records.Begin(0,135,false);assert(!records.Seen(0,135)&&!spell::Find(6,13,4));
    for(int character=0;character<4;++character) {
        spell::Records extraRecords;
        for(int card=119;card<=131;++card) {
            const auto* entry=spell::Find(6,card-119,4);assert(entry&&entry->phase==1);
            extraRecords.Begin(character,card,true);assert(!spell::Available(entry,character,extraRecords));
            extraRecords.Begin(character,card,false);
            for(int selected=0;selected<4;++selected)assert(spell::Available(entry,selected,extraRecords)==(selected==character));
        }
    }
    // 四种组合均已接通，但每一种仍必须独立遇见，不能借其他御札的记录解锁。
    for(int encountered=0;encountered<spell::kCharacterCount;++encountered) {
        spell::Records isolated;
        isolated.Begin(encountered,3,false);
        for(int selected=0;selected<spell::kCharacterCount;++selected) {
            assert(isolated.Seen(selected,3)==(selected==encountered));
            assert(spell::Available(night,selected,isolated)==(selected==encountered));
            assert(!isolated.Seen(selected,7));
        }
        assert(!isolated.Seen(-1,3) && !isolated.Seen(4,3));
    }
}
/// @brief 对照原图示例检查 Normal 奖励公式，并覆盖零点数与关卡系数。
void TestStageClearSummary() {
    const koumakyou::StageClearSummary clear{1,84,75,25};
    assert(clear.StagePoints()==1000 && clear.PowerPoints()==8400 && clear.GrazePoints()==750);
    assert(clear.Total()==253750);
    assert((koumakyou::StageClearSummary{6,128,1000,0}.Total()==0));
    assert((koumakyou::StageClearSummary{6,128,1000,1}.Total()==28800));
    using D=koumakyou::native_combat::Difficulty;
    assert((koumakyou::StageClearSummary{1,0,1,1,D::Easy}.Total()==500));
    assert((koumakyou::StageClearSummary{1,0,1,1,D::Hard}.Total()==1210));
    assert((koumakyou::StageClearSummary{1,0,1,1,D::Lunatic}.Total()==1510));
    assert((koumakyou::StageClearSummary{6,0,0,0,D::Normal,2,3,2}.Total()==9000000));
    assert((koumakyou::StageClearSummary{6,0,0,0,D::Hard,2,3,3}.Total()==5400000));
    assert((koumakyou::StageClearSummary{1,0,1,1,D::Lunatic,0,0,4}.Total()==300));
    assert((koumakyou::StageClearSummary{7,128,123,17,D::Normal,2,3,2}.Total()==18715020));
    assert((koumakyou::StageClearSummary{7,128,123,17,D::Normal,2,3,3}.Total()==9357510));
    assert((koumakyou::StageClearSummary{7,128,123,17,D::Normal,2,3,4}.Total()==3743000));
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
        assert(full.x==0 && full.y==0 && std::abs(full.width-size.x)<0.001F && std::abs(full.height-koumakyou::ui::CommandBar::ContentHeight(size.x,size.y))<0.001F);
        for(int index=0;index<koumakyou::menu::kOptionsCount;++index) {
            fits(koumakyou::menu::OptionRow(options,index));
            if(index<koumakyou::menu::kNumericOptions) {
                fits(koumakyou::menu::OptionArrow(options,index,-1)); fits(koumakyou::menu::OptionArrow(options,index,1));
            }
        }
        const koumakyou::menu::BindingsLayout bindings(size.x,size.y);
        const auto bindingsFull=bindings.canvas.Rect(0,0,bindings.canvas.width,bindings.canvas.height);
        assert(bindingsFull.x==0 && bindingsFull.y==0 && std::abs(bindingsFull.width-size.x)<0.001F && std::abs(bindingsFull.height-koumakyou::ui::CommandBar::ContentHeight(size.x,size.y))<0.001F);
        for(int index=0;index<=static_cast<int>(koumakyou::kBindingNames.size());++index)fits(bindings.Row(index));
        const koumakyou::menu::PageLayout page(size.x,size.y);
        fits(page.Forward());
        const koumakyou::menu::StartSetupLayout setup(size.x,size.y);
        fits(setup.Portrait()); fits(setup.CharacterInfo());
        fits(setup.Badge()); fits(setup.Heading()); fits(setup.ShotSummary()); fits(setup.ModeSummary());
        fits(setup.ReadyBand()); fits(setup.ReadyButton()); fits(setup.ExtraDifficulty());
        for(int direction:{-1,1}) {
            fits(setup.CharacterArrow(direction));
            assert(!CheckCollisionRecs(setup.CharacterArrow(direction),setup.Portrait()));
            assert(!CheckCollisionRecs(setup.CharacterArrow(direction),setup.CharacterInfo()));
        }
        const koumakyou::menu::SpellPracticeLayout spellLayout(size.x,size.y);
        fits(spellLayout.Title());fits(spellLayout.Heading());fits(spellLayout.Columns());fits(spellLayout.Footer());
        fits(spellLayout.Portrait());fits(spellLayout.CharacterInfo());
        for(int direction:{-1,1}) {fits(spellLayout.Arrow(direction));fits(spellLayout.GroupPage(direction));}
        for(int index=0;index<7;++index)fits(spellLayout.BossRow(index));
        for(int index=0;index<8;++index)fits(spellLayout.GroupRow(index));
        for(int index=0;index<4;++index)fits(spellLayout.Difficulty(index));
        fits(spellLayout.Difficulty(0,true));
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
        for(int index=0;index<koumakyou::menu::PageLayout::kTracksPerPage;++index)fits(page.Track(index));
        for(int direction:{-1,1}) {
            const auto arrow=page.TrackPageArrow(direction);fits(arrow);
            const auto center=page.canvas.Point(page.canvas.width/2,page.canvas.height/2);
            assert(std::abs(arrow.y+arrow.height/2-center.y)<0.001F);
            assert(direction<0?arrow.x+arrow.width<center.x:arrow.x>center.x);
            for(int index=0;index<koumakyou::menu::PageLayout::kTracksPerPage;++index)
                assert(!CheckCollisionRecs(arrow,page.Track(index)));
        }
        const koumakyou::menu::OverlayLayout overlay(size.x,size.y);
        fits(overlay.PausePanel()); fits(overlay.ClearPanel());
        fits(overlay.PausePanel(static_cast<int>(koumakyou::menu::PauseLabels(true).size())));
        fits(overlay.PauseRow(3));fits(overlay.SpellResultPanel());
        for(int index=0;index<3;++index) {
            const auto row=overlay.SpellResultRow(index),panel=overlay.SpellResultPanel();
            fits(row);
            assert(row.x>=panel.x && row.y>=panel.y && row.x+row.width<=panel.x+panel.width &&
                   row.y+row.height<=panel.y+panel.height);
        }
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
    // 新典脚本的入退场和强调彼此独立；以整段时钟重建，不能受跳句及显示帧率影响。
    const std::array<koumakyou::DialogueCue,5> cues{{
        {0,koumakyou::DialogueCueKind::Portrait,0,1},
        {60,koumakyou::DialogueCueKind::Portrait,1,1},
        {60,koumakyou::DialogueCueKind::Portrait,0,4},
        {120,koumakyou::DialogueCueKind::Portrait,0,5},
        {120,koumakyou::DialogueCueKind::Portrait,1,5},
    }};
    motion.Seek(cues,0);assert(motion.Sample(false).slide==1 && motion.Sample(true).opacity==0);
    motion.Seek(cues,0.5F);assert(motion.Sample(false).slide==0 && motion.Sample(false).opacity==1);
    motion.Seek(cues,68.0F/60);
    assert(std::abs(motion.Sample(false).opacity-191.0F/255)<0.0001F);
    motion.Seek(cues,1.5F);assert(motion.Sample(false).opacity==128.0F/255 && motion.Sample(true).opacity==1);
    motion.Seek(cues,2.25F);assert(motion.Sample(false).slide>0 && motion.Sample(false).slide<1);
    motion.Seek(cues,2.5F);assert(motion.Sample(false).opacity==0 && motion.Sample(true).opacity==0);
    motion.Seek(cues,0.5F);assert(motion.Sample(false).opacity==1 && motion.Sample(true).opacity==0);
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
/// @brief 运行魔理沙原始火力时序、导弹加速爆破和持续激光用例。
void RunMarisaTests();
/// @brief 运行七关 HP 目录、四机体原始伤害及未实现规则接口检查。
void RunNativeCombatTests();
/// @brief 运行四 Bomb 的真实动画、帧序和玩家保护期回归。
void RunPlayerBombTests();
/// @brief 运行时间轴及关卡内容用例，断言失败时终止进程。
void RunStageTests();
/// @brief 运行入退场曲线、快照与回收用例，断言失败时终止进程。
void RunActorEffectTests();
/// @brief 运行真实 DDS/ANM、固定帧序及全目录的兼容性验证。
void RunAnimationTests();

/// @brief 对比重构前解释器与带参行为的完整轨迹和技能。
void RunBehaviorTests();
/// @brief 一、二面的102/118源窗口和双频率时钟验收。
void RunStage1EffectTests();
void RunStage2EffectTests();
/// @brief 独立验证一面的空间面积采样、三维弹道和等路程移动。
void RunSpatialBehaviorTests();
/// @brief 验证二面独立出生、轨迹、Boss 生命/时限、对话及新道具能力。
void RunStage2Tests();
void RunStage3Tests();
/// @brief 三面效果源请求与双频率专项。
void RunStage3EffectTests();
void RunStage4Tests();
void RunStage4EncounterTests();
void RunStage5Tests();
void RunStage5DifficultyTests();
void RunStage5MidbossTests();
void RunStage5FinalNonspellTests();
void RunStage5FinalSpellTests();
void RunStage5PracticeTests();
void RunStage5EffectTests();
void RunStage6Tests();
void RunStage6DifficultyTests();
void RunStage6MidbossTests();
void RunStage6NonspellTests();
void RunStage6FirstSpellTests();
void RunStage6SpellTests();
void RunStage6PracticeTests();
void RunStageExtraTests();
void RunStageExtraDialogueTests();
void RunStageExtraTimelineTests();
void RunStageExtraPatchouliTests();
void RunStageExtraFlandreTests();
void RunStageExtraKagomeTests();
void RunStageExtraMazeTests();
void RunStageExtraStarbowTests();
void RunStageExtraCatadioptricTests();
void RunStageExtraClockTests();
void RunStageExtraSurvivalTests();
void RunStageExtraRipplesTests();
void RunStageExtraConfigurationTests();
void RunStageExtraEncoreEntranceTests();
void RunStageExtraFragileWingTests();
void RunStageExtraBloodstainTests();
void RunStageExtraHeartTests();
void RunStageExtraHeartProtocolTests();
void RunAuxiliaryAnimationTests();
void RunNativeEffectAnimationTests();
void RunExternalEffectTests();
/// @brief 四面102/118及完整爆散退场的独立源请求和双频率时钟验收。
void RunStage4EffectTests();

/// @brief 默认运行全部核心回归；--stageN-only运行对应关卡，--extra-config/--extra-ordinary/--extra-patchouli/--extra-flandre/--extra-kagome/--extra-maze/--extra-starbow/--extra-catadioptric/--extra-clock/--extra-survival/--extra-ripples/--extra-encore-entrance/--extra-fragile-wing/--extra-bloodstain/--extra-heart隔离Extra已接入段，--stage6-first-spell/--stage6-nonspells/--stage6-midboss分别隔离六面对应差分，其余专用入口用于定位首次分歧。
int main(int argc,char** argv) {
    if(argc==2&&std::string_view(argv[1])=="--start-setup-only") {TestResponsiveLayouts();TestStartSetupMotion();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage12-effects") {RunStage1EffectTests();RunStage2EffectTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage5-effects") {RunStage5EffectTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage1-behaviors") {RunBehaviorTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage2-only") {RunStage2Tests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage4-effects") {RunStage4EffectTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--gameplay-only") {RunGameplayTests();RunDropTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-timeline") {RunStageExtraTimelineTests();RunDropTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-dialogue") {RunStageExtraDialogueTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-heart-protocol") {RunStageExtraHeartProtocolTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-heart") {RunStageExtraHeartTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-bloodstain") {RunStageExtraBloodstainTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-fragile-wing") {RunStageExtraFragileWingTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-encore-entrance") {RunStageExtraEncoreEntranceTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-config") {RunStageExtraConfigurationTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-ripples") {RunStageExtraRipplesTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-survival") {RunStageExtraSurvivalTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-clock") {RunStageExtraClockTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-catadioptric") {RunStageExtraCatadioptricTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-starbow") {RunStageExtraStarbowTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-maze") {RunStageExtraMazeTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-kagome") {RunStageExtraKagomeTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-flandre") {RunStageExtraFlandreTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--auxiliary-animation") {RunAuxiliaryAnimationTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--native-effect-animation") {RunNativeEffectAnimationTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--external-effects") {RunNativeEffectAnimationTests();RunExternalEffectTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-patchouli") {RunStageExtraPatchouliTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--extra-ordinary") {RunStageExtraTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--difficulty-catalog") {RunNativeCombatTests();TestSpellPracticeCatalog();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage6-practice") {RunStage6PracticeTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage6-spells") {RunStage6SpellTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage6-first-spell") {RunStage6FirstSpellTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage6-nonspells") {RunStage6NonspellTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage6-midboss") {RunStage6MidbossTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage6-difficulties") {RunStage6DifficultyTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage6-only") {RunStage6Tests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage5-practice") {RunStage5PracticeTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage5-final-spells") {RunStage5FinalSpellTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage5-final-nonspells") {RunStage5FinalNonspellTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage5-midboss") {RunStage5MidbossTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage5-difficulties") {RunStage5DifficultyTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage3-effects") {RunStage3EffectTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage3-only") {RunStage3Tests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage4-encounters") {RunStage4EncounterTests();return 0;}
    if(argc==2&&std::string_view(argv[1])=="--stage4-only") {RunStage4Tests();return 0;}
    RunBehaviorTests();
    RunSpatialBehaviorTests();
    RunStage2Tests();
    RunStage3Tests();
    RunStage4Tests();
    RunStage5Tests();
    RunStage6Tests();
    RunStageExtraTests();
    RunStageExtraPatchouliTests();
    RunStageExtraFlandreTests();
    RunStageExtraEncoreEntranceTests();
    RunStageExtraFragileWingTests();
    RunStageExtraBloodstainTests();
    RunStageExtraHeartTests();
    RunStageExtraDialogueTests();
    RunStageExtraTimelineTests();
    RunStageExtraKagomeTests();
    RunStageExtraMazeTests();
    RunStageExtraStarbowTests();
    RunStageExtraCatadioptricTests();
    RunStageExtraClockTests();
    RunStageExtraSurvivalTests();
    RunStageExtraRipplesTests();
    assert(koumakyou::ui::BossSeconds(35,0)==35 && koumakyou::ui::BossSeconds(35,0.01F)==34);
    assert(koumakyou::ui::BossSeconds(25,5)==20 && koumakyou::ui::BossSeconds(25,25)==0);
    assert(koumakyou::ui::BossSeconds(25,26)==0 && koumakyou::ui::BossSeconds(-1,0)==-1);
    assert(koumakyou::ui::BossSeconds(150,0)==99);
    assert(koumakyou::ui::BossSeconds(0,0)==0);
    TestDialogueContent();
    TestSpellPracticeCatalog();
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
    RunNativeCombatTests();
    RunPlayerBombTests();
    RunMarisaTests();
    RunStageTests();
    RunActorEffectTests();
    RunAnimationTests();
    RunExternalEffectTests();
    return 0;
}
