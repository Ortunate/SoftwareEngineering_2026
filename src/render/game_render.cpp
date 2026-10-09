// 本文件负责三维世界、主菜单、HUD、对话和符卡界面的绘制。

#include "app/game.hpp"
#include "app/constants.hpp"
#include "app/math.hpp"
#include "config/settings.hpp"
#include "stages/stage1/scenery.hpp"
#include "render/gameplay_layout.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>

namespace redmist {
using namespace game_math;

#include "rlgl.h"

void Game::Draw() const {
    if (state_ == RunState::MainMenu) {
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

void Game::DrawWorld() const {
    const GameplayLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto drawTexture=[&](Texture2D texture,Rectangle source,Rectangle destination) {
        if(texture.id)DrawTexturePro(texture,source,destination,{},0,WHITE);
    };
    drawTexture(assets_.Common().referenceFrame,{1,1,1920,1080},layout.Rect(0,0,1920,1080));
    drawTexture(assets_.Common().referenceStage1,{0,0,560,1080},layout.Rect(0,0,560,1080));
    drawTexture(assets_.Common().referenceFrame,{1922,1,966,1080},layout.Rect(477,0,966,1080));
    const Rectangle viewport=layout.Viewport();
    // 默认窗口的 rlgl 尺寸是逻辑像素，macOS Retina 实际视口需用 GetRenderWidth/Height；
    // 离屏 FBO 则已按纹理像素建立，不能再次乘 DPI，否则截图会裁切到左下角。
    const bool windowTarget=rlGetActiveFramebuffer()==0;
    const int framebufferWidth=windowTarget ? GetRenderWidth() : rlGetFramebufferWidth();
    const int framebufferHeight=windowTarget ? GetRenderHeight() : rlGetFramebufferHeight();
    const float pixelX=static_cast<float>(framebufferWidth)/GetScreenWidth();
    const float pixelY=static_cast<float>(framebufferHeight)/GetScreenHeight();
    rlDrawRenderBatchActive();
    const int left=static_cast<int>(viewport.x*pixelX),bottom=static_cast<int>((GetScreenHeight()-viewport.y-viewport.height)*pixelY);
    rlViewport(left,bottom,static_cast<int>(viewport.width*pixelX),static_cast<int>(viewport.height*pixelY));
    rlEnableScissorTest();
    rlScissor(left,bottom,static_cast<int>(viewport.width*pixelX),static_cast<int>(viewport.height*pixelY));
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
                visual = stageContent_->PowerPickupVisual();
            else if (pickup.kind == PickupKind::Star)
                visual = stageContent_->StarPickupVisual();
            const float scale = pickup.kind == PickupKind::Power &&
                                        pickup.value >= 8 ? 1.25F : 0.9F;
            pickupSprites_.push_back({pickup.position, visual, scale});
        });
    }
    spriteRenderer_.Draw(camera_, bullets_, visible, boss_.get(),
                         pickupSprites_,
                         assets_.Stage().visuals, assets_.Stage().shaders,
                         actorEffects_);
#ifdef RED_MIST_DEBUG_FEATURES
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
}

