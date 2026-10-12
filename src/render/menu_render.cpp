// 本文件绘制标题与各菜单页，依据展示时钟呈现新典风格的太极选择、文字光晕和错落滑入。
#include "app/game.hpp"
#include "config/settings.hpp"
#include "render/menu_layout.hpp"
#include "render/menu_page.hpp"
#include "render/ui_typography.hpp"
#include "render/ui_effects.hpp"

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

/// @brief 菜单选项的文字强调；weight 为选中强度，reveal 为进场进度，坐标/字号为设计像素。
/// 动画只偏移绘制位置，命中区域始终使用 menu_layout 中的最终矩形。
void ChoiceText(const menu::Canvas& canvas, Font font, const char* text, float x, float y, float size,
                Color normal, Color selected, float weight, float reveal, float pulse, bool center = false) {
    auto point = canvas.Point(x + 16 * (1 - reveal) - 8 * weight, y);
    if (center) point.x -= ui::MeasureText(font,text,size*canvas.scale).x/2;
    ui::GlowText(font,text,point,size*canvas.scale,Fade(ui::Blend(normal,selected,weight),reveal),
                 Fade(selected,reveal),weight*pulse);
}

/// @brief 将借用的立绘等比放入屏幕矩形，布局铺满时仍保持人物原始比例。
void Portrait(Texture2D texture,Rectangle slot,Color color) {
    const float scale=std::min(slot.width/texture.width,slot.height/texture.height);
    const float width=texture.width*scale,height=texture.height*scale;
    DrawTexturePro(texture,{0,0,static_cast<float>(texture.width),static_cast<float>(texture.height)},
                   {slot.x+(slot.width-width)/2,slot.y+(slot.height-height)/2,width,height},{},0,color);
}
}  // namespace

void Game::DrawMainMenu() const {
    const menu::TitleLayout layout(GetScreenWidth(), GetScreenHeight(), menu::kTitleRowCount);
    const auto& canvas = layout.canvas;
    const auto& common = assets_.Common();
    const auto logoBounds = layout.Logo(IsTextureValid(common.titleLogo) ? common.titleLogo.width : 2019,
                                       IsTextureValid(common.titleLogo) ? common.titleLogo.height : 779);
    ClearBackground(kIvory);
    if (IsTextureValid(common.titleMoon) && IsTextureValid(common.titleShrine) && IsTextureValid(common.titleReimu)) {
        DrawRectangleGradientV(0,0,GetScreenWidth(),GetScreenHeight(),kIvory,Color{246,221,208,255});
        const auto layer = [](Texture2D texture, Rectangle frame) {
            DrawTexturePro(texture,{0,0,static_cast<float>(texture.width),static_cast<float>(texture.height)},
                           frame,{},0,WHITE);
        };
        // 透明纹理由远到近合成；人物与神社独立定位，石阶不会被重复烘焙进人物层。
        layer(common.titleMoon,layout.MoonFrame(common.titleMoon.width,common.titleMoon.height,logoBounds));
        layer(common.titleShrine,layout.LayerFrame(common.titleShrine.width,common.titleShrine.height));
        layer(common.titleReimu,layout.ReimuFrame(common.titleReimu.width,common.titleReimu.height));
    } else if (common.titleBackground.id != 0) {
        DrawTexturePro(common.titleBackground,
            layout.BackgroundSource(common.titleBackground.width, common.titleBackground.height),
            canvas.Rect(0, 0, layout.width, layout.height), {}, 0, WHITE);
    }
    // 方形/竖屏的背景裁剪会移除右侧留白，底板保障插画上方的标题和菜单对比度。
    if (layout.compact) {
        DrawRectangleRec(canvas.Rect(28, 28, layout.width - 56, 188), Color{252, 246, 236, 225});
        DrawRectangleRec(canvas.Rect(layout.menuLeft - 12, layout.menuTop + (titleIntro_ ? 250 : -10),
            layout.menuWidth + 24, titleIntro_ ? 94 : layout.rowCount * 45 + 12), Color{252, 246, 236, 228});
    }
    menu::DrawPageFrame(canvas,Color{134,50,57,210},layout.width,layout.height);
    if (IsTextureValid(common.titleLogo)) {
        // 字标自身带英文副标题和“异典”饰线，保留透明区域，不叠加重复的普通文字。
        DrawTexturePro(common.titleLogo,
            {0, 0, static_cast<float>(common.titleLogo.width), static_cast<float>(common.titleLogo.height)},
            logoBounds, {}, 0, WHITE);
    } else {
        const float titleX = layout.width / 2;
        TitleText(canvas, common.uiFont, "东方红魔乡", titleX, layout.compact ? 46 : 94, 61, kScarlet);
        TitleText(canvas, common.uiFont, "the Embodiment of Scarlet Devil", titleX,
                  layout.compact ? 125 : 171, 18, kScarlet);
    }
    if (titleIntro_) {
        const float pulse = menuMotion_.Pulse();
        Text(canvas, common.uiFont, "PRESS Z / ENTER", layout.menuLeft + layout.menuWidth / 2,
             layout.menuTop + 280, 29, Fade(kScarlet, pulse), true);
    } else {
        constexpr std::array<const char*, 11> labels{
            "Start", "Extra Start", "Practice Start", "Spell Practice", "Replay",
            "Score", "Music Room", "Credit", "Option", "Quit", "Shot Test"};
        constexpr int count = menu::kTitleRowCount;
        for (int index = 0; index < count; ++index) {
            const float rowY = layout.menuTop + index * 45;
            const float weight = menuMotion_.Weight(index);
            const float reveal = menuMotion_.Reveal(index*0.012F);
            const bool locked = index == 1 || index == 4;
            if (weight > 0.01F) {
                const auto center = canvas.Point(layout.menuLeft + 15 + 16*(1-reveal), rowY + 20);
                ui::YinYang(center,9.0F*canvas.scale,Fade(kScarlet,weight*reveal),
                           Fade(kIvory,weight*reveal),menuMotion_.Phase());
            }
            ChoiceText(canvas,common.uiFont,labels[index],layout.menuLeft+45,rowY+2,locked?23:28,
                locked?Color{155,116,117,80}:Color{160,87,94,160},kScarlet,weight,reveal,menuMotion_.Pulse()+menuMotion_.Flash());
            const auto left = canvas.Point(layout.menuLeft + 30, rowY + 41);
            const auto right = canvas.Point(layout.menuLeft + layout.menuWidth - 13, rowY + 41);
            ui::FocusLine(left,right,canvas.scale,Fade(kScarlet,reveal),weight,menuMotion_.Phase());
        }
    }
}

