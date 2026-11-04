// 本文件绘制全窗口战斗的两侧悬浮 HUD、Boss 信息、对话及符卡演出，不推进任何游戏时钟。
#include "app/game.hpp"
#include "app/constants.hpp"
#include "render/gameplay_layout.hpp"
#include "render/boss_hud.hpp"
#include "ui/effects.hpp"
#include "ui/typography.hpp"

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

/// @brief 围绕目标球投影绘制四个朝内的锁定箭头；age 使用玩家游戏秒数，暂停时不会漂移。
/// 世界球半径映射为屏幕尺寸，再留出精灵外缘与浮动间距；不移动中央准星或保存敌方指针。
void DrawAimLock(const Camera3D& camera,const PlayerAimResult& aim,double age,float scale) {
    if(!aim.assisted || aim.targetRadius<=0)return;
    const auto view=Vector3{camera.target.x-camera.position.x,camera.target.y-camera.position.y,camera.target.z-camera.position.z};
    const auto relative=Vector3{aim.targetPosition.x-camera.position.x,aim.targetPosition.y-camera.position.y,aim.targetPosition.z-camera.position.z};
    if(view.x*relative.x+view.y*relative.y+view.z*relative.z<=0)return;
    const auto center=GetWorldToScreen(aim.targetPosition,camera);
    if(center.x<0 || center.x>GetScreenWidth() || center.y<0 || center.y>GetScreenHeight())return;
    const auto top=GetWorldToScreen({aim.targetPosition.x,aim.targetPosition.y+aim.targetRadius,aim.targetPosition.z},camera);
    const float projectedRadius=std::hypot(top.x-center.x,top.y-center.y);
    const float gap=std::clamp(projectedRadius*1.8F,18*scale,140*scale)+9*scale;
    // 四箭头同步向内外浮动，保持朝向和目标中心一致，不能旋转成方向含糊的装饰环。
    const float floating=4*scale*std::sin(static_cast<float>(age)*2*PI*1.2F);
    constexpr std::array<Vector2,4> directions{{{0,-1},{1,0},{0,1},{-1,0}}};
    for(const auto direction:directions) {
        const Vector2 side{-direction.y,direction.x};
        const auto point=[&](float radial,float lateral) {
            return Vector2{center.x+direction.x*radial+side.x*lateral,center.y+direction.y*radial+side.y*lateral};
        };
        const float tip=gap+floating;
        const auto a=point(tip,0),b=point(tip+11*scale,-5*scale),c=point(tip+11*scale,5*scale);
        // raylib 的三角形顶点使用逆时针顺序；深色描边在红雾和明亮精灵前都保持可辨识。
        DrawTriangle(a,c,b,Color{255,221,143,235});
        DrawLineEx(a,b,1.3F*scale,Color{52,22,31,220});
        DrawLineEx(b,c,1.3F*scale,Color{52,22,31,220});
        DrawLineEx(c,a,1.3F*scale,Color{52,22,31,220});
    }
}

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

/// @brief 按新典题签的暖金斜排与酒红斜带绘制关卡名称；坐标为窗口逻辑像素，字体和名称仅借用。
/// 原图文字与竖长侧栏合在一起；在紧凑浮窗中以较小倾角重排，保留完整标题及副标题，不压缩 3D 视口。
void StageTitle(Font font, const StageDisplayInfo& info, Rectangle panel, float scale, float opacity) {
    const float left = panel.x + 2 * scale, right = panel.x + panel.width - 2 * scale;
    const Vector2 a{left, panel.y + 55 * scale}, b{left, panel.y + 124 * scale};
    const Vector2 c{right, panel.y + 72 * scale}, d{right, panel.y + 3 * scale};
    const Color band = ui::WithOpacity(Color{90, 22, 39, 170}, opacity);
    DrawTriangle(a, b, c, band);
    DrawTriangle(a, c, d, band);
    DrawLineEx(a, d, scale, ui::WithOpacity(Color{230, 196, 141, 75}, opacity));
    DrawLineEx(b, c, scale, ui::WithOpacity(Color{230, 196, 141, 45}, opacity));

    constexpr float angle = -14.0F;
    const float radians = angle * DEG2RAD;
    const Vector2 origin{panel.x + 16 * scale, panel.y + 41 * scale};
    const auto text = [&](std::string_view value, float y, float size, Color color) {
        const std::string content(value);
        size = ui::FittedSize(font, content.c_str(), size * scale, panel.width - 38 * scale);
        const Vector2 position{origin.x - y * scale * std::sin(radians),
                               origin.y + y * scale * std::cos(radians)};
        // 阴影随文字旋转但保持屏幕方向的小偏移，保证明亮背景上金色笔画仍清晰。
        DrawTextPro(font, content.c_str(), {position.x + scale, position.y + scale}, {}, angle,
                    size, size * ui::kTrackingRatio, ui::WithOpacity(Color{43, 11, 19, 245}, opacity));
        DrawTextPro(font, content.c_str(), position, {}, angle, size, size * ui::kTrackingRatio,
                    ui::WithOpacity(color, opacity));
    };
    text(info.label, 0, 16, Color{255, 231, 166, 255});
    text(info.title, 27, 30, Color{255, 238, 175, 255});
    text("~ " + std::string(info.subtitle), 58, 17, Color{251, 222, 157, 255});
}