void Game::DrawHud() const {
    const GameplayLayout l(GetScreenWidth(),GetScreenHeight());
    const Font font=assets_.Common().uiFont;
    const auto text=[&](const char* value,float x,float y,float size,Color color) {
        DrawTextEx(font,value,l.Point(x+1.5F,y+1.5F),size*l.scale,0,Color{0,0,0,220});
        DrawTextEx(font,value,l.Point(x,y),size*l.scale,0,color);
    };
    text("（普通难度）",1550,6,22,RAYWHITE);
    text("NORMAL",1490,30,52,Color{210,230,255,255});
    text("最高得分",1470,110,28,RAYWHITE);
    text(TextFormat("%09d",bestScore_),1645,104,38,RAYWHITE);
    text("得分",1470,158,28,RAYWHITE);
    text(TextFormat("%09lld",score_),1645,152,38,RAYWHITE);
    text("Player",1470,236,34,Color{255,180,190,255});
    if(challengeMode_)text(TextFormat("MISS %d",missCount_),1620,238,30,RAYWHITE);
    else for(int n=0;n<player_.RemainingLives();++n)text("★",1620+n*32,234,36,Color{255,185,220,255});
    text("Bomb",1470,294,34,Color{180,245,185,255});
    for(int n=0;n<player_.Bombs();++n)text("★",1620+n*32,290,36,Color{160,240,170,255});
    text("Power",1470,376,32,Color{255,160,180,255});
    DrawRectangleRec(l.Rect(1616,381,260,35),Color{5,3,8,255});
    DrawRectangleLinesEx(l.Rect(1616,381,260,35),l.scale,Color{215,210,215,255});
    DrawRectangleRec(l.Rect(1620,385,252.0F*player_.Power()/Player::kMaxPower,27),Color{165,75,100,255});
    text(player_.Power()==Player::kMaxPower ? "MAX" : TextFormat("%d",player_.Power()),1628,380,32,RAYWHITE);
    text("Graze",1470,433,32,Color{155,245,185,255});
    text(TextFormat("%d",graze_),1620,433,34,RAYWHITE);
    text("点数",1470,482,30,Color{180,145,250,255});
    text(TextFormat("%d",pointItems_),1620,482,34,RAYWHITE);
    const Rectangle field=l.Viewport();
    if(boss_ && boss_->active) {
        text(TextFormat("Enemy %d",!boss_->IsMidboss() && boss_->phase<=2 ? 1 : 0),550,46,31,GOLD);
        const float ratio=boss_->maxHealth>0 ? std::clamp(boss_->health/boss_->maxHealth,0.0F,1.0F):0;
        DrawRectangleRec(l.Rect(735,60,550,7),Color{60,20,40,255});
        DrawRectangleRec(l.Rect(735,60,550*ratio,7),RAYWHITE);
        if(boss_->TimeLimit()>0)text(TextFormat("%02d",std::max(0,static_cast<int>(std::ceil(boss_->TimeLimit()-boss_->phaseTimer)))),1320,43,35,RAYWHITE);
        if(boss_->SpellName())text(boss_->SpellName(),950,88,29,Color{165,155,255,255});
    }
    if(spellBonusTimer_>0)text(TextFormat("Spell Card Bonus  %lld",spellBonus_),735,125,30,GOLD);
    if(dialogueAfterBattle_ && dialogueActive_)text(TextFormat("BONUS  %lld",stageBonus_),775,85,35,GOLD);
    if(!dialogueActive_ && state_==RunState::Playing) {
        const Vector3 p=player_.Position(); bool danger=false;
        bullets_.Pool().ForEachActive([&](const Bullet& b) {
            if(b.owner==BulletOwner::Enemy && b.Dangerous() && b.DistanceSquaredTo(p)<1.15F*1.15F)danger=true;
        });
        const int x=static_cast<int>(field.x+field.width/2),y=static_cast<int>(field.y+field.height/2);
        const Color color=danger ? Color{255,70,90,255} : Color{215,240,255,235};
        DrawCircleLines(x,y,7*l.scale,color); DrawCircle(x,y,1.8F*l.scale,color);
        DrawLine(x-16*l.scale,y,x-9*l.scale,y,color); DrawLine(x+9*l.scale,y,x+16*l.scale,y,color);
        DrawLine(x,y-16*l.scale,x,y-9*l.scale,color); DrawLine(x,y+9*l.scale,x,y+16*l.scale,color);
    }
    text(TextFormat("%02d.00fps",GetFPS()),1660,1045,23,RAYWHITE);
#ifdef RED_MIST_DEBUG_FEATURES
    if(debugRender_)text(TextFormat("XYZ %+.1f %+.1f %+.1f | %d bullets",player_.Position().x,player_.Position().y,player_.Position().z,
                   static_cast<int>(bullets_.Pool().ActiveCount())),25,1020,22,RAYWHITE);
#endif
}

