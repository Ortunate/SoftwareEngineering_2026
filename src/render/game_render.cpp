// 本文件负责三维世界、主菜单、HUD、对话和符卡界面的绘制。

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
        ui::DrawText(font,value,l.Point(x+1.5F,y+1.5F),size*l.scale,Color{0,0,0,220});
        ui::DrawText(font,value,l.Point(x,y),size*l.scale,color);
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
#ifdef KOUMAKYOU_DEBUG_FEATURES
    if(debugRender_)text(TextFormat("XYZ %+.1f %+.1f %+.1f | %d bullets",player_.Position().x,player_.Position().y,player_.Position().z,
                   static_cast<int>(bullets_.Pool().ActiveCount())),25,1020,22,RAYWHITE);
#endif
}

void Game::DrawOverlay() const {
    const Font font = assets_.Common().uiFont;
    const menu::OverlayLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto& canvas=layout.canvas;
    if (messageTimer_ > 0.0F) {
        const float size=ui::FittedSize(font,message_.c_str(),29*canvas.scale,(canvas.width-100)*canvas.scale);
        const float width=ui::MeasureText(font,message_.c_str(),size).x;
        const auto center=canvas.Point(canvas.width/2,205);
        const float x=center.x-width/2;
        DrawRectangleRec({x-24*canvas.scale,center.y-12*canvas.scale,width+48*canvas.scale,size+24*canvas.scale},Color{5,7,18,185});
        ui::DrawText(font,message_.c_str(),{x+2*canvas.scale,center.y+2*canvas.scale},size,Color{80,20,40,230});
        ui::DrawText(font,message_.c_str(),{x,center.y},size,RAYWHITE);
    }

    DrawDialogue();
    DrawSpellCutIn();

    if (state_ == RunState::Paused) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Color{0, 0, 0, 155});
        const auto center=canvas.Point(380,310);
        DrawRectangleRounded(layout.PausePanel(),0.1F,12,Color{38,12,25,225});
        ui::DrawCentered(font,"暂停",center.x,canvas.Point(0,200).y,43*canvas.scale,RAYWHITE);
        constexpr std::array<const char*,4> labels{"解除暂停","返回开局选择","返回主菜单","从头再来"};
        for(int index=0;index<4;++index) {
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

void Game::DrawDialogue() const {
    if(!dialogueActive_)return;
    const GameplayLayout l(GetScreenWidth(),GetScreenHeight());
    const auto lines=CurrentDialogue(); float start=0;
    const int lineIndex=DialogueIndex(&start);
    const DialogueLine& line=lines[lineIndex];
    const auto expressions=DialogueExpressionsAt(lines,lineIndex);
    const float elapsed=dialogueTimer_-start;
    const auto portrait=[&](VisualId id,bool right,bool speaking) {
        const auto& visual=assets_.GetVisual(id);
        const Texture2D texture=visual.texture;
        if(!texture.id)return;
        const auto crop=visual.dialogueCrop;
        const Rectangle source{crop.x*texture.width,crop.y*texture.height,
                               crop.width*texture.width,crop.height*texture.height};
        if(source.width<=0 || source.height<=0)return;
        const float height=640, width=height*source.width/source.height;
        DrawTexturePro(texture,source,
            l.Rect(right ? 1388-width : 532,1042-height,width,height),{},0,
            speaking ? WHITE : Color{115,115,130,255});
    };
    // 平伸手臂会横跨另一方；先画未发言者，避免暗色立绘盖住当前说话人的脸。
    const auto hero=stageContent_->HeroPortraitVisual(expressions[0]);
    const auto boss=stageContent_->BossPortraitVisual(expressions[1]);
    if(line.bossSpeaking) {
        portrait(hero,false,false);
        if(line.showBoss)portrait(boss,true,true);
    } else {
        if(line.showBoss)portrait(boss,true,false);
        portrait(hero,false,true);
    }
    const Font font=assets_.Common().uiFont;
    if(elapsed>=line.textDelay) {
        const Rectangle panel=l.Rect(620,845,670,144);
        DrawRectangleRounded(panel,0.45F,16,Color{12,6,22,215});
        ui::DrawText(font,line.text,l.Point(650,872),31*l.scale,RAYWHITE);
        if(elapsed>=line.textDelay+line.line2Delay)
            ui::DrawText(font,line.secondLine,l.Point(650,918),31*l.scale,RAYWHITE);
    }
    if(line.introduce) {
        ui::DrawText(font,"宵暗的妖怪",l.Point(1035,280),33*l.scale,Color{225,220,255,255});
        ui::DrawText(font,"露米娅",l.Point(1100,325),53*l.scale,RAYWHITE);
    }
}

void Game::DrawSpellCutIn() const {
    if(spellCutInTimer_<=0 || !boss_ || !boss_->active || !boss_->SpellName())return;
    const GameplayLayout l(GetScreenWidth(),GetScreenHeight());
    const float elapsed=kSpellCutInDuration-spellCutInTimer_;
    const float opacity=std::clamp(std::min(elapsed/0.2F,spellCutInTimer_/0.4F),0.0F,1.0F);
    // 符卡差分由关卡决定；不套用对话裁剪，完整图仍沿用原有滑入和淡出时序。
    const Texture2D texture=assets_.GetVisual(stageContent_->SpellPortraitVisual()).texture;
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
    ui::DrawText(assets_.Common().uiFont,boss_->SpellName(),l.Point(850,670),32*l.scale,Fade(Color{190,185,255,255},opacity));
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
