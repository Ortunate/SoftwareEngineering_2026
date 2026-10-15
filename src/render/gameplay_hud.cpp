// 本文件绘制全窗口战斗的两侧悬浮 HUD、Boss 信息、对话及符卡演出，不推进任何游戏时钟。
#include "app/game.hpp"
#include "app/constants.hpp"
#include "render/gameplay_layout.hpp"
#include "render/ui_effects.hpp"
#include "render/ui_typography.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace koumakyou {
namespace {
constexpr Color kInk{240, 230, 218, 255};
constexpr Color kMuted{183, 165, 165, 255};
constexpr Color kGold{210, 173, 116, 255};
constexpr Color kRose{243, 163, 183, 255};
constexpr Color kGreen{169, 221, 178, 255};

/// @brief 以逻辑像素绘制半透明卡片；opacity 只影响显示，不能拦截鼠标或改变底下的视口。
void Panel(Rectangle rect, float scale, float opacity = 1) {
    DrawRectangleRounded(rect, 0.08F, 12, ui::WithOpacity(Color{18, 10, 23, 182}, opacity));
    DrawRectangleRoundedLinesEx(rect, 0.08F, 12, std::max(0.5F, scale),
                                ui::WithOpacity(Color{210, 173, 116, 100}, opacity));
    DrawLineEx({rect.x + 14 * scale, rect.y}, {rect.x + 46 * scale, rect.y},
                2 * scale, ui::WithOpacity(kGold, opacity));
}

/// @brief 文字在 maxWidth 逻辑像素内自适应字号；字符串只借用，right 表示 position 为右边缘。
void Label(Font font, std::string_view value, Vector2 position, float size, float maxWidth,
           Color color, float opacity = 1, bool right = false) {
    const std::string text(value);
    size = ui::FittedSize(font, text.c_str(), size, maxWidth);
    if (right) position.x -= ui::MeasureText(font, text.c_str(), size).x;
    ui::DrawText(font, text.c_str(), {position.x + 0.8F, position.y + 0.8F}, size,
                  ui::WithOpacity(Color{0, 0, 0, 190}, opacity));
    ui::DrawText(font, text.c_str(), position, size, ui::WithOpacity(color, opacity));
}

/// @brief 少量资源逐颗显示星标，超过五颗改用准确数量，防止额外残机奖励挤出状态卡。
std::string Stock(int count) {
    if (count <= 0) return "0";
    if (count > 5) return "★ x " + std::to_string(count);
    std::string value;
    for (int index = 0; index < count; ++index) value += "★ ";
    return value;
}
}