void Game::DrawOverlay() const {
    if (messageTimer_ > 0.0F) {
        const int fontSize = 29;
        const int width = MeasureText(message_.c_str(), fontSize);
        const int x = (GetScreenWidth() - width) / 2;
        const int y = GetScreenHeight() / 2 - 105;
        DrawRectangle(x - 24, y - 12, width + 48, fontSize + 24,
                      Color{5, 7, 18, 185});
        DrawText(message_.c_str(), x + 2, y + 2, fontSize,
                 Color{80, 20, 40, 230});
        DrawText(message_.c_str(), x, y, fontSize, RAYWHITE);
    }

    DrawDialogue();
    DrawSpellCutIn();

    if (state_ == RunState::Paused) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Color{0, 0, 0, 155});
        const float centerX = GetScreenWidth() * 0.5F;
        const float centerY = GetScreenHeight() * 0.5F;
        const Font font = assets_.Common().uiFont;
        const auto centered = [&](const char* text, float y, float size, Color color) {
            const float width = MeasureTextEx(font, text, size, 1).x;
            DrawTextEx(font, text, {centerX - width / 2, y}, size, 1, color);
        };
        DrawRectangleRounded({centerX - 245, centerY - 130, 490, 310}, 0.1F, 12, Color{38, 12, 25, 225});
        centered("暂停", centerY - 110, 43, RAYWHITE);
        constexpr std::array<const char*, 4> labels{"解除暂停", "返回开局选择", "返回主菜单", "从头再来"};
        for (int index = 0; index < 4; ++index) {
            const bool selected = index == selectedPauseItem_;
            if (selected) DrawRectangle(centerX - 210, centerY - 40 + index * 48, 420, 42, Color{114, 42, 53, 190});
            centered(labels[index], centerY - 36 + index * 48, 25,
                     selected ? GOLD : Color{190, 153, 164, 255});
        }
    } else if (state_ == RunState::Won || state_ == RunState::GameOver) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Color{0, 0, 0, 175});
        const char* title = state_ == RunState::Won ? stageContent_->ClearTitle() : "GAME OVER";
        DrawText(title, (GetScreenWidth() - MeasureText(title, 56)) / 2,
                 GetScreenHeight() / 2 - 75, 56,
                 state_ == RunState::Won ? GOLD : Color{255, 90, 110, 255});
        const int center = GetScreenWidth() / 2;
        const int y = GetScreenHeight() / 2;
        const char* detail = TextFormat("SCORE %09lld   GRAZE %d   MISS %d   BOMB %d", score_, graze_, missCount_, bombCount_);
        DrawText(detail, center - MeasureText(detail, 22) / 2, y, 22, RAYWHITE);
        const char* mode = spellPractice_ ? "SPELL PRACTICE" : practiceMode_ ? "STAGE PRACTICE" : challengeMode_ ? "CHALLENGE" : "NORMAL";
        DrawText(mode, center - MeasureText(mode, 18) / 2, y + 40, 18, GOLD);
        const char* retry = "R Restart     Esc / M Main Menu";
        DrawText(retry, center - MeasureText(retry, 22) / 2, y + 85, 22, RAYWHITE);
    }

    if (screenFlashTimer_ > 0.0F) {
        const unsigned char alpha = static_cast<unsigned char>(
            std::clamp(screenFlashTimer_ / 0.35F, 0.0F, 1.0F) * 145.0F);
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Color{220, 240, 255, alpha});
    }
}

