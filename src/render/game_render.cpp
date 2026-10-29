// 本文件负责全窗口三维世界、加载页、设置页面与暂停/结算叠加层的绘制。

#include "app/game.hpp"
#include "app/constants.hpp"
#include "app/math.hpp"
#include "config/settings.hpp"
#include "stages/stage1/scenery.hpp"
#include "render/gameplay_layout.hpp"
#include "render/menu_layout.hpp"
#include "ui/page.hpp"
#include "ui/typography.hpp"
#include "ui/effects.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>

namespace koumakyou {
using namespace game_math;

#include "rlgl.h"

void Game::Draw() const {
    // 最小化时某些后端可返回零尺寸；暂停绘制，避免 3D 视口换算除零或创建无效裁切区域。
    if(GetScreenWidth()<=0 || GetScreenHeight()<=0)return;
    if(state_==RunState::StartSetup || (state_==RunState::MainMenu && startSetupMotion_.Setup()>0)) {
        DrawStartSetup();
    } else if (state_ == RunState::Loading) {
        DrawLoading();
    } else if (state_ == RunState::MainMenu) {
        DrawMainMenu();
    } else if (state_ == RunState::Options) {
        DrawOptions();
    } else if (state_ == RunState::StartSetup || state_ == RunState::MusicRoom ||
               state_ == RunState::Score || state_ == RunState::Credit) {
        DrawMenuPage();
    } else {
        DrawWorld();
        DrawHud();
        DrawOverlay();
    }
    // 唯一的全局按键提示出口；在所有页面、演出与模态上方绘制，避免遗漏或叠画多个旧页脚。
    DrawCommandBar();
}

void Game::DrawLoading() const {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = ui::CommandBar::ContentHeight(width,GetScreenHeight());
    // 以短边限制字号和图案，锚定实际右下角；超宽和竖屏均保持等比与边距。
    const float scale = std::min(width / 600.0F, height / 760.0F);
    const float right = width - 48 * scale;
    const float baseline = height - 58 * scale;
    const Font font = assets_.Common().uiFont;
    const Color background{9, 7, 12, 255};
    const Color vermilion{191, 57, 69, 255};
    const Color ivory{240, 224, 210, 255};
    const float pulse = 0.72F + 0.18F * std::sin(loadingSeconds_ * 2.4F);
    ClearBackground(background);

    const char* title = loadingFailed_ ? "加载失败" : "少女祈祷中";
    const float titleSize = 34 * scale;
    const float titleWidth = ui::MeasureText(font, title, titleSize).x;
    const float left = right - titleWidth - 38 * scale;
    const Vector2 seal{left - 57 * scale, baseline - 2 * scale};
    ui::YinYang(seal, 23 * scale, vermilion, ivory, loadingSeconds_ * 2);
    ui::GlowText(font, title, {left, baseline - 30 * scale}, titleSize,
                 ui::WithOpacity(ivory, pulse + 0.1F), vermilion, pulse);
    if(!loadingFailed_)ui::DrawText(font, "Now Loading...",
                 {left + 2 * scale, baseline + 14 * scale}, 13 * scale,
                 ui::WithOpacity(ivory, 0.45F));
    if (loadingFailed_) return;

    for (int index = 0; index < 3; ++index) {
        const float opacity = 0.25F + 0.75F * std::pow(
            0.5F + 0.5F * std::sin(loadingSeconds_ * 4 - index * 0.9F), 2.0F);
        ui::Spark({right - (25 - index * 10) * scale, baseline - 9 * scale},
                  2.4F * scale, ui::WithOpacity(vermilion, opacity));
    }
    const float lineY = baseline + 38 * scale;
    DrawLineEx({left, lineY}, {right, lineY}, scale, ui::WithOpacity(ivory, 0.12F));
    const float progress = assets_.StageLoadProgress();
    if (progress > 0)
        DrawLineEx({left, lineY}, {left + (right - left) * progress, lineY},
                   scale, ui::WithOpacity(vermilion, 0.8F));
}

void Game::DrawWorld() const {
    const GameplayLayout layout(GetScreenWidth(),GetScreenHeight());
    const Rectangle viewport=layout.Viewport();
    // 默认窗口的 rlgl 尺寸是逻辑像素，macOS Retina 实际视口需用 GetRenderWidth/Height；
    // 离屏 FBO 则已按纹理像素建立，不能再次乘 DPI，否则截图会裁切到左下角。
    const bool windowTarget=rlGetActiveFramebuffer()==0;
    const int framebufferWidth=windowTarget ? GetRenderWidth() : rlGetFramebufferWidth();
    const int framebufferHeight=windowTarget ? GetRenderHeight() : rlGetFramebufferHeight();
    rlDrawRenderBatchActive();
    // 全窗口直接使用整数像素边界，避免 125% 等缩放下浮点回算少覆盖最后一列。
    rlViewport(0,0,framebufferWidth,framebufferHeight);
    rlEnableScissorTest();
    rlScissor(0,0,framebufferWidth,framebufferHeight);
    BeginMode3D(camera_);
    // 原生窗口与离屏截图共用当前 framebuffer；只更改视口和投影，不创建/切换额外 FBO。
    rlMatrixMode(RL_PROJECTION); rlLoadIdentity();
    const double top=rlGetCullDistanceNear()*std::tan(camera_.fovy*DEG2RAD/2);
    const double right=top*viewport.width/viewport.height;
    rlFrustum(-right,right,-top,top,rlGetCullDistanceNear(),rlGetCullDistanceFar());
    rlMatrixMode(RL_MODELVIEW);
    if (assets_.Common().skyboxModel.meshCount != 0) {
        // 天空盒仅提供远景，不写入深度，确保战场和角色正常显示在前方。
        rlDisableBackfaceCulling();
        rlDisableDepthMask();
        DrawModel(assets_.Common().skyboxModel, {}, 1.0F, WHITE);
        rlDrawRenderBatchActive();
        rlEnableDepthMask();
        rlEnableBackfaceCulling();
    }
    if (stageId_ == StageId::Stage1) stage1::DrawScenery(stage_.Elapsed());
    DrawArena();
    std::array<const Enemy*, kEnemyCapacity> visibleEnemies{};
    std::size_t visibleCount = 0;
    enemies_.ForEachActive([&](const Enemy& enemy) {
        visibleEnemies[visibleCount++] = &enemy;
    });
    const std::span<const Enemy*> visible(visibleEnemies.data(), visibleCount);
    pickupSprites_.clear();
    playerBombVolumes_.clear();
    if (stageContent_) {
        drops_.ForEachActive([&](const Pickup& pickup) {
            VisualId visual = stageContent_->ScorePickupVisual();
            if (pickup.kind == PickupKind::Power)
                visual = pickup.value>=8 ? stageContent_->LargePowerPickupVisual() : stageContent_->PowerPickupVisual();
            else if (pickup.kind == PickupKind::Star)
                visual = stageContent_->StarPickupVisual();
            const float scale = assets_.GetVisual(visual).archive ? 1.0F : pickup.kind == PickupKind::Power &&
                                        pickup.value >= 8 ? 1.25F : 0.9F;
            pickupSprites_.push_back({pickup.position, visual, scale,&pickup.animation.player.Sample(),255,pickup.RotationRadians()});
        });
        if (player_.OptionsVisible()) {
            const auto positions = player_.OptionPositions();
            for (int side = 0; side < 2; ++side) {
                const auto visual = stageContent_->PlayerOptionVisual(side,player_.Shot());
                if (visual == kInvalidVisualId || !assets_.GetVisual(visual).archive) continue;
                // 与道具共用透明公告板批次，保证圈与副弹按真实深度排序；这些绘制数据不参与拾取或碰撞。
                pickupSprites_.push_back({positions[side], visual, player_.OptionScale(),
                                          &playerOptionAnimations_[side].player.Sample(), Player::kShotAlpha});
            }
        }
        if(playerBomb_.Active()) {
            const auto visuals=stageContent_->PlayerBombVisuals(playerBomb_.Shot());
            for(const auto& sprite:playerBomb_.Sprites()) {
                const auto visual=visuals[sprite.layer];
                if(visual==kInvalidVisualId)continue;
                playerBombVolumes_.push_back({&sprite,visual});
            }
        }
    }
    DrawPlayerBombBackdrop(viewport.width,viewport.height);
    combatRenderer_.Prepare(assets_.Projectiles(),bullets_,assets_.Stage().visuals,camera_,viewport.width/viewport.height,stage_.Elapsed());
    spriteRenderer_.Draw(camera_, bullets_, visible, boss_.get(),
                         pickupSprites_,
                         assets_.Stage().visuals, assets_.Stage().shaders,
                         actorEffects_,playerBombVolumes_,&combatRenderer_);
#ifdef KOUMAKYOU_DEBUG_FEATURES
    if (debugRender_) {
        spriteRenderer_.DrawDebugGeometry(bullets_, visible, boss_.get(),
                                          player_.Position(),assets_.Projectiles(),assets_.Stage().visuals);
    }
#endif

    EndMode3D();
    rlDisableScissorTest();
    rlViewport(0,0,framebufferWidth,framebufferHeight);
}

void Game::DrawArena() const {
    // 辅助网格是半透明线，不能用深度写入切掉后续角色/掉落物的像素。
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    constexpr Vector3 h = kArenaHalfExtents;
    const Color border{170, 105, 150, 190};
    const Color grid{100, 65, 115, 24};
    DrawCubeWires({}, h.x * 2.0F, h.y * 2.0F, h.z * 2.0F, border);

    constexpr float step = 2.5F;
    // 各轴独立铺设网格；以原点对称排列，Z 长度不是间距整数倍时两端留白仍相等。
    for (float x = -h.x + step; x < h.x; x += step) {
        DrawLine3D({x, -h.y, -h.z}, {x, -h.y, h.z}, grid);
        DrawLine3D({x, h.y, -h.z}, {x, h.y, h.z}, grid);
        DrawLine3D({x, -h.y, -h.z}, {x, h.y, -h.z}, grid);
        DrawLine3D({x, -h.y, h.z}, {x, h.y, h.z}, grid);
    }
    for (float y = -h.y + step; y < h.y; y += step) {
        DrawLine3D({-h.x, y, -h.z}, {-h.x, y, h.z}, grid);
        DrawLine3D({h.x, y, -h.z}, {h.x, y, h.z}, grid);
        DrawLine3D({-h.x, y, -h.z}, {h.x, y, -h.z}, grid);
        DrawLine3D({-h.x, y, h.z}, {h.x, y, h.z}, grid);
    }
    const int zSteps = static_cast<int>(std::ceil(h.z / step)) - 1;
    for (int index = -zSteps; index <= zSteps; ++index) {
        const float z = index * step;
        DrawLine3D({-h.x, -h.y, z}, {h.x, -h.y, z}, grid);
        DrawLine3D({-h.x, h.y, z}, {h.x, h.y, z}, grid);
        DrawLine3D({-h.x, -h.y, z}, {-h.x, h.y, z}, grid);
        DrawLine3D({h.x, -h.y, z}, {h.x, h.y, z}, grid);
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

void Game::DrawOverlay() const {
    const Font font = assets_.Common().uiFont;
    const menu::OverlayLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto& canvas=layout.canvas;
    const GameplayLayout gameplay(GetScreenWidth(), GetScreenHeight());
    // 反馈只属于战斗与收尾；结果菜单已有完整计分，继续绘制奖励条会压住第三项“重试”。
    const bool battleFeedback=state_==RunState::Playing || state_==RunState::Finishing ||
        (state_==RunState::Paused && (pausedState_==RunState::Playing || pausedState_==RunState::Finishing));
    if (battleFeedback && !dialogueActive_ && spellCutInTimer_ <= 0 &&
        (messageTimer_ > 0 || spellBonusTimer_ > 0)) {
        const Rectangle feedback = gameplay.Feedback(boss_ && boss_->active);
        const char* message = messageTimer_ > 0 ? message_.c_str()
            : TextFormat("Spell Card Bonus  %lld", spellBonus_);
        const Font hudFont = assets_.Common().hudFont;
        const float size = ui::FittedSize(hudFont, message, 23 * gameplay.scale,
                                          feedback.width - 28 * gameplay.scale);
        DrawRectangleRounded(feedback, 0.15F, 8, Color{12, 7, 20, 185});
        ui::DrawCentered(hudFont, message, feedback.x + feedback.width / 2,
                         feedback.y + (feedback.height - size) / 2, size,
                         messageTimer_ > 0 ? RAYWHITE : GOLD);
    }

    DrawDialogue();
    if(playerBomb_.Active())DrawPlayerBombCutIn();else DrawSpellCutIn();
    if(state_==RunState::StageClear || (state_==RunState::Paused && pausedState_==RunState::StageClear))DrawStageClear();

    // 暂停与符卡结果共用底板和文字操作样式，禁用项只降透明度，不产生选中动效。
    ui::DrawContext modalContext{font,canvas.scale,1,menuMotion_.Phase(),menuMotion_.Pulse()};
    modalContext.theme.accent=GOLD;
    const auto drawModal=[&](Rectangle panel,const char* title,Color color) {
        ui::Modal{{0,0,float(GetScreenWidth()),float(GetScreenHeight())},panel,canvas.Point(380,200)}.Draw(modalContext,title,color);
    };
    const auto drawModalRow=[&](Rectangle row,const char* label,int index,bool available=true) {
        const float weight=available?menuMotion_.Weight(index):0;
        const float entrance=menuMotion_.Reveal(index*0.02F);
        ui::Button{row,available,ui::ButtonStyle::Text}.Draw(modalContext,label,{weight,entrance});
    };
    if (state_ == RunState::Paused) {
        const auto labels=menu::PauseLabels(practiceMode_ && !spellPractice_,spellPractice_);
        drawModal(layout.PausePanel(static_cast<int>(labels.size())),"暂停",RAYWHITE);
        for(int index=0;index<static_cast<int>(labels.size());++index)
            drawModalRow(layout.PauseRow(index),labels[index],index);
    } else if ((state_==RunState::Won || state_==RunState::GameOver) && spellPractice_) {
        drawModal(layout.SpellResultPanel(),spellCaptured_?"Success!":"Failed...",spellCaptured_?GOLD:Color{255,90,110,255});
        // 统计位于标题和操作之间，按底板宽度适配极限计数，避免窄窗口中文按钮与计分相互覆盖。
        const auto detail=[&](const char* text,float y,float size) {
            const float fitted=ui::FittedSize(font,text,size*canvas.scale,420*canvas.scale);
            ui::DrawCentered(font,text,canvas.Point(380,0).x,canvas.Point(0,y).y,fitted,RAYWHITE);
        };
        detail(TextFormat("SCORE %09lld",score_),252,22);
        detail(TextFormat("GRAZE %d   MISS %d   BOMB %d",graze_,missCount_,bombCount_),284,18);
        for(int index=0;index<static_cast<int>(menu::kSpellResultLabels.size());++index)
            drawModalRow(layout.SpellResultRow(index),menu::kSpellResultLabels[index],index,menu::kSpellResultAvailable[index]);
    } else if (state_ == RunState::Won || state_ == RunState::GameOver) {
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Color{0,0,0,175});
        const float center=canvas.Point(380,0).x;
        const auto centered=[&](const char* text,float y,float size,Color color) {
            const float fitted=ui::FittedSize(font,text,size*canvas.scale,660*canvas.scale);
            ui::DrawCentered(font,text,center,canvas.Point(0,y).y,fitted,color);
        };
        const char* title=state_==RunState::Won ? stageContent_->ClearTitle() : "GAME OVER";
        centered(title,205,56,state_==RunState::Won?GOLD:Color{255,90,110,255});
        // 统计拆成两行，最大分数/擦弹数也不会挤出窄窗口或与重试提示重叠。
        centered(TextFormat("SCORE %09lld",score_),295,26,RAYWHITE);
        centered(TextFormat("GRAZE %d   MISS %d   BOMB %d",graze_,missCount_,bombCount_),337,22,RAYWHITE);
        centered(practiceMode_?"STAGE PRACTICE":challengeMode_?"CHALLENGE":"NORMAL",382,18,GOLD);
        for(int index=0;index<menu::kResultCount;++index) {
            const auto row=layout.ResultRow(index);
            const float weight=menuMotion_.Weight(index),reveal=menuMotion_.Reveal(index*0.02F);
            ui::DrawPageButton(canvas,font,row,index==1 && practiceMode_?"返回关卡选择画面":menu::kResultLabels[index],menuMotion_,weight,reveal);
        }
    }

    if (screenFlashTimer_ > 0.0F) {
        const unsigned char alpha = static_cast<unsigned char>(
            std::clamp(screenFlashTimer_ / 0.35F, 0.0F, 1.0F) * 145.0F);
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Color{220, 240, 255, alpha});
    }
}

void Game::DrawStageClear() const {
    const menu::OverlayLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto& canvas=layout.canvas;
    const float s=canvas.scale;
    const Font font=assets_.Common().hudFont;
    const Color gold{233,204,139,255},ivory{252,246,236,255},rose{207,119,132,255};
    const float reveal=std::clamp(stageClearSeconds_/0.22F,0.0F,1.0F);
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Color{0,0,0,115});
    DrawRectangleRounded(layout.ClearPanel(),0.035F,8,Fade(Color{29,10,22,240},0.7F+0.3F*reveal));
    DrawRectangleRoundedLinesEx(layout.ClearPanel(),0.035F,8,s,Fade(gold,0.7F));
    const float center=canvas.Point(380,0).x;
    ui::DrawCentered(font,"Stage Clear",center,canvas.Point(0,122).y,38*s,gold);
    ui::DrawCentered(font,TextFormat("STAGE %d · %s",stageClearSummary_.stage,challengeMode_?"挑战模式":"标准模式"),
                     center,canvas.Point(0,171).y,18*s,rose);
    const auto row=[&](const char* label,const char* value,float y,Color color,float size=26) {
        ui::DrawText(font,label,canvas.Point(120,y),size*s,color);
        const float fitted=ui::FittedSize(font,value,size*s,280*s);
        const auto right=canvas.Point(640,y);
        ui::DrawText(font,value,{right.x-ui::MeasureText(font,value,fitted).x,right.y},fitted,color);
    };
    row("Stage * 1000 =",TextFormat("%lld",stageClearSummary_.StagePoints()),214,ivory);
    row("Power * 100 =",TextFormat("%lld",stageClearSummary_.PowerPoints()),251,ivory);
    row("Graze * 10 =",TextFormat("%lld",stageClearSummary_.GrazePoints()),288,ivory);
    row("* Point Item",TextFormat("%d",stageClearSummary_.pointItems),325,rose);
    row("Normal Rank","* 1.0",380,rose,24);
    const auto left=canvas.Point(120,416),right=canvas.Point(640,416);
    DrawLineEx(left,right,s,Fade(gold,0.6F));
    row("Total =",TextFormat("%lld",stageClearSummary_.Total()),431,gold,32);
    ui::DrawCentered(font,TextFormat("%ds",static_cast<int>(std::ceil(kStageClearSeconds-stageClearSeconds_))),
                     center,canvas.Point(0,493).y,18*s,ivory);
}