/// @brief 在资源值区域右对齐绘制图标；count 为资源数量，bounds/size 使用屏幕像素，font 仅借用。
/// icon 接收中心、外接尺寸和颜色；最多逐颗显示五个，更多时显示单个图标与数量。
/// 图标按共同五格宽度缩放，数量文字单独适配，保证两行图标不会因数量不同而大小不一；opacity 沿用 HUD 透明度。
void ResourceStock(Font font,int count,Rectangle bounds,float size,Color color,
                   void (*icon)(Vector2,float,Color),float opacity) {
    if(count<=0) {
        Label(font,"0",{bounds.x+bounds.width,bounds.y},size,bounds.width,color,opacity,true);
        return;
    }
    const int icons=count>5?1:count;
    const std::string caption=count>5?"x "+std::to_string(count):"";
    const float iconSize=size*0.76F,gap=size*0.24F;
    const float scale=std::min(1.0F,bounds.width/(5*iconSize+4*gap));
    const float captionSize=ui::FittedSize(font,caption.c_str(),size*scale,std::max(0.1F,bounds.width-(iconSize+gap)*scale));
    const float captionWidth=caption.empty()?0:gap*scale+ui::MeasureText(font,caption.c_str(),captionSize).x;
    const float width=(icons*iconSize+(icons-1)*gap)*scale+captionWidth;
    const float x=bounds.x+bounds.width-width;
    for(int index=0;index<icons;++index)
        icon({x+(index*(iconSize+gap)+iconSize/2)*scale,bounds.y+size/2},iconSize*scale,ui::WithOpacity(color,opacity));
    if(!caption.empty())
        Label(font,caption,{bounds.x+bounds.width,bounds.y+(size-captionSize)/2},captionSize,
              bounds.width,color,opacity,true);
}
}

