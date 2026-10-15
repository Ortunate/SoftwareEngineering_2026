// 本文件负责全窗口三维世界、加载页、设置页面与暂停/结算叠加层的绘制。

#include "app/game.hpp"
#include "app/constants.hpp"
#include "app/math.hpp"
#include "config/settings.hpp"
#include "stages/stage1/scenery.hpp"
#include "render/gameplay_layout.hpp"
#include "render/menu_layout.hpp"
#include "render/menu_page.hpp"
#include "render/ui_typography.hpp"
#include "render/ui_effects.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>

namespace koumakyou {
using namespace game_math;

#include "rlgl.h"

void Game::Draw() const {
    // 最小化时某些后端可返回零尺寸；暂停绘制，避免 3D 视口换算除零或创建无效裁切区域。
    if(GetScreenWidth()<=0 || GetScreenHeight()<=0)return;
    if(state_==RunState::StartSetup || (state_==RunState::MainMenu && startSetupMotion_.Setup()>0)) {
        DrawStartSetup();
        return;
    }
    if (state_ == RunState::Loading) {
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
}

void Game::DrawLoading() const {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
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
    ui::DrawText(font, loadingFailed_ ? "按确认键返回主菜单" : "Now Loading...",
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
    if (stageContent_) {
        drops_.ForEachActive([&](const Pickup& pickup) {
            VisualId visual = stageContent_->ScorePickupVisual();
            if (pickup.kind == PickupKind::Power)
                visual = pickup.value>=8 ? stageContent_->LargePowerPickupVisual() : stageContent_->PowerPickupVisual();
            else if (pickup.kind == PickupKind::Star)
                visual = stageContent_->StarPickupVisual();
            const float scale = assets_.GetVisual(visual).archive ? 1.0F : pickup.kind == PickupKind::Power &&
                                        pickup.value >= 8 ? 1.25F : 0.9F;
            pickupSprites_.push_back({pickup.position, visual, scale,&pickup.animation.player.Sample()});
        });
    }
    spriteRenderer_.Draw(camera_, bullets_, visible, boss_.get(),
                         pickupSprites_,
                         assets_.Stage().visuals, assets_.Stage().shaders,
                         actorEffects_);
#ifdef KOUMAKYOU_DEBUG_FEATURES
    if (debugRender_) {
        spriteRenderer_.DrawDebugGeometry(bullets_, visible, boss_.get(),
                                          player_.Position());
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
    // 通用反馈与符卡奖励共用底部区域；同一时刻优先显示通用事件，避免两块文字互相遮挡。
    if (!dialogueActive_ && spellCutInTimer_ <= 0 &&
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
    DrawSpellCutIn();

    if (state_ == RunState::Paused) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Color{0, 0, 0, 155});
        const auto center=canvas.Point(380,310);
        DrawRectangleRounded(layout.PausePanel(),0.1F,12,Color{38,12,25,225});
        ui::DrawCentered(font,"暂停",center.x,canvas.Point(0,200).y,43*canvas.scale,RAYWHITE);
        const auto& labels=menu::kPauseLabels;
        for(int index=0;index<menu::kPauseCount;++index) {
            const float weight=menuMotion_.Weight(index),reveal=menuMotion_.Reveal(index*0.02F);
            const float size=25*canvas.scale,width=ui::MeasureText(font,labels[index],size).x;
            const auto row=layout.PauseRow(index);
            ui::GlowText(font,labels[index],{center.x-width/2+(-8*weight+16*(1-reveal))*canvas.scale,row.y+4*canvas.scale},size,
                         Fade(ui::Blend(Color{190,153,164,255},GOLD,weight),reveal),GOLD,weight*menuMotion_.Pulse());
            ui::FocusLine({row.x+20*canvas.scale,row.y+row.height},{row.x+row.width-20*canvas.scale,row.y+row.height},canvas.scale,
                          GOLD,weight*reveal,menuMotion_.Phase());
        }
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
        centered(spellPractice_?"SPELL PRACTICE":practiceMode_?"STAGE PRACTICE":challengeMode_?"CHALLENGE":"NORMAL",382,18,GOLD);
        centered("R Restart     Esc / M Main Menu",430,22,RAYWHITE);
    }

    if (screenFlashTimer_ > 0.0F) {
        const unsigned char alpha = static_cast<unsigned char>(
            std::clamp(screenFlashTimer_ / 0.35F, 0.0F, 1.0F) * 145.0F);
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Color{220, 240, 255, alpha});
    }
}

void Game::DrawKeyBindings() const {
    const Font font=assets_.Common().uiFont;
    const menu::BindingsLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto& canvas=layout.canvas;
    const float scale=canvas.scale;
    const Color gold=menu::kPageGold;
    menu::DrawPageSurface(canvas,menuMotion_);
    menu::DrawPageHeader(canvas,font,"按键设置","选择操作后，按下新的键盘或鼠标按键","Key Bindings");
    for(int index=0;index<static_cast<int>(kBindingNames.size());++index) {
        const auto row=layout.Row(index);
        const float weight=menuMotion_.Weight(index),reveal=menuMotion_.Reveal(index*0.009F);
        const float size=(layout.compact?20:17)*scale,y=row.y+(layout.compact?10:6)*scale;
        menu::DrawPageRowRule(canvas,row,menuMotion_,weight,reveal);
        ui::GlowText(font,kBindingNames[index],{row.x+(15-6*weight+12*(1-reveal))*scale,y},size,
                     Fade(ui::Blend(Color{193,162,150,255},gold,weight),reveal),gold,weight*menuMotion_.Pulse());
        const auto key=KeyLabel(BindingAt(index));
        const float keySize=ui::FittedSize(font,key.c_str(),size,row.width*0.45F);
        ui::GlowText(font,key.c_str(),{row.x+row.width-16*scale-ui::MeasureText(font,key.c_str(),keySize).x,y},keySize,
                     Fade(ui::Blend(Color{212,190,184,255},gold,weight),reveal),gold,weight*(menuMotion_.Pulse()+menuMotion_.Flash()));
    }
    constexpr std::array<const char*,2> buttons{"恢复默认键位","返回"};
    for(int index=0;index<2;++index) {
        const float weight=menuMotion_.Weight(static_cast<int>(kBindingNames.size())+index);
        const auto button=layout.Button(index);
        menu::DrawPageButton(canvas,font,button,buttons[index],menuMotion_,weight);
    }
    if(layout.compact)
        menu::DrawPageFooter(canvas,font,"方向键 / W / S：选择   Z / 回车：修改","Esc：返回选项");
    else menu::DrawPageFooter(canvas,font,"方向键 / W / S：选择   Z / 回车：修改   Esc：返回选项");
    if(waitingForBinding_) {
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Color{0,0,0,185});
        const auto modal=layout.Modal();
        DrawRectangleRounded(modal,0.16F,10,Color{38,12,25,250});
        DrawRectangleRoundedLinesEx(modal,0.16F,10,2*scale,gold);
        const auto modalText=[&](const char* text,float offset,float size,Color color) {
            ui::DrawCentered(font,text,modal.x+modal.width/2,modal.y+offset*canvas.yScale,
                             ui::FittedSize(font,text,size*scale,modal.width-40*scale),color);
        };
        modalText("请按下键盘或鼠标按键",32,24,RAYWHITE);
        modalText(kBindingNames[selectedBinding_],84,20,gold);
        modalText("Esc：取消   F3～F6：保留   请先松开，再按下新键",142,16,Color{193,162,150,255});
    }
}

}  // namespace koumakyou