void Game::DrawHud() const {
    const GameplayLayout l(GetScreenWidth(), GetScreenHeight());
    const float s = l.scale;
    const Font font = assets_.Common().uiFont;
    // 演出时保留关键状态但降低强调，立绘/台词使用独立图层，不被侧栏文字争抢视觉焦点。
    const float emphasis = dialogueActive_ || spellCutInTimer_ > 0 ? 0.38F : 1.0F;
    const Rectangle stage = l.StagePanel(), status = l.StatusPanel();
    Panel(stage, s, emphasis);
    if (stageContent_) {
        const auto info = stageContent_->DisplayInfo();
        const float x = stage.x + 16 * s, w = stage.width - 32 * s;
        Label(font, info.label, {x, stage.y + 12 * s}, 13 * s, w, kGold, emphasis);
        Label(font, info.title, {x, stage.y + 34 * s}, 24 * s, w, kInk, emphasis);
        Label(font, info.subtitle, {x, stage.y + 66 * s}, 13 * s, w, kMuted, emphasis);
        const char* mode = spellPractice_ ? "NORMAL / 符卡练习" : practiceMode_ ? "NORMAL / 关卡练习"
            : challengeMode_ ? "NORMAL / 挑战模式" : "NORMAL";
        Label(font, mode, {x, stage.y + (l.compact ? 92 : 109) * s}, 13 * s, w,
               kRose, emphasis);
    }

    Panel(status, s, emphasis);
    const float pad = (l.compact ? 13 : 16) * s;
    const float left = status.x + pad, right = status.x + status.width - pad;
    const float contentWidth = right - left;
    const auto row = [&](const char* name, std::string_view value, float y,
                         float valueSize, Color valueColor = kInk) {
        Label(font, name, {left, status.y + y * s}, (l.compact ? 14 : 16) * s,
               65 * s, kMuted, emphasis);
        Label(font, value, {right, status.y + (y - 1) * s}, valueSize * s,
               contentWidth - 66 * s, valueColor, emphasis, true);
    };
    if (!l.compact)
        Label(font, "状态", {left, status.y + 13 * s}, 13 * s, contentWidth, kGold, emphasis);
    row("得分", TextFormat("%09lld", score_), l.compact ? 12 : 42, l.compact ? 18 : 23);
    row("最高得分", TextFormat("%09d", bestScore_), l.compact ? 37 : 77, l.compact ? 12 : 15);
    if (!l.compact)
        DrawLineEx({left, status.y + 102 * s}, {right, status.y + 102 * s}, s,
                    ui::WithOpacity(Color{210, 173, 116, 65}, emphasis));
    row(challengeMode_ ? "MISS" : "Player",
        challengeMode_ ? std::to_string(missCount_) : Stock(player_.RemainingLives()),
        l.compact ? 59 : 116, l.compact ? 15 : 20, kRose);
    row("Bomb", Stock(player_.Bombs()), l.compact ? 81 : 148, l.compact ? 15 : 20, kGreen);
    row("Power", player_.Power() == Player::kMaxPower ? "MAX" : TextFormat("%d / 128", player_.Power()),
        l.compact ? 103 : 180, l.compact ? 13 : 16, kRose);
    const Rectangle power{left, status.y + (l.compact ? 123 : 205) * s, contentWidth, 4 * s};
    DrawRectangleRec(power, ui::WithOpacity(Color{65, 41, 56, 230}, emphasis));
    DrawRectangleRec({power.x, power.y, power.width * player_.Power() / Player::kMaxPower, power.height},
                      ui::WithOpacity(kRose, emphasis));
    if (l.compact) {
        Label(font, TextFormat("Graze %d", graze_), {left, status.y + 135 * s}, 13 * s,
               contentWidth * 0.52F, kGreen, emphasis);
        Label(font, TextFormat("点数 %d", pointItems_), {right, status.y + 135 * s}, 13 * s,
               contentWidth * 0.46F, kInk, emphasis, true);
    } else {
        row("Graze", TextFormat("%d", graze_), 226, 16, kGreen);
        row("点数", TextFormat("%d", pointItems_), 251, 16);
    }

    if (boss_ && boss_->active && !dialogueActive_) {
        const Rectangle panel = l.BossPanel();
        Panel(panel, s);
        const float x = panel.x + 14 * s, y = panel.y + 9 * s;
        const float timerWidth = 45 * s;
        const char* phase = TextFormat("Enemy %d", !boss_->IsMidboss() && boss_->phase <= 2 ? 1 : 0);
        Label(font, phase, {x, y}, 12 * s, 76 * s, kGold);
        if (boss_->SpellName()) {
            Label(font, boss_->SpellName(), {x + 84 * s, y}, (l.compact ? 13 : 16) * s,
                   panel.width - (28 + 84) * s - timerWidth, kInk);
        }
        if (boss_->TimeLimit() > 0) {
            const int seconds = std::max(0, static_cast<int>(std::ceil(boss_->TimeLimit() - boss_->phaseTimer)));
            Label(font, TextFormat("%02d", seconds), {panel.x + panel.width - 14 * s, y - 2 * s},
                   23 * s, timerWidth, seconds <= 10 ? kRose : kInk, 1, true);
        }
        const float ratio = boss_->maxHealth > 0 ? std::clamp(boss_->health / boss_->maxHealth, 0.0F, 1.0F) : 0;
        const Rectangle bar{x, panel.y + panel.height - 17 * s, panel.width - 28 * s, 5 * s};
        DrawRectangleRec(bar, Color{64, 35, 50, 230});
        DrawRectangleRec({bar.x, bar.y, bar.width * ratio, bar.height}, kInk);
    }

    if (!dialogueActive_ && state_ == RunState::Playing) {
        const Vector3 position = player_.Position();
        bool danger = false;
        bullets_.Pool().ForEachActive([&](const Bullet& bullet) {
            if (bullet.owner == BulletOwner::Enemy && bullet.Dangerous() &&
                bullet.DistanceSquaredTo(position) < 1.15F * 1.15F) danger = true;
        });
        const int x = static_cast<int>(l.width / 2), y = static_cast<int>(l.height / 2);
        const float aimScale = std::clamp(s, 0.75F, 1.5F);
        const Color color = danger ? Color{255, 70, 90, 255} : Color{215, 240, 255, 235};
        DrawCircleLines(x, y, 6 * aimScale, color);
        DrawCircle(x, y, 1.5F * aimScale, color);
        DrawLine(x - 14 * aimScale, y, x - 9 * aimScale, y, color);
        DrawLine(x + 9 * aimScale, y, x + 14 * aimScale, y, color);
        DrawLine(x, y - 14 * aimScale, x, y - 9 * aimScale, color);
        DrawLine(x, y + 9 * aimScale, x, y + 14 * aimScale, color);
    }
    const float fpsY = l.compact && l.height < 420 * s && boss_ && boss_->active
        ? status.y + status.height + 8 * s : l.height - l.margin - 14 * s;
    Label(font, TextFormat("%d fps", GetFPS()), {l.width - l.margin, fpsY},
           12 * s, 85 * s, kMuted, emphasis, true);
#ifdef KOUMAKYOU_DEBUG_FEATURES
    if (debugRender_)
        Label(font, TextFormat("XYZ %+.1f %+.1f %+.1f | %d bullets", player_.Position().x,
              player_.Position().y, player_.Position().z, static_cast<int>(bullets_.Pool().ActiveCount())),
              {l.margin, l.height - l.margin - 14 * s}, 12 * s, l.width * 0.6F, kInk);
#endif
}

