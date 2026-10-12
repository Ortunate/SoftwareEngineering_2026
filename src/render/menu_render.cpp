// 本文件绘制标题与各菜单页，依据展示时钟呈现新典风格的太极选择、文字光晕和错落滑入。
#include "app/game.hpp"
#include "config/settings.hpp"
#include "render/menu_layout.hpp"
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

/// @brief 菜单选项的文字强调；weight 为选中强度，reveal 为进场进度，坐标/字号为设计像素。
/// 动画只偏移绘制位置，命中区域始终使用 menu_layout 中的最终矩形。
void ChoiceText(const menu::Canvas& canvas, Font font, const char* text, float x, float y, float size,
                Color normal, Color selected, float weight, float reveal, float pulse, bool center = false) {
    auto point = canvas.Point(x + 16 * (1 - reveal) - 8 * weight, y);
    if (center) point.x -= ui::MeasureText(font,text,size*canvas.scale).x/2;
    ui::GlowText(font,text,point,size*canvas.scale,Fade(ui::Blend(normal,selected,weight),reveal),
                 Fade(selected,reveal),weight*pulse);
}

/// @brief 按设计像素 width/height 绘制回纹边框；标题使用窗口全幅，其他页采用默认设计尺寸。
void Frame(const menu::Canvas& canvas, Color color, float width = 1100, float height = 760) {
    DrawRectangleLinesEx(canvas.Rect(18, 18, width - 36, height - 36), canvas.scale, color);
    for (float x : {18.0F, width - 38}) {
        for (float y : {18.0F, height - 38}) {
            DrawRectangleRec(canvas.Rect(x, y, 20, 20), color);
            DrawRectangleRec(canvas.Rect(x + 4, y + 4, 12, 12), kIvory);
            DrawRectangleRec(canvas.Rect(x + 6, y + 6, 8, 8), color);
        }
    }
}

/// @brief 深红花纹页；面板由中心展开，边缘装饰低对比，交互区域保持最终位置。
void DarkPage(const menu::Canvas& canvas, const ui::MenuMotion& motion) {
    const float phase=motion.Phase(),reveal=motion.Reveal();
    ClearBackground(Color{21, 13, 18, 255});
    DrawRectangleGradientV(static_cast<int>(canvas.origin.x), static_cast<int>(canvas.origin.y+40*(1-reveal)*canvas.scale),
        static_cast<int>(canvas.width * canvas.scale), static_cast<int>((canvas.height-80*(1-reveal))*canvas.scale),
        Color{18, 16, 19, 255}, Color{62, 17, 25, 255});
    for (int index = 0; index < 18; ++index) {
        const float x = index % 2 == 0 ? 80 + (index % 4) * 38.0F : canvas.width-140 + (index % 3) * 32.0F;
        const float y = 65 + index * (canvas.height-100)/18;
        for (int petal = 0; petal < 5; ++petal) {
            const float angle = petal * 1.256637F + index * 0.3F + phase * 0.035F;
            const auto center = canvas.Point(x + std::cos(angle) * 13, y + std::sin(angle) * 13);
            DrawCircleV(center, 10 * canvas.scale, Color{208, 158, 126, 15});
        }
    }
    // 边缘环纹缓慢转动，延续新典花纹页的背景动感，不覆盖设置行。
    for (Vector2 center : {canvas.Point(-110,130),canvas.Point(canvas.width+110,canvas.height-110)})
        DrawRing(center,220*canvas.scale,221*canvas.scale,phase*3,phase*3+300,80,Color{208,158,126,18});
    Frame(canvas,Color{149,113,84,180},canvas.width,canvas.height);
}

/// @brief 页面操作按钮；hover 只影响外观，点击区域由 UpdateMenuPage 使用相同矩形。
void PageButton(const menu::Canvas& canvas, Font font, Rectangle bounds, const char* label,
                const ui::MenuMotion& motion) {
    const float hover=CheckCollisionPointRec(GetMousePosition(),bounds) ? 1.0F : 0.0F;
    DrawRectangleRec(bounds, Color{69,32,32,170});
    ui::FocusLine({bounds.x,bounds.y+bounds.height},{bounds.x+bounds.width,bounds.y+bounds.height},
                  canvas.scale,kGold,hover,motion.Phase());
    ui::GlowText(font,label,{bounds.x+12*canvas.scale,bounds.y+8*canvas.scale},22*canvas.scale,
                 ui::Blend(kIvory,kGold,hover),kGold,hover*motion.Pulse());
}
}  // namespace