void Game::DrawHud() const {
    const GameplayLayout l(GetScreenWidth(), GetScreenHeight());
    const float s = l.scale;
    const Font font = assets_.Common().hudFont;
    // 演出时保留关键状态但降低强调，立绘/台词使用独立图层，不被侧栏文字争抢视觉焦点。
    const float emphasis = dialogueActive_ || spellCutInTimer_ > 0 ? 0.38F : 1.0F;
    const Rectangle stage = l.StagePanel(), status = l.StatusPanel();
    Panel(stage, s, emphasis);
    if (stageContent_) {
        const auto info = stageContent_->DisplayInfo();
        const float x = stage.x + 16 * s, w = stage.width - 32 * s;
        StageTitle(font, info, stage, s, emphasis);
        constexpr std::array<const char*,5> difficultyNames{"EASY","NORMAL","HARD","LUNATIC","EXTRA"};
        const auto mode=std::string(difficultyNames[ScoreDifficultyIndex()])+
            (spellPractice_ ? " / 符卡练习" : practiceMode_ ? " / 关卡练习" : challengeMode_ ? " / 挑战模式" : "");
        Label(font, mode.c_str(), {x, stage.y + (l.compact ? 118 : 124) * s}, 16 * s, w,
               kRose, emphasis);
    }

    Panel(status, s, emphasis);
    const float pad = (l.compact ? 13 : 16) * s;
    const float left = status.x + pad, right = status.x + status.width - pad;
    const float contentWidth = right - left;
    const auto row = [&](const char* name, std::string_view value, float y,
                         float valueSize, Color valueColor = kInk) {
        // 四字标签与九位分数分列，避免字号放大后又因旧列宽被自动缩回。
        Label(font, name, {left, status.y + y * s}, (l.compact ? 17 : 19) * s,
               78 * s, kMuted, emphasis);
        Label(font, value, {right, status.y + (y - 1) * s}, valueSize * s,
               contentWidth - 84 * s, valueColor, emphasis, true);
    };
    if (!l.compact)
        Label(font, "状态", {left, status.y + 13 * s}, 16 * s, contentWidth, kGold, emphasis);
    row("得分", TextFormat("%09lld", score_), l.compact ? 12 : 48, l.compact ? 22 : 27);
    const auto& namedScores=scoreProfile_.Get(challengeMode_,static_cast<int>(player_.Shot()),ScoreDifficultyIndex());
    // 展开技能编号只在各自工厂内有效；读取全局Card编号，保证单符工厂的attack=1不会串到一面成绩。
    const auto* practiceCard=spell::Find(spellSelection_.stage,spellSelection_.group,spellSelection_.difficulty);
    const long long displayedBest=spellPractice_?spellRecords_.Get(static_cast<int>(player_.Shot()),practiceCard?practiceCard->number:0).bestScore:
        namedScores[0]?namedScores[0]->value:challengeMode_||stageId_==StageId::Extra||runDifficulty_!=native_combat::Difficulty::Normal?0:bestScore_;
    row("最高得分", TextFormat("%09lld", displayedBest), l.compact ? 42 : 88, l.compact ? 16 : 18);
    if (!l.compact)
        DrawLineEx({left, status.y + 118 * s}, {right, status.y + 118 * s}, s,
                    ui::WithOpacity(Color{210, 173, 116, 65}, emphasis));
    const float livesY=l.compact?70:132,bombY=l.compact?96:170,stockSize=(l.compact?19:24)*s;
    row(challengeMode_ ? "MISS" : "Player",challengeMode_ ? std::to_string(missCount_) : "",
        livesY,l.compact ? 19 : 24,kRose);
    if(!challengeMode_)
        ResourceStock(font,player_.RemainingLives(),{left+84*s,status.y+(livesY-1)*s,contentWidth-84*s,stockSize},
                      stockSize,kRose,ui::Heart,emphasis);
    row("Bomb","",bombY,l.compact ? 19 : 24,kGreen);
    ResourceStock(font,player_.Bombs(),{left+84*s,status.y+(bombY-1)*s,contentWidth-84*s,stockSize},
                  stockSize,kGreen,ui::FivePointStar,emphasis);
    row("Power", player_.Power() == Player::kMaxPower ? "MAX" : TextFormat("%d / 128", player_.Power()),
        l.compact ? 122 : 208, l.compact ? 17 : 20, kRose);
    const Rectangle power{left, status.y + (l.compact ? 145 : 236) * s, contentWidth, 4 * s};
    DrawRectangleRec(power, ui::WithOpacity(Color{65, 41, 56, 230}, emphasis));
    DrawRectangleRec({power.x, power.y, power.width * player_.Power() / Player::kMaxPower, power.height},
                      ui::WithOpacity(kRose, emphasis));
    if (l.compact) {
        Label(font, TextFormat("Graze %d", graze_), {left, status.y + 159 * s}, 16 * s,
               contentWidth * 0.52F, kGreen, emphasis);
        Label(font, TextFormat("点数 %d", pointItems_), {right, status.y + 159 * s}, 16 * s,
               contentWidth * 0.46F, kInk, emphasis, true);
    } else {
        row("Graze", TextFormat("%d", graze_), 254, 20, kGreen);
        row("点数", TextFormat("%d", pointItems_), 284, 20);
    }

    const auto bossHealth = boss_ ? boss_->HealthBar() : BossHealthBar{};
    if (bossHealth.count > 0 && !dialogueActive_) {
        const Rectangle panel = l.BossPanel();
        const ui::BossHudLayout row(panel, s);
        Panel(panel, s);
        Label(font, WorldTimeStopped() ? "TIME STOP" : TextFormat("Enemy %d", bossHealth.remainingBars),
               {row.enemy.x, row.enemy.y + (row.enemy.height - 21 * s) / 2}, 21 * s, row.enemy.width, kGold);
        boss_->DrawHealthBar(row.bar);
        const int seconds = ui::BossSeconds(boss_->TimeLimit(), boss_->skillTimer);
        Label(font, seconds < 0 ? "--" : TextFormat("%02d", seconds),
               {row.timer.x + row.timer.width, row.timer.y + (row.timer.height - 27 * s) / 2},
               27 * s, row.timer.width, ui::BossTimerColor(seconds), 1, true);
        if (boss_->SpellName())
            Label(font, boss_->SpellName(), {row.spell.x + row.spell.width, row.spell.y}, 19 * s,
                   row.spell.width, Color{220, 211, 255, 255}, 1, true);
    }

    if (!dialogueActive_ && state_ == RunState::Playing) {
        const Vector3 position = player_.Position();
        bool danger = false;
        bullets_.Pool().ForEachActive([&](const Bullet& bullet) {
            if (bullet.owner == BulletOwner::Enemy && bullet.Dangerous() &&
                bullet.DistanceSquaredTo(position) < 1.15F * 1.15F) danger = true;
        });
        DrawAimLock(camera_,player_.Aim(),player_.VisualAge(),s);
        const float aimScale = std::clamp(s, 0.75F, 1.5F);
        const Color warning{255,90,110,255};
        ui::AimReticle({l.width/2,l.height/2},aimScale,danger?warning:kGold,danger?warning:kInk,
                       danger?kInk:Color{173,61,65,255});
    }
    const float fpsY = l.compact && l.height < 540 * s && boss_ && boss_->active
        ? status.y + status.height + 8 * s : l.ContentBottom() - l.margin - 14 * s;
    Label(font, TextFormat("%d fps", GetFPS()), {l.width - l.margin, fpsY},
           12 * s, 85 * s, kMuted, emphasis, true);
#ifdef KOUMAKYOU_DEBUG_FEATURES
    if (debugRender_)
        Label(font, TextFormat("XYZ %+.1f %+.1f %+.1f | %d bullets", player_.Position().x,
              player_.Position().y, player_.Position().z, static_cast<int>(bullets_.Pool().ActiveCount())),
              {l.margin, l.ContentBottom() - l.margin - 14 * s}, 12 * s, l.width * 0.6F, kInk);
#endif
}

