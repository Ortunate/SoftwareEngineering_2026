// 本文件绘制标题与各菜单页，依据展示时钟呈现新典风格的太极选择、文字光晕和错落滑入。
#include "app/game.hpp"
#include "config/settings.hpp"
#include "render/menu_layout.hpp"
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
        const bool locked = index == 1 || index == 4;
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

void Game::DrawOptions() const {
    if (optionsPage_ == OptionsPage::KeyBindings) { DrawKeyBindings(); return; }
    const auto canvas=menu::PageCanvas(GetScreenWidth(),GetScreenHeight());
    const bool compact=canvas.width<1000;
    const Font font=assets_.Common().uiFont;
    ui::DrawPageSurface(canvas,menuMotion_);
    ui::DrawPageHeader(canvas,font,"游戏设置","设置自动保存 · Player 为额外残机","OPTION");
    const std::array<std::string,6> values{
        kSupportedFpsLimits[fpsLimitIndex_]==0 ? "Unlimited" : std::to_string(kSupportedFpsLimits[fpsLimitIndex_]),
        std::to_string(musicVolume_)+"%",std::string(TextFormat("%.1fx",mouseSensitivity_/100.0F)),
        std::to_string(soundVolume_)+"%",std::to_string(initialLives_),std::to_string(initialBombs_)};
    const std::array<int,6> levels{fpsLimitIndex_,musicVolume_,mouseSensitivity_,soundVolume_,initialLives_,initialBombs_};
    constexpr std::array<int,6> minimum{0,0,kMinMouseSensitivity,0,1,0};
    constexpr std::array<int,6> maximum{5,100,kMaxMouseSensitivity,100,5,3};
    for(int index=0;index<menu::kOptionsCount;++index) {
        const float weight=menuMotion_.Weight(index),reveal=menuMotion_.Reveal(index*0.012F);
        const float y=(compact?230:168)+index*(compact?76:48);
        DrawPageChoiceText(canvas,font,menu::kOptionLabels[index],compact?64:322,y,compact?24:22,
                   Color{177,150,143,255},kGold,weight,reveal,menuMotion_.Pulse());
        const auto row=menu::OptionRow(canvas,index);
        ui::DrawPageRowRule(canvas,row,menuMotion_,weight,reveal);
        if(index>=menu::kNumericOptions)continue;
        DrawPageChoiceText(canvas,font,values[index].c_str(),compact?552:795,y,23,kIvory,kGold,
                   weight,reveal,menuMotion_.Pulse()+menuMotion_.Flash(),true);
        const ui::ValueSelector selector{row,menu::OptionArrow(canvas,index,-1),menu::OptionArrow(canvas,index,1)};
        selector.DrawArrows({font,canvas.scale,1,menuMotion_.Phase(),menuMotion_.Pulse()},
                            levels[index],minimum[index],maximum[index],{weight,reveal});
    }
}

void Game::DrawMenuPage() const {
    if (state_ == RunState::StartSetup) { DrawStartSetup(); return; }
    const menu::PageLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto& canvas=layout.canvas;
    const bool compact=layout.compact;
    const auto& common = assets_.Common();
    const Font font = common.uiFont;
    ui::DrawPageSurface(canvas,menuMotion_);
    if (state_ == RunState::MusicRoom) {
        Text(canvas, font, "MUSIC ROOM", canvas.width/2, 100, 46, kGold, true);
        constexpr std::array<const char*, 3> names{"比赤色更鲜红的梦", "宛如鬼灯的鲜红之魂", "妖魔夜行"};
        constexpr std::array<const char*, 3> roles{"Title", "Stage 1", "Rumia"};
        for (int index = 0; index < 3; ++index) {
            const auto row=layout.Track(index);
            const float weight=menuMotion_.Weight(index),reveal=menuMotion_.Reveal(index*0.025F);
            DrawRectangleRec(row,Fade(Color{55,30,29,100},reveal));
            ui::GlowText(font,names[index],{row.x+20*canvas.scale,row.y+14*canvas.scale},26*canvas.scale,
                         Fade(ui::Blend(kIvory,kGold,weight),reveal),kGold,weight*menuMotion_.Pulse());
            ui::DrawText(font,roles[index],{row.x+row.width-135*canvas.scale,row.y+(compact?60:25)*canvas.scale},19*canvas.scale,Fade(kGold,reveal));
            ui::FocusLine({row.x,row.y+row.height},{row.x+row.width,row.y+row.height},canvas.scale,
                          kGold,weight*reveal,menuMotion_.Phase());
        }
    } else if (state_ == RunState::Score) {
        Text(canvas, font, "SCORE", canvas.width/2, 110, 50, kGold, true);
        Text(canvas, font, "Stage 1 · Normal · 标准模式", canvas.width/2, compact?360:250, 28, kIvory, true);
        const std::string score = TextFormat("%09d", bestScore_);
        Text(canvas, font, score.c_str(), canvas.width/2, compact?460:345, 65, kGold, true);
        Text(canvas, font, "最高通关分数", canvas.width/2, compact?574:444, 23, kIvory, true);
    } else {
        Text(canvas, font, "CREDIT", canvas.width/2, 110, 50, kGold, true);
        Text(canvas, font, "原作 · 东方红魔乡：新典", canvas.width/2, compact?330:260, 30, kIvory, true);
        Text(canvas, font, "Team Shanghai Alice", canvas.width/2, compact?420:320, 28, kGold, true);
        Text(canvas, font, "三维扩展与现代化重绘", canvas.width/2, compact?620:430, 27, kIvory, true);
        Text(canvas, font, "koumakyou · Fan recreation", canvas.width/2, compact?710:480, 23, kGold, true);
    }
}
}  // namespace koumakyou
