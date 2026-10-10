// 本文件绘制浅色红月标题、深色花纹选项及一面启动/音乐/成绩页面。
#include "app/game.hpp"
#include "config/settings.hpp"
#include "render/menu_layout.hpp"

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
    if (center) point.x -= MeasureTextEx(font, text, size * canvas.scale, canvas.scale).x / 2;
    DrawTextEx(font, text, point, size * canvas.scale, canvas.scale, color);
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

/// @brief 深红花纹页；低对比装饰只放在页面边缘，避免干扰数值和交互文本。
void DarkPage(const menu::Canvas& canvas) {
    ClearBackground(Color{21, 13, 18, 255});
    DrawRectangleGradientV(static_cast<int>(canvas.origin.x), static_cast<int>(canvas.origin.y),
        static_cast<int>(1100 * canvas.scale), static_cast<int>(760 * canvas.scale),
        Color{18, 16, 19, 255}, Color{62, 17, 25, 255});
    for (int index = 0; index < 18; ++index) {
        const float x = index % 2 == 0 ? 80 + (index % 4) * 38.0F : 960 + (index % 3) * 32.0F;
        const float y = 65 + index * 36.0F;
        for (int petal = 0; petal < 5; ++petal) {
            const float angle = petal * 1.256637F + index * 0.3F;
            const auto center = canvas.Point(x + std::cos(angle) * 13, y + std::sin(angle) * 13);
            DrawCircleV(center, 10 * canvas.scale, Color{208, 158, 126, 15});
        }
    }
    Frame(canvas, Color{149, 113, 84, 180});
}