void Game::DrawKeyBindings() const {
    const Font font=assets_.Common().uiFont;
    const menu::BindingsLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto& canvas=layout.canvas;
    const float scale=canvas.scale;
    const Color gold=ui::kPageGold;
    ui::DrawPageSurface(canvas,menuMotion_);
    ui::DrawPageHeader(canvas,font,"按键设置","选择操作后，按下新的键盘或鼠标按键","KEYBINDINGS");
    const int resetIndex=static_cast<int>(kBindingNames.size());
    for(int index=0;index<=resetIndex;++index) {
        const auto row=layout.Row(index);
        const float weight=menuMotion_.Weight(index),reveal=menuMotion_.Reveal(index*0.012F);
        const auto label=layout.LabelOrigin(index);
        ui::DrawPageRowRule(canvas,row,menuMotion_,weight,reveal);
        ui::DrawPageChoiceText(canvas,font,index==resetIndex?"恢复默认键位":kBindingNames[index],label.x,label.y,layout.compact?24:22,
                               Color{177,150,143,255},gold,weight,reveal,menuMotion_.Pulse());
        if(index==resetIndex)continue;
        const bool capturing=waitingForBinding_ && index==selectedBinding_;
        const auto key=capturing?std::string("请按下新键"):KeyLabel(BindingAt(index));
        const float keySize=ui::FittedSize(font,key.c_str(),23*scale,row.width*0.42F)/scale;
        const Color grey{145,145,150,255};
        ui::DrawPageChoiceText(canvas,font,key.c_str(),layout.compact?552:795,label.y,keySize,
                               capturing?grey:ui::kPageIvory,capturing?grey:gold,weight,reveal,
                               capturing?0:menuMotion_.Pulse()+menuMotion_.Flash(),true);
    }
}

}  // namespace koumakyou