void Game::DrawOptions() const {
    if (optionsPage_ == OptionsPage::KeyBindings) { DrawKeyBindings(); return; }
    const auto canvas=menu::PageCanvas(GetScreenWidth(),GetScreenHeight());
    const bool compact=canvas.width<1000;
    const Font font=assets_.Common().uiFont;
    menu::DrawPageSurface(canvas,menuMotion_);
    menu::DrawPageHeader(canvas,font,"游戏设置","设置自动保存 · Player 为额外残机","OPTION");
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
        ChoiceText(canvas,font,menu::kOptionLabels[index],compact?64:322,y,compact?24:22,
                   Color{177,150,143,255},kGold,weight,reveal,menuMotion_.Pulse());
        const auto row=menu::OptionRow(canvas,index);
        menu::DrawPageRowRule(canvas,row,menuMotion_,weight,reveal);
        if(index>=menu::kNumericOptions)continue;
        ChoiceText(canvas,font,values[index].c_str(),compact?552:795,y,23,kIvory,kGold,
                   weight,reveal,menuMotion_.Pulse()+menuMotion_.Flash(),true);
        for(int direction : {-1,1}) {
            const auto arrow=menu::OptionArrow(canvas,index,direction);
            const bool disabled=direction<0 ? levels[index]==minimum[index] : levels[index]==maximum[index];
            const Color color=Fade(disabled?Color{92,71,65,255}:ui::Blend(Color{137,103,82,255},kGold,weight),reveal);
            DrawRectangleLinesEx(arrow,canvas.scale,color);
            ui::DrawCentered(font,direction<0?"<":">",arrow.x+arrow.width/2,arrow.y+2*canvas.scale,23*canvas.scale,color);
        }
    }
    menu::DrawPageFooter(canvas,font,"方向键选择 / 调整   Z / Enter 确认   Esc 返回","开局资源在下一局生效");
}