void Game::DrawMainMenu() const {
    const menu::TitleLayout layout(GetScreenWidth(), GetScreenHeight(), menu::kTitleRowCount);
    const auto& canvas = layout.canvas;
    const auto& common = assets_.Common();
    ClearBackground(kIvory);
    if (common.titleBackground.id != 0) {
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
    Frame(canvas, Color{134, 50, 57, 210}, layout.width, layout.height);
    const float titleX = layout.compact ? layout.width / 2 : layout.width - 650;
    Text(canvas, common.uiFont, "东方红魔乡", titleX, layout.compact ? 46 : 94, 61, kScarlet, layout.compact);
    Text(canvas, common.uiFont, "the Embodiment of Scarlet Devil", titleX + (layout.compact ? 0 : 5),
         layout.compact ? 125 : 171, 18, kScarlet, layout.compact);
    Text(canvas, common.uiFont, "三维新绘 · 第一面", titleX + (layout.compact ? 0 : 5),
         layout.compact ? 163 : 205, 23, kScarlet, layout.compact);
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
    DrawRectangleRec(canvas.Rect(layout.compact ? 28 : layout.width - 770, layout.height - 84,
        layout.compact ? layout.width - 56 : 730, 62), Color{252, 246, 236, 225});
    Text(canvas, common.uiFont, "Z / Enter 确认   方向键选择   Esc 返回",
        layout.compact ? layout.width / 2 : layout.width - 550, layout.height - 72, 15, kScarlet, true);
    // 页脚放在浅色底板内，繁密插画上仍保持可读。
    Text(canvas, common.uiFont, "Stage 1 · Normal · 3D", layout.compact ? layout.width / 2 : layout.width - 268,
        layout.height - 52, 17, kScarlet, true);
    Text(canvas, common.uiFont, "Fan recreation · 原作 Team Shanghai Alice",
        layout.compact ? layout.width / 2 : layout.width - 550, layout.height - 33, 12, kScarlet, true);
}

void Game::DrawOptions() const {
    if (optionsPage_ == OptionsPage::KeyBindings) { DrawKeyBindings(); return; }
    const auto canvas=menu::PageCanvas(GetScreenWidth(),GetScreenHeight());
    const bool compact=canvas.width<1000;
    const Font font=assets_.Common().uiFont;
    DarkPage(canvas,menuMotion_);
    Text(canvas,font,"OPTION",compact?64:100,compact?942:590,52,Color{173,61,65,255});
    Text(canvas,font,"游戏设置",canvas.width/2,compact?90:72,35,kIvory,true);
    Text(canvas,font,"设置自动保存 · Player 为额外残机",canvas.width/2,compact?146:116,17,kGold,true);
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
        ui::FocusLine({row.x+12*canvas.scale,row.y+row.height},{row.x+row.width-12*canvas.scale,row.y+row.height},
                      canvas.scale,Fade(kGold,reveal),weight,menuMotion_.Phase());
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
    Text(canvas,font,"方向键选择 / 调整   Z / Enter 确认   Esc 返回",canvas.width/2,compact?1036:692,18,kGold,true);
    Text(canvas,font,"开局资源在下一局生效",canvas.width/2,compact?1069:722,15,Color{163,133,121,255},true);
}

void Game::DrawMenuPage() const {
    const menu::PageLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto& canvas=layout.canvas;
    const bool compact=layout.compact;
    const auto& common = assets_.Common();
    const Font font = common.uiFont;
    DarkPage(canvas,menuMotion_);
    PageButton(canvas, font, layout.Back(), "返回 / Esc",menuMotion_);
    if (state_ == RunState::StartSetup) {
        if (common.reimuPortrait.id != 0) {
            const auto texture = common.reimuPortrait;
            DrawTexturePro(texture, {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
                canvas.Rect((compact?244:130)-35*(1-menuMotion_.Reveal()),compact?142:82,compact?272:357,compact?434:570),{},0,Fade(WHITE,menuMotion_.Reveal()));
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
        PageButton(canvas, font, layout.Forward(), setupStep_ >= 2 ? "开始游戏 / Z" : "下一步 / Z",menuMotion_);
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