void Game::DrawDialogue() const {
    if (!dialogueActive_ || !stageContent_) return;
    const GameplayLayout l(GetScreenWidth(), GetScreenHeight());
    const auto lines = CurrentDialogue();
    float start = 0;
    const int lineIndex = DialogueIndex(&start);
    const DialogueLine& line = lines[lineIndex];
    const auto expressions = DialogueExpressionsAt(lines, lineIndex);
    const float elapsed = dialogueTimer_ - start;
    const auto portrait = [&](VisualId id, bool right, bool speaking) {
        const auto& visual = assets_.GetVisual(id);
        const Texture2D texture = visual.texture;
        if (!texture.id) return;
        const Rectangle crop = visual.dialogueCrop;
        const Rectangle source{crop.x * texture.width, crop.y * texture.height,
                               crop.width * texture.width, crop.height * texture.height};
        if (source.width <= 0 || source.height <= 0) return;
        DrawTexturePro(texture, source, l.DialoguePortrait(source.width / source.height, right), {}, 0,
                        speaking ? WHITE : Color{115, 115, 130, 255});
    };
    // 先画未发言者，避免灵梦展开的手臂覆盖正在说话的角色；裁剪仍取自各差分的元数据。
    const auto hero = stageContent_->HeroPortraitVisual(expressions[0]);
    const auto boss = stageContent_->BossPortraitVisual(expressions[1]);
    if (line.bossSpeaking) {
        portrait(hero, false, false);
        if (line.showBoss) portrait(boss, true, true);
    } else {
        if (line.showBoss) portrait(boss, true, false);
        portrait(hero, false, true);
    }
    const Font font = assets_.Common().uiFont;
    const Rectangle panel = l.DialoguePanel();
    if (elapsed >= line.textDelay) {
        Panel(panel, l.scale);
        const float x = panel.x + 22 * l.scale, w = panel.width - 44 * l.scale;
        Label(font, line.speaker, {x, panel.y + 12 * l.scale}, 17 * l.scale, w, kGold);
        // 每句的两个原文行共享字号，长句按可用宽度收敛，不能静默裁掉末尾或改变等待时序。
        const float size = std::min(ui::FittedSize(font, line.text, 22 * l.scale, w),
                                    ui::FittedSize(font, line.secondLine, 22 * l.scale, w));
        Label(font, line.text, {x, panel.y + 43 * l.scale}, size, w, kInk);
        if (elapsed >= line.textDelay + line.line2Delay)
            Label(font, line.secondLine, {x, panel.y + 77 * l.scale}, size, w, kInk);
    }
    if (line.introduce) {
        const float w = std::min(l.width * 0.42F, 250 * l.scale);
        const Rectangle nameplate{l.width - l.margin - w, panel.y - 106 * l.scale, w, 88 * l.scale};
        Panel(nameplate, l.scale);
        const float x = nameplate.x + 14 * l.scale, textWidth = w - 28 * l.scale;
        Label(font, "宵暗的妖怪", {x, nameplate.y + 12 * l.scale}, 16 * l.scale, textWidth, kMuted);
        Label(font, "露米娅", {x, nameplate.y + 39 * l.scale}, 28 * l.scale, textWidth, kInk);
    }
    if (dialogueAfterBattle_) {
        const char* bonus = TextFormat("BONUS %lld", stageBonus_);
        const float size = ui::FittedSize(font, bonus, 21 * l.scale, l.width - 2 * l.margin);
        ui::DrawCentered(font, bonus, l.width / 2, panel.y - 31 * l.scale, size, kGold);
    }
}

void Game::DrawSpellCutIn() const {
    if (spellCutInTimer_ <= 0 || !boss_ || !boss_->active || !boss_->SpellName()) return;
    const GameplayLayout l(GetScreenWidth(), GetScreenHeight());
    const float elapsed = kSpellCutInDuration - spellCutInTimer_;
    const float opacity = std::clamp(std::min(elapsed / 0.2F, spellCutInTimer_ / 0.4F), 0.0F, 1.0F);
    const Texture2D texture = assets_.GetVisual(stageContent_->SpellPortraitVisual()).texture;
    if (texture.id && texture.height > 0) {
        // 只以窗口边缘裁掉滑入阶段的离屏部分；完整全身差分保持宽高比和原有淡入淡出时序。
        BeginScissorMode(0, 0, GetScreenWidth(), GetScreenHeight());
        const float slide = 1 - std::clamp(elapsed / 0.4F, 0.0F, 1.0F);
        DrawTexturePro(texture, {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
                        l.SpellPortrait(static_cast<float>(texture.width) / texture.height, slide),
                        {}, 0, Fade(WHITE, opacity));
        EndScissorMode();
    }
    const Rectangle banner = l.SpellBanner();
    Panel(banner, l.scale, opacity);
    const float size = ui::FittedSize(assets_.Common().uiFont, boss_->SpellName(), 25 * l.scale,
                                     banner.width - 32 * l.scale);
    ui::DrawCentered(assets_.Common().uiFont, boss_->SpellName(), banner.x + banner.width / 2,
                     banner.y + (banner.height - size) / 2, size, Fade(kInk, opacity));
}
} // namespace koumakyou