void Game::DrawDialogue() const {
    if(!dialogueActive_)return;
    const GameplayLayout l(GetScreenWidth(),GetScreenHeight());
    const auto lines=CurrentDialogue(); float start=0;
    const DialogueLine& line=lines[DialogueIndex(&start)];
    const float elapsed=dialogueTimer_-start;
    const auto portrait=[&](VisualId id,bool right,bool speaking) {
        const Texture2D texture=assets_.GetVisual(id).texture;
        if(!texture.id)return;
        const float height=640, width=height*texture.width/texture.height;
        DrawTexturePro(texture,{0,0,static_cast<float>(texture.width),static_cast<float>(texture.height)},
            l.Rect(right ? 1388-width : 532,1042-height,width,height),{},0,
            speaking ? WHITE : Color{115,115,130,255});
    };
    portrait(stageContent_->HeroPortraitVisual(),false,!line.bossSpeaking);
    if(line.showBoss)portrait(stageContent_->BossPortraitVisual(),true,line.bossSpeaking);
    const Font font=assets_.Common().uiFont;
    if(elapsed>=line.textDelay) {
        const Rectangle panel=l.Rect(620,845,670,144);
        DrawRectangleRounded(panel,0.45F,16,Color{12,6,22,215});
        DrawTextEx(font,line.text,l.Point(650,872),31*l.scale,0,RAYWHITE);
        if(elapsed>=line.textDelay+line.line2Delay)
            DrawTextEx(font,line.secondLine,l.Point(650,918),31*l.scale,0,RAYWHITE);
    }
    if(line.introduce) {
        DrawTextEx(font,"宵暗的妖怪",l.Point(1035,480),33*l.scale,0,Color{225,220,255,255});
        DrawTextEx(font,"露米娅",l.Point(1100,525),53*l.scale,0,RAYWHITE);
    }
}

