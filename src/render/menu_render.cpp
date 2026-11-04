// 本文件绘制标题与各菜单页，依据展示时钟呈现新典风格的太极选择、文字光晕和错落滑入。
#include "app/game.hpp"
#include "config/settings.hpp"
#include "render/menu_layout.hpp"
#include "resources/music_catalog.hpp"
#include "ui/components.hpp"
#include "ui/page.hpp"
#include "ui/typography.hpp"
#include "ui/effects.hpp"

#include <array>
#include <cmath>
#include <string>

namespace koumakyou {
namespace {
constexpr Color kScarlet{133, 24, 42, 255};
constexpr Color kIvory{252, 246, 236, 255};
constexpr Color kGold{233, 204, 139, 255};

/// @brief 在设计画布内绘制文字；字体由 AssetManager 借用，size 为设计像素，center 表示水平居中。
void Text(const menu::Canvas& canvas, Font font, const char* text,
          float x, float y, float size, Color color, bool center = false) {
    auto point = canvas.Point(x, y);
    if (center) point.x -= ui::MeasureText(font, text, size * canvas.scale).x / 2;
    ui::DrawText(font, text, point, size * canvas.scale, color);
}

/// @brief 首页标题居中加粗；x/y/size 为设计像素，保留共用宋体的字形与字距。
/// 对称扩展笔画而不偏移文字中心，加粗半径随字号缩放，小窗口不会出现固定像素粗边。
void TitleText(const menu::Canvas& canvas, Font font, const char* text,
               float x, float y, float size, Color color) {
    const float pixels = size * canvas.scale;
    const Vector2 origin = canvas.Point(x, y);
    const Vector2 position{origin.x - ui::MeasureText(font, text, pixels).x / 2, origin.y};
    const float stroke = pixels * 0.02F;
    for (int index = 0; index < 8; ++index) {
        const float angle = index * PI / 4;
        ui::DrawText(font, text, {position.x + std::cos(angle) * stroke,
                                position.y + std::sin(angle) * stroke}, pixels, color);
    }
    ui::DrawText(font, text, position, pixels, color);
}

using ui::DrawPageChoiceText;

}  // namespace

void Game::DrawMainMenu(const ui::MenuMotion* motion) const {
    const auto& animation=motion?*motion:menuMotion_;
    const menu::TitleLayout layout(GetScreenWidth(), GetScreenHeight(), menu::kTitleRowCount);
    const auto& canvas = layout.canvas;
    const auto& common = assets_.Common();
    const auto logoBounds = layout.Logo(IsTextureValid(common.titleLogo) ? common.titleLogo.width : 2019,
                                       IsTextureValid(common.titleLogo) ? common.titleLogo.height : 779);
    const float moonProgress = titleEntrance_.Progress(ui::TitleElement::Moon);
    const float titleProgress = titleEntrance_.Progress(ui::TitleElement::Title);
    const float shrineProgress = titleEntrance_.Progress(ui::TitleElement::Shrine);
    const float reimuProgress = titleEntrance_.Progress(ui::TitleElement::Reimu);
    const float menuProgress = titleEntrance_.Progress(ui::TitleElement::Menu);
    ClearBackground(kIvory);
    if (IsTextureValid(common.titleMoon) && IsTextureValid(common.titleShrine) && IsTextureValid(common.titleReimu)) {
        DrawRectangleGradientV(0,0,GetScreenWidth(),GetScreenHeight(),kIvory,Color{246,221,208,255});
        const auto layer = [](Texture2D texture, Rectangle frame, float opacity) {
            DrawTexturePro(texture,{0,0,static_cast<float>(texture.width),static_cast<float>(texture.height)},
                           frame,{},0,Fade(WHITE,opacity));
        };
        // 透明纹理由远到近合成；人物与神社独立定位，石阶不会被重复烘焙进人物层。
        auto moon = layout.MoonFrame(common.titleMoon.width,common.titleMoon.height,logoBounds);
        const float moonScale = 0.92F + 0.08F * moonProgress;
        // 绕月球真实中心缩放；透明画布中心偏在右下，不能作为缩放支点。
        moon.x += moon.width * 0.34F * (1-moonScale);
        moon.y += moon.height * 0.42F * (1-moonScale);
        moon.width *= moonScale; moon.height *= moonScale;
        auto shrine = layout.LayerFrame(common.titleShrine.width,common.titleShrine.height);
        shrine.x -= 24 * canvas.scale * (1-shrineProgress);
        auto reimu = layout.ReimuFrame(common.titleReimu.width,common.titleReimu.height);
        reimu.x -= 32 * canvas.scale * (1-reimuProgress);
        reimu.y += 12 * canvas.scale * (1-reimuProgress);
        layer(common.titleMoon,moon,moonProgress);
        layer(common.titleShrine,shrine,shrineProgress);
        layer(common.titleReimu,reimu,reimuProgress);
    } else if (common.titleBackground.id != 0) {
        DrawTexturePro(common.titleBackground,
            layout.BackgroundSource(common.titleBackground.width, common.titleBackground.height),
            canvas.Rect(0, 0, layout.width, layout.height), {}, 0, Fade(WHITE,reimuProgress));
    }
    // 方形/竖屏的背景裁剪会移除右侧留白，底板保障插画上方的标题和菜单对比度。
    if (layout.compact) {
        DrawRectangleRec(canvas.Rect(28, 28, layout.width - 56, 188), Fade(Color{252,246,236,225},titleProgress));
        DrawRectangleRec(canvas.Rect(layout.menuLeft - 12, layout.menuTop - 10,
            layout.menuWidth + 24, layout.rowCount * 45 + 12), Fade(Color{252,246,236,228},menuProgress));
    }
    ui::DrawPageFrame(canvas,Color{134,50,57,210},layout.width,
                      ui::CommandBar::ContentHeight(GetScreenWidth(),GetScreenHeight())/canvas.yScale);
    if (IsTextureValid(common.titleLogo)) {
        // 字标自身带英文副标题和“异典”饰线，保留透明区域，不叠加重复的普通文字。
        DrawTexturePro(common.titleLogo,
            {0, 0, static_cast<float>(common.titleLogo.width), static_cast<float>(common.titleLogo.height)},
            {logoBounds.x,logoBounds.y + 18 * canvas.scale * (1-titleProgress),logoBounds.width,logoBounds.height},
            {},0,Fade(WHITE,titleProgress));
    } else {
        const float titleX = layout.width / 2;
        const float offset = 18 * (1-titleProgress);
        TitleText(canvas, common.uiFont, "东方红魔乡", titleX, (layout.compact ? 46 : 94) + offset, 61, Fade(kScarlet,titleProgress));
        TitleText(canvas, common.uiFont, "the Embodiment of Scarlet Devil", titleX,
                  (layout.compact ? 125 : 171) + offset, 18, Fade(kScarlet,titleProgress));
    }
    constexpr std::array<const char*, 11> labels{
        "Start", "Extra Start", "Practice Start", "Spell Practice", "Replay",
        "Score", "Music Room", "Credit", "Option", "Quit", "Shot Test"};
    constexpr int count = menu::kTitleRowCount;
    for (int index = 0; index < count; ++index) {
        const float rowY = layout.menuTop + index * 45;
        const float weight = animation.Weight(index);
        const float reveal = animation.Reveal(index*0.012F) * titleEntrance_.MenuProgress(index);
        if (reveal <= 0) continue;
        const bool locked = !MainMenuItemAvailable(index);
        if (weight > 0.01F) {
            const auto center = canvas.Point(layout.menuLeft + 15 + 16*(1-reveal), rowY + 20);
            ui::YinYang(center,9.0F*canvas.scale,Fade(kScarlet,weight*reveal),
                       Fade(kIvory,weight*reveal),animation.Phase());
        }
        DrawPageChoiceText(canvas,common.uiFont,labels[index],layout.menuLeft+45,rowY+2,locked?23:28,
            locked?Color{155,116,117,80}:Color{160,87,94,160},kScarlet,weight,reveal,animation.Pulse()+animation.Flash());
        const auto left = canvas.Point(layout.menuLeft + 30, rowY + 41);
        const auto right = canvas.Point(layout.menuLeft + layout.menuWidth - 13, rowY + 41);
        ui::FocusLine(left,right,canvas.scale,Fade(kScarlet,reveal),weight,animation.Phase());
    }
}

void Game::DrawTitlePages() const {
    // 首页作为混合底层保持完整绘制；子页背景和内容使用相同权重覆盖，不创建截图纹理。
    DrawMainMenu(state_==RunState::MainMenu?nullptr:&titleMenuMotion_);
    float accumulated=titlePageMotion_.Title();
    for(int index=0;index<kTitlePages;++index) {
        const float weight=titlePageMotion_.Layer(index);
        if(weight<=0)continue;
        accumulated+=weight;
        // 将混合权重转换为逐层覆盖透明度；子页互换时总覆盖保持完整，不让首页从背景中闪现。
        const float opacity=weight/accumulated;
        const auto page=static_cast<TitlePage>(index);const auto* motion=&titlePageLayers_[index];
        if(page==TitlePage::Replay)DrawReplayTable(false,opacity,motion);
        else if(page>=TitlePage::ScoreDifficulty && page<=TitlePage::ScoreFourth)
            DrawScorePage(opacity,motion,index-static_cast<int>(TitlePage::ScoreDifficulty));
        else if(page==TitlePage::Music || page==TitlePage::Credit)
            DrawMenuPage(page==TitlePage::Music?RunState::MusicRoom:RunState::Credit,opacity,motion);
        else if(page==TitlePage::Options)DrawOptions(opacity,motion);
        else if(page==TitlePage::Bindings)DrawKeyBindings(opacity,motion);
    }
}

void Game::DrawOptions(float opacity,const ui::MenuMotion* motion) const {
    const auto& animation=motion?*motion:menuMotion_;
    const auto canvas=menu::PageCanvas(GetScreenWidth(),GetScreenHeight());
    const bool compact=canvas.width<1000;
    const Font font=assets_.Common().uiFont;
    ui::DrawPageSurface(canvas,animation,opacity);
    ui::DrawPageHeader(canvas,font,"游戏设置","设置自动保存 · Player 为额外残机","OPTION",opacity);
    const std::array<std::string,7> values{
        kSupportedFpsLimits[fpsLimitIndex_]==0 ? "Unlimited" : std::to_string(kSupportedFpsLimits[fpsLimitIndex_]),
        std::to_string(musicVolume_)+"%",std::string(TextFormat("%.1fx",mouseSensitivity_/100.0F)),
        aimAssistEnabled_ ? "ON" : "OFF",std::to_string(soundVolume_)+"%",std::to_string(initialLives_),std::to_string(initialBombs_)};
    const std::array<int,7> levels{fpsLimitIndex_,musicVolume_,mouseSensitivity_,aimAssistEnabled_ ? 1 : 0,soundVolume_,initialLives_,initialBombs_};
    constexpr std::array<int,7> minimum{0,0,kMinMouseSensitivity,0,0,1,0};
    constexpr std::array<int,7> maximum{5,100,kMaxMouseSensitivity,1,100,5,3};
    for(int index=0;index<menu::kOptionsCount;++index) {
        const float weight=animation.Weight(index),reveal=opacity*animation.Reveal(index*0.012F);
        const float y=(compact?230:168)+index*(compact?64:46);
        DrawPageChoiceText(canvas,font,menu::kOptionLabels[index],compact?64:322,y,compact?24:22,
                   Color{177,150,143,255},kGold,weight,reveal,animation.Pulse());
        const auto row=menu::OptionRow(canvas,index);
        ui::DrawPageRowRule(canvas,row,animation,weight,reveal);
        if(index>=menu::kNumericOptions)continue;
        DrawPageChoiceText(canvas,font,values[index].c_str(),compact?552:795,y,23,kIvory,kGold,
                   weight,reveal,animation.Pulse()+animation.Flash(),true);
        const ui::ValueSelector selector{row,menu::OptionArrow(canvas,index,-1),menu::OptionArrow(canvas,index,1)};
        selector.DrawArrows({font,canvas.scale,opacity,animation.Phase(),animation.Pulse()},
                            levels[index],minimum[index],maximum[index],{weight,animation.Reveal(index*0.012F)});
    }
}

void Game::DrawMenuPage(RunState page,float opacity,const ui::MenuMotion* motion) const {
    const auto& animation=motion?*motion:menuMotion_;
    const menu::PageLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto& canvas=layout.canvas;
    const bool compact=layout.compact;
    const auto& common = assets_.Common();
    const Font font = common.uiFont;
    ui::DrawPageSurface(canvas,animation,opacity);
    if (page == RunState::MusicRoom) {
        Text(canvas, font, "MUSIC ROOM", canvas.width/2, 100, 46, ui::WithOpacity(kGold,opacity), true);
        constexpr int count=static_cast<int>(music::kTracks.size()),pageSize=menu::PageLayout::kTracksPerPage;
        const int first=selectedTrack_/pageSize*pageSize;
        const ui::DrawContext context{font,canvas.scale,opacity,animation.Phase(),animation.Pulse()};
        for(int index=first;index<std::min(first+pageSize,count);++index) {
            const int rowIndex=index-first;const auto row=layout.Track(rowIndex);
            const float weight=animation.Weight(rowIndex),reveal=animation.Reveal(rowIndex*0.025F);
            auto rowContext=context;rowContext.opacity=opacity*reveal;
            const auto color=ui::SelectableRow{row,index}.Draw(rowContext,weight);
            const auto& track=music::kTracks[index];
            const auto title=std::to_string(index+1)+". "+track.title;
            const auto role=std::string(track.role)+(assets_.MusicRoomTrack()==index?" · 播放中":"");
            ui::Label{{row.x+16*canvas.scale,row.y,row.width-32*canvas.scale,row.height*0.55F},24}.Draw(rowContext,title.c_str(),color);
            ui::Label{{row.x+16*canvas.scale,row.y+row.height*0.55F,row.width-32*canvas.scale,row.height*0.4F},17}.Draw(rowContext,role.c_str(),kGold);
        }
        for(int direction:{-1,1})ui::ArrowButton{layout.TrackPageArrow(direction),direction,direction<0?first>0:first+pageSize<count}.Draw(context);
        const auto page=std::to_string(first/pageSize+1)+" / "+std::to_string((count+pageSize-1)/pageSize);
        ui::Label{canvas.Rect(canvas.width/2-80,compact?1010:675,160,42),22,ui::TextAlign::Center}.Draw(context,page.c_str(),kGold);
    } else {
        Text(canvas, font, "CREDIT", canvas.width/2, 110, 50, ui::WithOpacity(kGold,opacity), true);
        Text(canvas, font, "原作 · 东方红魔乡：新典", canvas.width/2, compact?330:260, 30, ui::WithOpacity(kIvory,opacity), true);
        Text(canvas, font, "Team Shanghai Alice", canvas.width/2, compact?420:320, 28, ui::WithOpacity(kGold,opacity), true);
        Text(canvas, font, "三维扩展与现代化重绘", canvas.width/2, compact?620:430, 27, ui::WithOpacity(kIvory,opacity), true);
        Text(canvas, font, "koumakyou · Fan recreation", canvas.width/2, compact?710:480, 23, ui::WithOpacity(kGold,opacity), true);
    }
}
}  // namespace koumakyou