void Game::DrawMenuPage() const {
    const menu::PageLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto& canvas=layout.canvas;
    const bool compact=layout.compact;
    const auto& common = assets_.Common();
    const Font font = common.uiFont;
    menu::DrawPageSurface(canvas,menuMotion_);
    menu::DrawPageButton(canvas,font,layout.Back(),"返回 / Esc",menuMotion_);
    if (state_ == RunState::StartSetup) {
        if (common.reimuPortrait.id != 0) {
            const auto texture = common.reimuPortrait;
            Portrait(texture,canvas.Rect((compact?244:130)-35*(1-menuMotion_.Reveal()),compact?142:82,
                     compact?272:357,compact?434:570),Fade(WHITE,menuMotion_.Reveal()));
        }
        Text(canvas, font, "博丽 灵梦", compact?380:260, compact?584:598, 37, kIvory, true);
        Text(canvas, font, "Stage 1 · Mystic Flier", compact?380:790, compact?86:100, 25, kGold, true);
        constexpr std::array<const char*, 4> titles{"请选择难度", "请选择使用的御札", "要以哪一种模式开始游戏？", "符卡练习"};
        Text(canvas, font, titles[setupStep_], compact?380:825, compact?630:178, 28, kIvory, true);
        if (setupStep_ == 0) {
            const auto design=layout.Choice(0);
            const auto card=canvas.Rect(design.x+30*(1-menuMotion_.Reveal()),design.y,design.width,220);
            DrawRectangleRec(card, kIvory);
            ChoiceText(canvas,font,"NORMAL",design.x+design.width/2,design.y+38,45,Color{40,69,91,255},kScarlet,
                       1,menuMotion_.Reveal(),menuMotion_.Pulse(),true);
            ui::FocusLine(canvas.Point(design.x+22,design.y+98),canvas.Point(design.x+design.width-22,design.y+98),canvas.scale,kScarlet,1,menuMotion_.Phase());
            Text(canvas, font, "第一面 · 红雾异变的开端", design.x+design.width/2, design.y+114, 22, kScarlet, true);
            Text(canvas, font, "三维自由移动与球形判定", design.x+design.width/2, design.y+155, 18, kScarlet, true);
        } else {
            const std::array<const char*, 2> names = setupStep_ == 1
                ? std::array<const char*, 2>{"灵之御札 · Homing Amulet", "梦之御札 · Persuasion Needle"}
                : setupStep_ == 2 ? std::array<const char*, 2>{"标准模式", "挑战模式"}
                : std::array<const char*, 2>{"夜符 · Night Bird", "暗符 · Demarcation"};
            const std::array<const char*, 2> hints = setupStep_ == 1
                ? std::array<const char*, 2>{"广域追踪 · 梦想封印", "集中直射 · 封魔阵"}
                : setupStep_ == 2 ? std::array<const char*, 2>{"有限残机 · 挑战一面最高分", "无限残机 · 挑战最少 Miss"}
                : std::array<const char*, 2>{"旋转交错的三维双层弹幕", "多层边界环与交替开口"};
            for (int index = 0; index < 2; ++index) {
                const float weight=menuMotion_.Weight(index),reveal=menuMotion_.Reveal(index*0.04F);
                const auto design=layout.Choice(index);
                const auto card=canvas.Rect(design.x+16*(1-reveal)-8*weight,design.y,design.width,design.height);
                DrawRectangleRec(card,Fade(ui::Blend(Color{77,38,36,160},Color{248,236,214,255},weight),reveal));
                DrawRectangleLinesEx(card,canvas.scale,Fade(kGold,0.3F+weight*0.7F));
                ChoiceText(canvas,font,names[index],design.x+20,design.y+18,23,kGold,kScarlet,weight,reveal,menuMotion_.Pulse());
                ChoiceText(canvas,font,hints[index],design.x+20,design.y+64,18,Color{193,162,150,255},kScarlet,weight,reveal,menuMotion_.Pulse());
                ui::FocusLine({card.x+20*canvas.scale,card.y+card.height-8*canvas.scale},
                    {card.x+card.width-20*canvas.scale,card.y+card.height-8*canvas.scale},canvas.scale,
                    ui::Blend(kGold,kScarlet,weight),weight*reveal,menuMotion_.Phase());
            }
        }
        menu::DrawPageButton(canvas,font,layout.Forward(),setupStep_>=2?"开始游戏 / Z":"下一步 / Z",menuMotion_);
    } else if (state_ == RunState::MusicRoom) {
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
        Text(canvas, font, "方向键选择 · Z 播放", canvas.width/2, compact?900:598, 22, kGold, true);
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