/// @brief 页面操作按钮；hover 只影响外观，点击区域由 UpdateMenuPage 使用相同矩形。
void PageButton(const menu::Canvas& canvas, Font font, Rectangle bounds, const char* label) {
    DrawRectangleRec(bounds, CheckCollisionPointRec(GetMousePosition(), bounds)
        ? Color{120, 56, 46, 255} : Color{69, 32, 32, 255});
    DrawRectangleLinesEx(bounds, canvas.scale, kGold);
    DrawTextEx(font, label, {bounds.x + 12 * canvas.scale, bounds.y + 8 * canvas.scale},
               22 * canvas.scale, canvas.scale, kIvory);
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
        const float pulse = 0.7F + 0.3F * std::sin(static_cast<float>(GetTime()) * 2.5F);
        Text(canvas, common.uiFont, "PRESS Z / ENTER", layout.menuLeft + layout.menuWidth / 2,
             layout.menuTop + 280, 29, Fade(kScarlet, pulse), true);
    } else {
        constexpr std::array<const char*, 11> labels{
            "Start", "Extra Start", "Practice Start", "Spell Practice", "Replay",
            "Score", "Music Room", "Credit", "Option", "Quit", "Shot Test"};
        constexpr int count = menu::kTitleRowCount;
        for (int index = 0; index < count; ++index) {
            const auto row = layout.Row(index);
            const float rowY = layout.menuTop + index * 45;
            const bool selected = selectedMenuItem_ == index;
            const bool locked = index == 1 || index == 4;
            if (selected) {
                DrawRectangleRec(row, Color{255, 250, 240, 175});
                const auto center = canvas.Point(layout.menuLeft + 14, rowY + 20);
                DrawCircleV(center, 8 * canvas.scale, kScarlet);
                DrawCircleV({center.x, center.y - 4 * canvas.scale}, 3 * canvas.scale, kIvory);
            }
            Text(canvas, common.uiFont, labels[index], layout.menuLeft + (selected ? 30 : 45),
                rowY + 2, locked ? 23 : 28,
                locked ? Color{155, 116, 117, 100} : selected ? kScarlet : Color{160, 87, 94, 220});
            const auto left = canvas.Point(layout.menuLeft + 30, rowY + 41);
            const auto right = canvas.Point(layout.menuLeft + layout.menuWidth - 13, rowY + 41);
            DrawLineEx(left, right, canvas.scale, selected ? kScarlet : Color{178, 119, 124, 70});
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
    const menu::Canvas canvas(GetScreenWidth(), GetScreenHeight());
    const Font font = assets_.Common().uiFont;
    DarkPage(canvas);
    Text(canvas, font, "OPTION", 100, 590, 52, Color{173, 61, 65, 255});
    Text(canvas, font, "游戏设置", 550, 72, 35, kIvory, true);
    Text(canvas, font, "设置自动保存 · Player 为额外残机", 550, 116, 17, kGold, true);
    const std::array<std::string, 6> values{
        kSupportedFpsLimits[fpsLimitIndex_] == 0 ? "Unlimited" : std::to_string(kSupportedFpsLimits[fpsLimitIndex_]),
        std::to_string(musicVolume_) + "%", std::string(TextFormat("%.1fx", mouseSensitivity_ / 100.0F)),
        std::to_string(soundVolume_) + "%", std::to_string(initialLives_), std::to_string(initialBombs_)};
    const std::array<int, 6> levels{fpsLimitIndex_, musicVolume_, mouseSensitivity_, soundVolume_, initialLives_, initialBombs_};
    constexpr std::array<int, 6> minimum{0, 0, kMinMouseSensitivity, 0, 1, 0};
    constexpr std::array<int, 6> maximum{5, 100, kMaxMouseSensitivity, 100, 5, 3};
    for (int index = 0; index < menu::kOptionsCount; ++index) {
        const auto row = menu::OptionRow(canvas, index);
        const bool selected = selectedOption_ == index;
        if (selected) DrawRectangleRec(row, Color{168, 112, 73, 30});
        Text(canvas, font, menu::kOptionLabels[index], 322, 168 + index * 48, 22,
             selected ? kGold : Color{212, 190, 184, 255});
        const auto from = canvas.Point(322, 202 + index * 48);
        const auto to = canvas.Point(920, 202 + index * 48);
        DrawLineEx(from, to, canvas.scale, Color{122, 84, 72, 75});
        if (index >= menu::kNumericOptions) continue;
        Text(canvas, font, values[index].c_str(), 795, 168 + index * 48, 23, kIvory, true);
        for (int direction : {-1, 1}) {
            const auto arrow = menu::OptionArrow(canvas, index, direction);
            const bool disabled = direction < 0 ? levels[index] == minimum[index] : levels[index] == maximum[index];
            DrawRectangleLinesEx(arrow, canvas.scale, disabled ? Color{92, 71, 65, 255} : kGold);
            Text(canvas, font, direction < 0 ? "<" : ">", direction < 0 ? 680 : 896,
                 167 + index * 48, 23, disabled ? Color{92, 71, 65, 255} : kGold);
        }
    }
    Text(canvas, font, "方向键选择 / 调整   Z / Enter 确认   Esc 返回", 550, 692, 18, kGold, true);
    Text(canvas, font, "开局资源在下一局生效", 550, 722, 15, Color{163, 133, 121, 255}, true);
}

void Game::DrawMenuPage() const {
    const menu::Canvas canvas(GetScreenWidth(), GetScreenHeight());
    const auto& common = assets_.Common();
    const Font font = common.uiFont;
    DarkPage(canvas);
    PageButton(canvas, font, canvas.Rect(70, 675, 170, 42), "返回 / Esc");
    if (state_ == RunState::StartSetup) {
        if (common.reimuPortrait.id != 0) {
            const auto texture = common.reimuPortrait;
            DrawTexturePro(texture, {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
                canvas.Rect(130, 82, 357, 570), {}, 0, WHITE);
        }
        Text(canvas, font, "博丽 灵梦", 260, 598, 37, kIvory, true);
        Text(canvas, font, "Stage 1 · Mystic Flier", 790, 100, 25, kGold, true);
        constexpr std::array<const char*, 4> titles{"请选择难度", "请选择使用的御札", "要以哪一种模式开始游戏？", "符卡练习"};
        Text(canvas, font, titles[setupStep_], 825, 178, 28, kIvory, true);
        if (setupStep_ == 0) {
            const auto card = canvas.Rect(640, 300, 385, 220);
            DrawRectangleRec(card, kIvory);
            Text(canvas, font, "NORMAL", 832, 338, 45, Color{40, 69, 91, 255}, true);
            Text(canvas, font, "第一面 · 红雾异变的开端", 832, 414, 22, kScarlet, true);
            Text(canvas, font, "三维自由移动与球形判定", 832, 455, 18, kScarlet, true);
        } else {
            const std::array<const char*, 2> names = setupStep_ == 1
                ? std::array<const char*, 2>{"灵之御札 · Homing Amulet", "梦之御札 · Persuasion Needle"}
                : setupStep_ == 2 ? std::array<const char*, 2>{"标准模式", "挑战模式"}
                : std::array<const char*, 2>{"夜符 · Night Bird", "暗符 · Demarcation"};
            const std::array<const char*, 2> hints = setupStep_ == 1
                ? std::array<const char*, 2>{"广域追踪 · 梦想封印", "集中直射 · 封魔阵"}
                : setupStep_ == 2 ? std::array<const char*, 2>{"有限残机 · 挑战一面最高分", "无限残机 · 挑战最少 Miss"}
                : std::array<const char*, 2>{"旋转交错的三维双层弹幕", "多层边界环与交替开口"};
            const int selection = setupStep_ == 1 ? selectedShot_ : setupStep_ == 2 ? static_cast<int>(challengeMode_)
                : (practicePhase_ == 2 ? 0 : 1);
            for (int index = 0; index < 2; ++index) {
                const bool selected = index == selection;
                const auto card = canvas.Rect(640, 300 + index * 125, 385, 105);
                DrawRectangleRec(card, selected ? Color{248, 236, 214, 255} : Color{77, 38, 36, 255});
                DrawRectangleLinesEx(card, canvas.scale, kGold);
                Text(canvas, font, names[index], 660, 318 + index * 125, 23, selected ? kScarlet : kGold);
                Text(canvas, font, hints[index], 660, 364 + index * 125, 18, selected ? kScarlet : Color{193, 162, 150, 255});
            }
        }
        PageButton(canvas, font, canvas.Rect(775, 675, 250, 42), setupStep_ >= 2 ? "开始游戏 / Z" : "下一步 / Z");
    } else if (state_ == RunState::MusicRoom) {
        Text(canvas, font, "MUSIC ROOM", 550, 100, 46, kGold, true);
        constexpr std::array<const char*, 3> names{"比赤色更鲜红的梦", "宛如鬼灯的鲜红之魂", "妖魔夜行"};
        constexpr std::array<const char*, 3> roles{"Title", "Stage 1", "Rumia"};
        for (int index = 0; index < 3; ++index) {
            const auto row = canvas.Rect(280, 260 + index * 95, 680, 70);
            DrawRectangleRec(row, index == selectedTrack_ ? Color{122, 57, 49, 255} : Color{55, 30, 29, 255});
            Text(canvas, font, names[index], 300, 274 + index * 95, 26, kIvory);
            Text(canvas, font, roles[index], 825, 285 + index * 95, 19, kGold);
        }
        Text(canvas, font, "方向键选择 · Z 播放", 550, 598, 22, kGold, true);
    } else if (state_ == RunState::Score) {
        Text(canvas, font, "SCORE", 550, 110, 50, kGold, true);
        Text(canvas, font, "Stage 1 · Normal · 标准模式", 550, 250, 28, kIvory, true);
        const std::string score = TextFormat("%09d", bestScore_);
        Text(canvas, font, score.c_str(), 550, 345, 65, kGold, true);
        Text(canvas, font, "最高通关分数", 550, 444, 23, kIvory, true);
    } else {
        Text(canvas, font, "CREDIT", 550, 110, 50, kGold, true);
        Text(canvas, font, "原作 · 东方红魔乡：新典", 550, 260, 30, kIvory, true);
        Text(canvas, font, "Team Shanghai Alice", 550, 320, 28, kGold, true);
        Text(canvas, font, "三维扩展与现代化重绘", 550, 430, 27, kIvory, true);
        Text(canvas, font, "koumakyou · Fan recreation", 550, 480, 23, kGold, true);
    }
}
}  // namespace koumakyou