void Game::DrawDialogue() const {
    if (!dialogueActive_ || !stageContent_) return;
    const GameplayLayout l(GetScreenWidth(), GetScreenHeight());
    const auto lines = CurrentDialogue();
    if(lines.empty())return;
    float start = 0;
    const int lineIndex = DialogueIndex(&start);
    const DialogueLine& line = lines[lineIndex];
    const float elapsed = dialogueTimer_ - start;
    const auto expressions = DialogueCueExpressionsAt(
        stageContent_->SceneDialogueCues(dialogueScene_,dialogueAfterBattle_,player_.Shot()),dialogueTimer_,
        DialogueExpressionsAt(lines,lineIndex,elapsed));
    const auto portrait = [&](VisualId id, bool right) {
        const auto& visual = assets_.GetVisual(id);
        const Texture2D texture = visual.texture;
        if (!texture.id) return;
        const Rectangle crop = visual.dialogueCrop;
        const Rectangle source{crop.x * texture.width, crop.y * texture.height,
                               crop.width * texture.width, crop.height * texture.height};
        if (source.width <= 0 || source.height <= 0) return;
        const auto pose = dialogueMotion_.Sample(right);
        auto destination=l.DialoguePortrait(source.width / source.height, right, pose.focus);
        destination.x+=(right?1:-1)*destination.width*pose.slide;
        DrawTexturePro(texture, source, destination, {}, 0,
                        Fade(WHITE, pose.opacity));
    };
    // 先画未发言者，避免灵梦展开的手臂覆盖正在说话的角色；裁剪仍取自各差分的元数据。
    const auto hero = stageContent_->HeroPortraitVisual(expressions[0],player_.Shot());
    const auto boss = stageContent_->SceneBossPortraitVisual(dialogueScene_,expressions[1]);
    if (line.bossSpeaking) {
        portrait(hero, false);
        if (line.showBoss) portrait(boss, true);
    } else {
        if (line.showBoss) portrait(boss, true);
        portrait(hero, false);
    }
    const Font font = assets_.Common().hudFont;
    const Rectangle panel = l.DialoguePanel();
    const auto rows=DialogueTextRowsAt(line,elapsed);
    if (elapsed >= line.textDelay) {
        Panel(panel, l.scale);
        const float x = panel.x + 22 * l.scale, w = panel.width - 44 * l.scale;
        // 每句的两个原文行共享字号，长句按可用宽度收敛，不能静默裁掉末尾或改变等待时序。
        const float size = std::min(ui::FittedSize(font, line.text, 26 * l.scale, w),
                                    ui::FittedSize(font, line.secondLine, 26 * l.scale, w));
        if(line.secondLineSpeaker) {
            // 共同发言仍占一页，每行单独标名；保留完整宽度，窄屏也不会把任一方挤到框外。
            for(std::size_t row=0;row<rows.size();++row) {
                const float y=panel.y+(12+64*row)*l.scale;
                Label(font,rows[row].speaker,{x,y},20*l.scale,w,kGold);
                Label(font,rows[row].text,{x,y+28*l.scale},size,w,kInk);
            }
        } else {
            Label(font,rows[0].speaker,{x,panel.y+12*l.scale},20*l.scale,w,kGold);
            Label(font,rows[0].text,{x,panel.y+48*l.scale},size,w,kInk);
            Label(font,rows[1].text,{x,panel.y+89*l.scale},size,w,kInk);
        }
    }
    if (line.introduce&&elapsed>=line.introduceDelay) {
        const float w = std::min(l.width * 0.42F, 250 * l.scale);
        float y = panel.y - 120 * l.scale;
        // 竖屏角色靠近底角后，名牌需放在头部上方；跟随同一强调采样移动，不能盖住淡出的脸。
        if (l.width < l.height) {
            const auto& visual = assets_.GetVisual(boss);
            const float sourceHeight = visual.texture.height * visual.dialogueCrop.height;
            if (sourceHeight > 0) {
                const float aspect = visual.texture.width * visual.dialogueCrop.width / sourceHeight;
                if (aspect > 0) {
                    const auto rect = l.DialoguePortrait(aspect, true, dialogueMotion_.Sample(true).focus);
                    y = std::min(y, rect.y - 112 * l.scale);
                }
            }
        }
        const Rectangle nameplate{l.width - l.margin - w, std::max(l.margin, y), w, 100 * l.scale};
        Panel(nameplate, l.scale);
        const float x = nameplate.x + 14 * l.scale, textWidth = w - 28 * l.scale;
        const auto introduction=stageContent_->BossIntroduction();
        Label(font,introduction[0],{x,nameplate.y+12*l.scale},19*l.scale,textWidth,kMuted);
        Label(font,introduction[1],{x,nameplate.y+46*l.scale},32*l.scale,textWidth,kInk);
    }

}

void Game::DrawSpellCutIn() const {
    if (spellCutInTimer_ <= 0 || !boss_ || !boss_->active || !boss_->SpellName()) return;
    const GameplayLayout l(GetScreenWidth(), GetScreenHeight());
    const float elapsed = kSpellCutInDuration - spellCutInTimer_;
    const float opacity = std::clamp(std::min(elapsed / 0.2F, spellCutInTimer_ / 0.4F), 0.0F, 1.0F);
    const Texture2D texture = assets_.GetVisual(stageContent_->SpellPortraitVisual(boss_->SpellCardNumber())).texture;
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
    const float size = ui::FittedSize(assets_.Common().hudFont, boss_->SpellName(), 29 * l.scale,
                                     banner.width - 32 * l.scale);
    ui::DrawCentered(assets_.Common().hudFont, boss_->SpellName(), banner.x + banner.width / 2,
                     banner.y + (banner.height - size) / 2, size, Fade(kInk, opacity));
}
} // namespace koumakyou