void Game::DrawSpellCutIn() const {
    if(spellCutInTimer_<=0 || !boss_ || !boss_->active || !boss_->SpellName())return;
    const GameplayLayout l(GetScreenWidth(),GetScreenHeight());
    const float elapsed=kSpellCutInDuration-spellCutInTimer_;
    const float opacity=std::clamp(std::min(elapsed/0.2F,spellCutInTimer_/0.4F),0.0F,1.0F);
    const Texture2D texture=assets_.GetVisual(stageContent_->BossPortraitVisual()).texture;
    if(texture.id) {
        const float height=760, width=height*texture.width/texture.height;
        const float offset=(1-std::clamp(elapsed/0.4F,0.0F,1.0F))*400;
        BeginScissorMode(static_cast<int>(l.Viewport().x),static_cast<int>(l.Viewport().y),
                         static_cast<int>(l.Viewport().width),static_cast<int>(l.Viewport().height));
        DrawTexturePro(texture,{0,0,static_cast<float>(texture.width),static_cast<float>(texture.height)},
            l.Rect(1388-width+offset,1042-height,width,height),{},0,Fade(WHITE,opacity));
        EndScissorMode();
    }
    DrawRectangleRec(l.Rect(820,660,550,55),Color{10,5,30,static_cast<unsigned char>(opacity*180)});
    DrawTextEx(assets_.Common().uiFont,boss_->SpellName(),l.Point(850,670),32*l.scale,0,Fade(Color{190,185,255,255},opacity));
}
void Game::DrawKeyBindings() const {
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const float centerX = static_cast<float>(screenWidth) * 0.5F;
    DrawRectangle(0, 0, screenWidth, screenHeight, Color{2, 4, 14, 205});

    const char* title = "KEY BINDINGS";
    DrawText(title, (screenWidth - MeasureText(title, 42)) / 2,
             45, 42, RAYWHITE);
    const char* instruction =
        "Select an action, then press a key or mouse button";
    DrawText(instruction, (screenWidth - MeasureText(instruction, 18)) / 2,
             101, 18, Color{155, 190, 215, 255});

    for (int index = 0; index < static_cast<int>(kBindingNames.size()); ++index) {
        const Rectangle row{centerX - 270.0F,
                            kBindingsTop + static_cast<float>(index) * kBindingsSpacing,
                            540.0F, kBindingsHeight};
        const bool selected = selectedBinding_ == index;
        DrawRectangleRounded(row, 0.16F, 8,
                             selected ? Color{90, 24, 50, 235}
                                      : Color{8, 15, 30, 220});
        DrawRectangleRoundedLinesEx(row, 0.16F, 8, selected ? 2.0F : 1.0F,
                                    selected ? Color{255, 110, 145, 255}
                                             : Color{58, 95, 125, 210});
        DrawText(kBindingNames[index], static_cast<int>(row.x + 15.0F),
                 static_cast<int>(row.y + 6.0F), 17,
                 selected ? RAYWHITE : Color{175, 195, 210, 255});
        const std::string keyName = KeyLabel(BindingAt(index));
        DrawText(keyName.c_str(),
                 static_cast<int>(row.x + row.width - 16.0F) -
                     MeasureText(keyName.c_str(), 17),
                 static_cast<int>(row.y + 6.0F), 17,
                 selected ? Color{120, 215, 255, 255}
                          : Color{110, 160, 195, 255});
    }

    constexpr int resetIndex = static_cast<int>(kBindingNames.size());
    constexpr int backIndex = resetIndex + 1;
    const Rectangle resetButton{centerX - 270.0F, 500.0F, 260.0F, 42.0F};
    const Rectangle backButton{centerX + 10.0F, 500.0F, 260.0F, 42.0F};
    const Rectangle buttons[2] = {resetButton, backButton};
    const char* labels[2] = {"RESET DEFAULTS", "BACK"};
    for (int index = 0; index < 2; ++index) {
        const int selection = index == 0 ? resetIndex : backIndex;
        const bool selected = selectedBinding_ == selection;
        DrawRectangleRounded(buttons[index], 0.18F, 8,
                             selected ? Color{90, 24, 50, 235}
                                      : Color{8, 15, 30, 220});
        DrawRectangleRoundedLinesEx(buttons[index], 0.18F, 8,
                                    selected ? 2.0F : 1.0F,
                                    selected ? Color{255, 110, 145, 255}
                                             : Color{58, 95, 125, 210});
        DrawText(labels[index],
                 static_cast<int>(buttons[index].x + buttons[index].width * 0.5F) -
                     MeasureText(labels[index], 18) / 2,
                 static_cast<int>(buttons[index].y + 12.0F), 18, RAYWHITE);
    }

    const char* footer = "Arrow Keys / W / S: Select     Enter: Rebind     Esc: Options";
    DrawText(footer, (screenWidth - MeasureText(footer, 17)) / 2,
             screenHeight - 42, 17, Color{145, 175, 198, 235});

    if (waitingForBinding_) {
        DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 185});
        const Rectangle modal{centerX - 250.0F,
                              static_cast<float>(screenHeight) * 0.5F - 82.0F,
                              500.0F, 164.0F};
        DrawRectangleRounded(modal, 0.16F, 10, Color{12, 18, 36, 250});
        DrawRectangleRoundedLinesEx(modal, 0.16F, 10, 2.0F,
                                    Color{255, 110, 145, 255});
        const char* prompt = "PRESS KEY / MOUSE BUTTON";
        DrawText(prompt, (screenWidth - MeasureText(prompt, 24)) / 2,
                 static_cast<int>(modal.y + 32.0F), 24, RAYWHITE);
        DrawText(kBindingNames[selectedBinding_],
                 (screenWidth - MeasureText(kBindingNames[selectedBinding_], 20)) / 2,
                 static_cast<int>(modal.y + 78.0F), 20,
                 Color{120, 215, 255, 255});
        const char* cancel = "Esc: cancel   F3-F6: reserved   Release, then press";
        DrawText(cancel, (screenWidth - MeasureText(cancel, 16)) / 2,
                 static_cast<int>(modal.y + 120.0F), 16,
                 Color{145, 165, 185, 255});
    }
}

}  // namespace redmist
