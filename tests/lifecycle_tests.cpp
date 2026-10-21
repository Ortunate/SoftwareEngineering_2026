// 本文件负责在隐藏图形窗口中验证自机射击可见性、Game 战斗演出、结算及资源生命周期。

#include "app/game.hpp"
#include "rlgl.h"
#include "stages/stage1/enemies.hpp"
#include "ui/typography.hpp"
#include "render/gameplay_layout.hpp"
#include "render/boss_hud.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <filesystem>
#include <chrono>
#include <array>
#include <utility>
#include <vector>

namespace koumakyou {

/// @brief 仅此测试定义的友元探针；调用正式游戏路径验证效果不重复奖励或保留战斗对象。
struct GameLifecycleProbe {
    /// @brief 两种字重均须加载真实中英文字形和等宽数字，避免浮窗加粗静默回退或分数宽度跳动。
    static void TestUiFont(const Game& game) {
        const auto& common = game.assets_.Common();
        assert(common.uiFont.texture.id != common.hudFont.texture.id);
        for (const Font font : {common.uiFont, common.hudFont}) {
            assert(font.texture.id != GetFontDefault().texture.id);
            for (int codepoint : std::array<int, 8>{'0', '9', 'Z', 0x4e2d, 0x300c, 0x2605, 0x5e7b, 0x5377})
                assert(font.glyphs[GetGlyphIndex(font, codepoint)].value == codepoint);
            const float zeroWidth = ui::MeasureText(font, "000000000", 38).x;
            assert(std::abs(zeroWidth - ui::MeasureText(font, "111111111", 38).x) < 0.001F);
            assert(std::abs(zeroWidth - ui::MeasureText(font, "987654321", 38).x) < 0.001F);
        }
    }
    static std::size_t EnemyCount(const Game& game) {
        std::size_t count = 0;
        game.enemies_.ForEachActive([&](const Enemy&) { ++count; });
        return count;
    }
    static std::size_t PickupCount(const Game& game) {
        std::size_t count = 0;
        game.drops_.ForEachActive([&](const Pickup&) { ++count; });
        return count;
    }
    /// @brief 可选导出同一测试状态的图片；prefix 为空时只绘制并检查逻辑。
    static void Capture(const Game& game, const std::string& prefix, const char* name) {
        const RenderTexture2D target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        assert(IsRenderTextureValid(target));
        BeginDrawing();
        BeginTextureMode(target);
        ClearBackground(BLACK);
        game.Draw();
        EndTextureMode();
        EndDrawing();
        if (!prefix.empty()) {
            Image image = LoadImageFromTexture(target.texture);
            assert(IsImageValid(image));
            ImageFlipVertical(&image);
            const std::string path = prefix + "-" + name + ".png";
            assert(ExportImage(image, path.c_str()));
            UnloadImage(image);
        }
        UnloadRenderTexture(target);
    }

    /// @brief 独立绘制世界，检查四边没有留黑，左右边缘随镜头变化，防止旧侧栏或裁切恢复。
    static void TestFullWindow(Game& game, const std::string& prefix) {
        game.StartPreview();
        const int originalWidth=GetScreenWidth(), originalHeight=GetScreenHeight();
        for(const Vector2 size : {Vector2{1100,760},{1280,720},{960,720},{1680,720},{390,844}}) {
            SetWindowSize(static_cast<int>(size.x),static_cast<int>(size.y));
            const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
            const auto render=[&](float yaw) {
                game.SetPreviewView(yaw,0);
                BeginDrawing(); BeginTextureMode(target);
                ClearBackground(Color{255,0,255,255});
                game.DrawWorld();
                EndTextureMode(); EndDrawing();
                Image image=LoadImageFromTexture(target.texture);
                assert(IsImageValid(image));
                ImageFlipVertical(&image);
                return image;
            };
            Image first=render(0), turned=render(35);
            assert(game.camera_.fovy==72);
            for(const Vector2 point : {Vector2{1,1},{size.x-2,1},{1,size.y-2},{size.x-2,size.y-2}}) {
                const Color pixel=GetImageColor(first,static_cast<int>(point.x),static_cast<int>(point.y));
                assert(!(pixel.r==255 && pixel.g==0 && pixel.b==255));
            }
            for(const int x : {2,static_cast<int>(size.x)-3}) {
                int changed=0, samples=0;
                for(int y=2;y<static_cast<int>(size.y)-2;y+=7) {
                    const Color a=GetImageColor(first,x,y), b=GetImageColor(turned,x,y);
                    if(std::abs(int(a.r)-b.r)+std::abs(int(a.g)-b.g)+std::abs(int(a.b)-b.b)>6)++changed;
                    ++samples;
                }
                assert(changed>samples/10);
            }
            UnloadImage(first); UnloadImage(turned); UnloadRenderTexture(target);
            game.SetPreviewView(0,0);
            const std::string name="full-window-"+std::to_string(static_cast<int>(size.x))+"x"+std::to_string(static_cast<int>(size.y));
            Capture(game,prefix,name.c_str());
        }
        SetWindowSize(originalWidth,originalHeight);
        // 实际提交全窗口世界与 HUD；只报告耗时，不用依赖机器速度的阈值卡测试。
        for(int frame=0;frame<10;++frame) { BeginDrawing(); game.Draw(); EndDrawing(); }
        const double start=GetTime();
        for(int frame=0;frame<60;++frame) { BeginDrawing(); game.Draw(); EndDrawing(); }
        std::cout<<"Full-window world and HUD: "<<(GetTime()-start)*1000/60<<" ms/frame\n";
    }

    /// @brief 按正式更新顺序以玩家弹击破或等超时，验证 30～240 FPS 各脚本帧间隙均能保留第二组非符。
    static void TestBossTransitions(Game& game, const std::string& prefix) {
        game.StartPreview();
        game.stage_=StageDirector{};
        game.player_.SetInvulnerability(1000);
        for(int fps : {30,60,120,240}) for(bool timeout : {false,true}) {
            // 击破可能发生在一个 60 Hz 脚本帧中的任意渲染帧；只测整 1/60 秒会漏掉准备期空隙。
            for(int alignment=0;alignment<std::max(1,fps/60);++alignment) {
                const float dt=1.0F/fps;
                game.bullets_.Reset(); game.drops_.Clear();
                game.BeginStageBoss(true,1);
                const auto ready=[&](int phase) {
                    int frames=0;
                    while(!game.boss_->CanTakeDamage() && frames++<5*fps) {
                        game.Update(dt);
                        assert(game.boss_->phase==phase);
                    }
                    assert(game.boss_->phase==phase && game.boss_->CanTakeDamage());
                };
                const auto hit=[&] {
                    auto* shot=game.bullets_.Spawn(BulletOwner::Player,game.boss_->position,{},0.1F,WHITE,kInvalidVisualId);
                    assert(shot); shot->damage=100000;
                    game.Update(dt); // 包含 UpdateBoss、弹幕移动及 ResolveCollisions，不能直接调用 AdvancePhase 代替。
                };
                ready(1); hit();
                assert(game.boss_->phase==2);
                ready(2);
                for(int frame=0;frame<alignment;++frame)game.Update(dt);
                if(timeout) {
                    int frames=0;
                    while(game.boss_->phase==2 && frames++<30*fps)game.Update(dt);
                } else hit();
                assert(game.boss_->phase==3 && game.boss_->SpellName()==nullptr);
                assert(game.boss_->health==7500/stage1::kReferenceHealthUnit);
                assert(game.boss_->maxHealth==7500/stage1::kReferenceHealthUnit);
                const auto bar=game.boss_->HealthBar();
                assert(bar.remainingBars==0 && std::abs(bar.segments[1].remaining-6700.0F/7500)<0.00001F);
                for(int frame=0;frame<std::max(1,fps/60);++frame) {
                    game.Update(dt);
                    assert(game.boss_->phase==3 && game.boss_->SpellName()==nullptr);
                }
                ready(3);
                for(int frame=0;frame<static_cast<int>(1.1F*fps);++frame) {
                    game.Update(dt);
                    assert(game.boss_->phase==3);
                }
                int beams=0;
                game.bullets_.Pool().ForEachActive([&](const Bullet& bullet) { if(bullet.beamLength>0)++beams; });
                assert(beams==6 && game.boss_->TimeLimit()==30);
                if(!timeout && fps==240 && alignment==3)Capture(game,prefix,"second-nonspell-restored");
                hit();
                assert(game.boss_->phase==4 && game.boss_->SpellName()!=nullptr);
                assert(std::abs(game.boss_->HealthBar().segments[0].remaining-800.0F/7500)<0.00001F);
            }
        }
        std::cout<<"Boss damage/timeout transitions and second nonspell beams passed at 30/60/120/240 FPS\n";
    }

    /// @brief 通过正式 HUD 读取分段颜色与空白像素，覆盖四个攻击阶段、道中及多种窗口比例。
    static void TestBossHud(Game& game, const std::string& prefix) {
        game.StartPreview();
        const int width=GetScreenWidth(),height=GetScreenHeight();
        struct Scene { int phase; float sourceHealth; const char* name; };
        const std::array<Scene,10> scenes{{
            {1,7000,"first-full"},{1,3950,"first-normal-half"},{1,900,"first-boundary"},
            {2,900,"first-spell-full"},{2,450,"first-spell-half"},{2,0,"first-spell-empty"},
            {3,7500,"second-full"},{3,4150,"second-normal-half"},
            {4,400,"second-spell-half"},{0,6000,"midboss-full"}}};
        for(const Vector2 size : {Vector2{1280,720},{900,900},{390,844},{1600,400},{320,240}}) {
            SetWindowSize(static_cast<int>(size.x),static_cast<int>(size.y));
            const GameplayLayout layout(GetScreenWidth(),GetScreenHeight());
            const ui::BossHudLayout row(layout.BossPanel(),layout.scale);
            const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
            for(const auto& scene : scenes) {
                game.BeginStageBoss(scene.phase!=0,1);
                while(game.boss_->phase<scene.phase)assert(game.boss_->AdvancePhase());
                game.UpdateBoss(1.0F/60);
                game.bullets_.Reset(); game.drops_.Clear(); // 隔离 Sub19 的散落，截图不能累积上一个展示场景的对象。
                game.boss_->phaseTimer=5;
                game.boss_->health=scene.sourceHealth/stage1::kReferenceHealthUnit;
                const auto health=game.boss_->HealthBar();
                BeginDrawing(); BeginTextureMode(target); ClearBackground(Color{17,31,47,255});
                game.DrawHud();
                EndTextureMode(); EndDrawing();
                Image pixels=LoadImageFromTexture(target.texture); ImageFlipVertical(&pixels);
                const auto near=[](Color a,Color b) {
                    return std::abs(int(a.r)-b.r)<=2 && std::abs(int(a.g)-b.g)<=2 && std::abs(int(a.b)-b.b)<=2;
                };
                float offset=0;
                for(std::size_t index=0;index<health.count;++index) {
                    const auto segment=health.segments[index];
                    for(float fraction : {0.25F,0.75F}) {
                        const float sample=segment.capacity*fraction;
                        const int x=static_cast<int>(row.bar.x+(offset+sample)*row.bar.width);
                        const int y=static_cast<int>(row.bar.y+row.bar.height/2);
                        const auto color=GetImageColor(pixels,x,y);
                        if(sample<segment.remaining) {
                            const Color expected=segment.kind==BossHealthKind::Spell ? Color{239,138,152,255} : WHITE;
                            assert(near(color,expected));
                        } else {
                            assert(!near(color,WHITE) && !near(color,Color{239,138,152,255}));
                        }
                    }
                    offset+=segment.capacity;
                }
                UnloadImage(pixels);
                const auto name="boss-hud-"+std::to_string(GetScreenWidth())+"-"+scene.name;
                Capture(game,prefix,name.c_str());
            }
            UnloadRenderTexture(target);
        }
        SetWindowSize(width,height);
        std::cout<<"Boss HUD segmented colors and same-row layout passed at five window sizes\n";
    }

    /// @brief 用纯色纹理读取边缘像素验证透明混合、暂停冻结，并用正式立绘导出切换过渡。
    static void TestDialogueTransitions(Game& game, const std::string& prefix) {
        game.StartPreview();
        game.BeginStageDialogue(1);
        const auto lines=game.CurrentDialogue();
        const auto at=[&](int index) {
            float seconds=1.1F;
            for(int line=0;line<index;++line)
                seconds+=lines[line].duration>0 ? lines[line].duration : game.stageContent_->DialogueLineDuration();
            return seconds;
        };
        // 用不透明纯色替代纹理，直接验证最终像素混合，防止把变暗 tint 误当作 30% Alpha。
        auto& visuals=const_cast<StageAssets&>(game.assets_.Stage()).visuals;
        std::vector<std::pair<VisualId,VisualResource>> originals;
        Image redImage=GenImageColor(32,32,RED), greenImage=GenImageColor(32,32,GREEN);
        const Texture2D red=LoadTextureFromImage(redImage), green=LoadTextureFromImage(greenImage);
        UnloadImage(redImage); UnloadImage(greenImage);
        for(int index=0;index<10;++index) {
            const auto id=index<6 ? game.stageContent_->HeroPortraitVisual(index)
                : game.stageContent_->BossPortraitVisual(index-6);
            originals.emplace_back(id,visuals[id]);
            visuals[id].texture=index<6 ? red : green;
            visuals[id].dialogueCrop={0,0,1,1};
        }
        const RenderTexture2D target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
        constexpr Color background{17,31,47,255};
        const auto pixels=[&](float leftAlpha,float rightAlpha) {
            BeginDrawing(); BeginTextureMode(target); ClearBackground(background);
            game.DrawDialogue();
            EndTextureMode(); EndDrawing();
            Image image=LoadImageFromTexture(target.texture); ImageFlipVertical(&image);
            const auto check=[&](int x,Color ink,float alpha) {
                // raylib 将浮点 Alpha 量化为 8 位，允许最多两级的 GPU 舍入误差。
                const float a=static_cast<unsigned char>(alpha*255)/255.0F;
                const Color pixel=GetImageColor(image,x,image.height*3/5);
                assert(std::abs(int(pixel.r)-int(std::round(ink.r*a+background.r*(1-a))))<=2);
                assert(std::abs(int(pixel.g)-int(std::round(ink.g*a+background.g*(1-a))))<=2);
                assert(std::abs(int(pixel.b)-int(std::round(ink.b*a+background.b*(1-a))))<=2);
            };
            check(0,RED,leftAlpha); check(image.width-1,GREEN,rightAlpha);
            UnloadImage(image);
        };
        game.dialogueTimer_=at(8); game.UpdateUiMotion(0.3F);
        pixels(1,0.3F);
        game.dialogueTimer_=at(7); game.UpdateUiMotion(0);
        pixels(1,0.3F);
        game.UpdateUiMotion(0.13F);
        pixels(0.65F,0.65F);
        const auto paused=game.dialogueMotion_.Sample(false);
        game.TogglePause(); game.Update(0.5F);
        assert(game.dialogueMotion_.Sample(false).opacity==paused.opacity);
        assert(game.dialogueMotion_.Sample(false).focus==paused.focus);
        game.TogglePause(); game.UpdateUiMotion(0.13F);
        pixels(0.3F,1);
        for(const auto& [id,original]:originals)visuals[id]=original;
        UnloadTexture(red); UnloadTexture(green); UnloadRenderTexture(target);

        // 正式差分逐帧审阅两个方向的过渡，保持其他世界状态不动；Draw 不推进动画。
        game.dialogueTimer_=at(8); game.UpdateUiMotion(0.3F);
        Capture(game,prefix,"dialogue-focus-left");
        for(int direction=0;direction<2;++direction) {
            game.dialogueTimer_=at(direction==0 ? 7 : 8);
            for(int frame=0;frame<8;++frame) {
                game.UpdateUiMotion(frame==0 ? 0 : 0.04F);
                const std::string name="dialogue-turn-"+std::to_string(direction)+"-"+std::to_string(frame);
                Capture(game,prefix,name.c_str());
            }
        }
        game.BeginStageDialogue(2,true);
        assert(game.dialogueMotion_.Sample(false).opacity==1 && game.dialogueMotion_.Sample(true).opacity==0);
        std::cout<<"Dialogue transition opacity, pause and edge tests passed\n";
    }

    /// @brief 实际加载十个差分，渲染逐句切换及菜单/符卡，验证离开关卡不释放通用立绘。
    static void TestPortraits(Game& game, const std::string& prefix) {
        game.StartPreview();
        const auto menuTexture=game.assets_.Common().reimuPortrait;
        assert(IsTextureValid(menuTexture) && menuTexture.width==976 && menuTexture.height==1612);
        for(int index=0;index<6;++index)
            assert(IsTextureValid(game.assets_.GetVisual(game.stageContent_->HeroPortraitVisual(index)).texture));
        for(int index=0;index<4;++index) {
            const auto& visual=game.assets_.GetVisual(game.stageContent_->BossPortraitVisual(index));
            assert(IsTextureValid(visual.texture) && visual.texture.width==887 && visual.texture.height==1774);
            assert(visual.dialogueCrop.height<1);
        }
        const int width=GetScreenWidth(),height=GetScreenHeight();
        // 用正式 PNG（含近透明修复残留）独立绘制每个差分，防止纯色画布贴边测试漏掉内部留白。
        for (const Vector2 size : {Vector2{1100,760},{1280,720},{390,844}}) {
            const GameplayLayout layout(size.x,size.y);
            const auto target=LoadRenderTexture(static_cast<int>(size.x),static_cast<int>(size.y));
            constexpr Color background{255,0,255,255};
            for (int index=0;index<10;++index) {
                const bool right=index>=6;
                const auto id=right ? game.stageContent_->BossPortraitVisual(index-6)
                    : game.stageContent_->HeroPortraitVisual(index);
                const auto& visual=game.assets_.GetVisual(id);
                const auto crop=visual.dialogueCrop;
                const Rectangle source{crop.x*visual.texture.width,crop.y*visual.texture.height,
                    crop.width*visual.texture.width,crop.height*visual.texture.height};
                BeginDrawing(); BeginTextureMode(target); ClearBackground(background);
                DrawTexturePro(visual.texture,source,layout.DialoguePortrait(source.width/source.height,right),{},0,WHITE);
                EndTextureMode(); EndDrawing();
                Image image=LoadImageFromTexture(target.texture); ImageFlipVertical(&image);
                int x0=image.width,x1=0,y1=0;
                for (int y=0;y<image.height;++y) for(int x=0;x<image.width;++x) {
                    const auto pixel=GetImageColor(image,x,y);
                    if (std::abs(int(pixel.r)-255)+pixel.g+std::abs(int(pixel.b)-255)<=6) continue;
                    x0=std::min(x0,x); x1=std::max(x1,x); y1=std::max(y1,y);
                }
                const int gap=right ? image.width-1-x1 : x0;
                std::cout<<"Portrait edge "<<index<<" at "<<image.width<<"x"<<image.height
                         <<": side="<<gap<<", bottom="<<image.height-1-y1<<" px\n";
                assert(gap<=2 && image.height-1-y1<=2);
                if (!prefix.empty()) {
                    const auto path=prefix+"-portrait-edge-"+std::to_string(index)+"-"+std::to_string(image.width)+".png";
                    assert(ExportImage(image,path.c_str()));
                }
                UnloadImage(image);
            }
            UnloadRenderTexture(target);
        }
        game.BeginStageDialogue(1);
        const auto lines=game.CurrentDialogue();
        float start=0;
        for(std::size_t index=0;index<lines.size();++index) {
            game.dialogueTimer_=start+std::max(1.1F,lines[index].textDelay+lines[index].line2Delay+0.1F);
            assert(game.DialogueIndex()==static_cast<int>(index));
            game.UpdateUiMotion(0.3F);
            const auto name="portrait-dialogue-"+std::to_string(index);
            Capture(game,prefix,name.c_str());
            start+=lines[index].duration;
        }
        // 重建计时器时应恢复双方表情，而不是只设置当前说话人的图。
        game.dialogueTimer_=31;
        game.UpdateUiMotion(0.3F);
        for(const auto size:std::array<Vector2,2>{{{1280,720},{390,844}}}) {
            SetWindowSize(size.x,size.y);
            const auto name="portrait-dialogue-"+std::to_string(static_cast<int>(size.x));
            Capture(game,prefix,name.c_str());
        }
        SetWindowSize(width,height);
        game.dialogueActive_=false;
        game.state_=RunState::StartSetup;
        game.UpdateUiMotion(1);
        Capture(game,prefix,"portrait-menu");
        game.state_=RunState::Playing;
        game.BeginStageBoss(true,2);
        game.boss_->AdvancePhase();
        game.spellCutInTimer_=1.5F;
        Capture(game,prefix,"portrait-spell");
        game.EnterMainMenu();
        assert(game.assets_.Stage().visuals.empty());
        assert(IsTextureValid(game.assets_.Common().reimuPortrait));
        assert(game.assets_.Common().reimuPortrait.id==menuTexture.id);
    }

    /// @brief 道中与关底 Boss 入场均保留现存战斗对象，并继续推进其运动。
    static void TestBossEntryPreservesField(Game& game) {
        for (const bool final : {false, true}) {
            game.StartPreview();
            game.stage_ = StageDirector{};
            game.enemies_.Add(std::make_unique<stage1::FairyEnemy>(
                stage1::FairyParams{{4, 0, 5}}));
            Bullet* enemyShot = game.bullets_.Spawn(
                BulletOwner::Enemy, {-4, 0, 0}, {1, 0, 0}, 0.1F, WHITE, kInvalidVisualId);
            Bullet* playerShot = game.bullets_.Spawn(
                BulletOwner::Player, {-4, 2, 0}, {1, 0, 0}, 0.1F, WHITE, kInvalidVisualId);
            assert(enemyShot && playerShot);
            game.drops_.SpawnFixedPower({4, 0, 0}, 1);
            const auto pickups = PickupCount(game);
            assert(pickups > 0);

            game.BeginStageBoss(final, 1);
            assert(game.boss_ && game.boss_->active);
            assert(EnemyCount(game) == 1);
            assert(enemyShot->active && playerShot->active);
            assert(game.bullets_.Pool().ActiveCount() == 2);
            assert(PickupCount(game) == pickups);
            assert(game.actorEffects_.Pool().ActiveCount() == 0);

            game.Update(0.1F);
            assert(EnemyCount(game) == 1);
            game.enemies_.ForEachActive([](const Enemy& enemy) {
                assert(enemy.age == 0.1F && enemy.position.z < 5.0F);
            });
            assert(enemyShot->active && enemyShot->position.x > -4.0F);
            assert(playerShot->active && playerShot->position.x > -4.0F);
            assert(PickupCount(game) == pickups);
        }
    }

    /// @brief 不跳过事件，按 60 Hz 从开场推进到结算，并渲染对话、每个 Boss 阶段和光束。
    static void TestWholeStage(Game& game, const std::string& prefix) {
        game.StartPreview();
        game.previewMode_ = false; // 本用例在临时目录验证标准模式成绩保存。
        bool midbossSeen = false, dialogueSeen = false;
        std::array<bool, 4> phases{};
        int frame = 0;
        for (; frame < 400 * 60 && game.state_ != RunState::Won; ++frame) {
            game.Update(1.0F / 60.0F);
            if (frame == 12 * 60) {
                const int width=GetScreenWidth(),height=GetScreenHeight();
                for(const auto size:std::array<Vector2,3>{{{1280,720},{960,720},{1680,720}}}) {
                    SetWindowSize(static_cast<int>(size.x),static_cast<int>(size.y));
                    const std::string name="opening-"+std::to_string(static_cast<int>(size.x));
                    Capture(game,prefix,name.c_str());
                }
                SetWindowSize(width,height);
                Capture(game, prefix, "stage-opening");
                // 真实默认 framebuffer 与离屏纹理分别验证；Retina 恢复逻辑视口会使右半屏空白。
                BeginDrawing(); ClearBackground(BLACK); game.Draw(); EndDrawing();
                BeginDrawing(); ClearBackground(BLACK); game.Draw(); rlDrawRenderBatchActive();
                Image windowImage=LoadImageFromScreen();
                const Color edge=GetImageColor(windowImage,windowImage.width*9/10,windowImage.height/2);
                assert(edge.r+edge.g+edge.b>0);
                if(!prefix.empty())assert(ExportImage(windowImage,(prefix+"-window-retina.png").c_str()));
                UnloadImage(windowImage); EndDrawing();
            }
            if (game.dialogueActive_ && !dialogueSeen && game.dialogueTimer_ > 1) {
                dialogueSeen = true;
                Capture(game, prefix, "dialogue");
            }
            if (game.boss_ && game.boss_->active) {
                if (game.boss_->IsMidboss()) midbossSeen = true;
                else if (game.boss_->phaseTimer > 5.1F && !phases[game.boss_->phase - 1]) {
                    phases[game.boss_->phase - 1] = true;
                    const auto name = "phase-" + std::to_string(game.boss_->phase);
                    Capture(game, prefix, name.c_str());
                }
            }
            assert(game.state_ != RunState::GameOver);
        }
        if (frame>=400*60) std::cerr << "Stage stalled: state " << static_cast<int>(game.state_)
            << " stage " << game.stage_.Elapsed() << " dialogue " << game.dialogueActive_
            << " after " << game.dialogueAfterBattle_ << " boss phase " << (game.boss_ ? game.boss_->phase : -1)
            << " hp " << (game.boss_ ? game.boss_->health : -1) << " timer " << (game.boss_ ? game.boss_->phaseTimer : -1) << "\n";
        assert(frame < 400 * 60 && midbossSeen && dialogueSeen);
        for (bool seen : phases) assert(seen);
        assert(game.state_ == RunState::Won && game.bullets_.Pool().ActiveCount() == 0);
        assert(game.assets_.Stage().visuals.empty());
        assert(game.bestScore_ >= 120000);
        Capture(game, prefix, "whole-stage-clear");
        std::cout << "Whole Stage 1 cleared at " << frame / 60.0F << " seconds\n";

        for (int phase : {2, 4}) {
            game.practiceMode_ = game.spellPractice_ = true;
            game.practicePhase_ = phase;
            const int bestScore = game.bestScore_;
            game.StartRun(false, true);
            game.player_.SetInvulnerability(1000);
            assert(game.boss_->phase == phase && game.player_.Power() == Player::kMaxPower);
            for (int step = 0; step < 30 * 60 && game.state_ != RunState::Won; ++step)
                game.Update(1.0F / 60);
            assert(game.state_ == RunState::Won && game.bestScore_ == bestScore);
            // 超时练习只保留实际擦弹分数，不能获得旧版固定 25000 分捕获奖励。
            assert(game.score_ == game.graze_*100 && EnemyCount(game) == 0);
            game.StartRun(false, true); game.player_.SetInvulnerability(1000);
            for(int step=0;step<130;++step)game.Update(1.0F/60);
            assert(game.boss_->CanTakeDamage());
            game.boss_->health=0; game.ResolveCollisions();
            assert(game.score_>=200000 && game.spellBonus_>=200000);
            assert(game.bestScore_==bestScore); // 捕获练习不能写入整关最高分。
        }
        game.StartPreview();
        game.stage_ = StageDirector{};
        game.bullets_.SpawnBeam({-2, 2, 7}, {0, -0.1F, -1}, 24, 0.22F, 1, 0.65F, SKYBLUE);
        game.Update(0.5F);
        Capture(game, prefix, "beam-warning");
        game.TogglePause();
        game.Update(3);
        game.bullets_.Pool().ForEachActive([](const Bullet& bullet) { assert(bullet.age == 0.5F); });
        Capture(game, prefix, "pause");
        game.TogglePause();
        game.Update(0.6F);
        Capture(game, prefix, "beam-active");
        game.ApplyBombEffect();
        assert(game.bullets_.Pool().ActiveCount() == 0);
        game.StartPreview(); // 恢复整关模式，不能把练习设置泄漏给后续调用方。
        assert(!game.spellPractice_ && !game.practiceMode_ && !game.challengeMode_);
    }

    /// @brief 通过正式开局绑定检查两种御札，并渲染真实追踪弹，防止生成正常而素材/动画不可见。
    static void TestPlayerShots(Game& game, const std::string& prefix) {
        const int previousShot=game.selectedShot_;
        for(int shot : {0,1})for(int power : {0,8,128}) {
            game.selectedShot_=shot;
            game.StartPreview();
            game.stage_=StageDirector{};
            game.player_.AdjustPower(power);
            const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
            const auto render=[&] {
                BeginDrawing(); BeginTextureMode(target); ClearBackground(BLACK);
                game.DrawWorld(); EndTextureMode(); EndDrawing();
                Image image=LoadImageFromTexture(target.texture); ImageFlipVertical(&image);
                return image;
            };
            Image background=render();
            game.player_.Update(0,{.fire=true},game.bullets_,game.stageContent_->PlayerBulletVisual(),
                                game.stageContent_->PlayerHomingVisual(),true);
            int homing=0;
            float directionBefore=0;
            game.bullets_.Pool().ForEachActive([&](const Bullet& bullet) {
                if(bullet.homingTurnRate>0) { ++homing; directionBefore+=bullet.velocity.x; }
            });
            assert(homing==(shot==0 && power>=8 ? (power==128 ? 6 : 2) : 0));
            // 偏右目标迫使成对弹的横向合速度改变；不能只检查 homingTurnRate 字段存在。
            const std::array<Vector3,1> targets{{{4,0,4}}};
            game.bullets_.Update(0.2F,targets,game.player_.Position());
            game.UpdateBattleAnimations();
            if(homing>0) {
                float directionAfter=0;
                game.bullets_.Pool().ForEachActive([&](Bullet& bullet) {
                    if(bullet.homingTurnRate>0) {
                        directionAfter+=bullet.velocity.x;
                        const auto& frame=bullet.animation.player.Sample();
                        assert(frame.visible && frame.sprite>=0 && frame.color.a>0);
                    } else bullet.active=false; // 隔离直射弹，像素断言只能由追踪弹满足。
                });
                assert(directionAfter>directionBefore+1);
                Image image=render();
                int ink=0,peak=0;
                for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x) {
                    const Color a=GetImageColor(image,x,y),b=GetImageColor(background,x,y);
                    const int difference=std::abs(int(a.r)-b.r)+std::abs(int(a.g)-b.g)+std::abs(int(a.b)-b.b);
                    peak=std::max(peak,difference);
                    if(difference>3)++ink;
                }
                assert(ink>20 && peak>6);
                if(!prefix.empty()) {
                    const std::string path=prefix+"-homing-power-"+std::to_string(power)+".png";
                    assert(ExportImage(image,path.c_str()));
                }
                UnloadImage(image);
                std::cout<<"Homing Power "<<power<<": "<<homing<<" bullets, "<<ink<<" visible pixels, peak "<<peak<<"\n";
            }
            UnloadImage(background); UnloadRenderTexture(target);
        }
        game.selectedShot_=previousShot;
        game.StartPreview();
    }

    static void Run(Game& game, const std::string& prefix) {
        TestUiFont(game);
        TestPlayerShots(game,prefix);
        TestBossTransitions(game,prefix);
        TestBossHud(game,prefix);
        TestFullWindow(game,prefix);
        TestDialogueTransitions(game,prefix);
        TestPortraits(game,prefix);
        TestWholeStage(game, prefix);
        TestBossEntryPreservesField(game);
        // 经过真实 Game 更新验证高帧率入场，不能把轨道首帧偏移锁定为退场。
        game.StartPreview();
        game.stage_ = StageDirector{};
        game.enemies_.Add(std::make_unique<stage1::FairyEnemy>(stage1::FairyParams{
            {0, -0.5F, 8.8F}, stage1::FairyParams::Movement::Helix}));
        game.enemies_.Add(std::make_unique<stage1::FairyEnemy>(stage1::FairyParams{
            {9, 3.15F, 7.8F}, stage1::FairyParams::Movement::CrossFromRight}));
        for (int frame = 0; frame < 90; ++frame) game.Update(1.0F / 240.0F);
        assert(EnemyCount(game) == 2);
        game.enemies_.ForEachActive([](const Enemy& enemy) {
            assert(enemy.presentation.Sample().alpha == 1.0F);
        });
        Capture(game, prefix, "fairy-visible");

        game.StartPreview();
        game.stage_ = StageDirector{};
        game.enemies_.Add(std::make_unique<stage1::FairyEnemy>(stage1::FairyParams{{0, 0, 4}}));
        game.UpdateEnemies(0.125F);
        Capture(game, prefix, "enemy-entry");
        game.TogglePause();
        const Vector3 pausedPosition = [&] {
            Vector3 position{};
            game.enemies_.ForEachActive([&](const Enemy& enemy) { position = enemy.position; });
            return position;
        }();
        game.Update(2.0F);
        game.enemies_.ForEachActive([&](const Enemy& enemy) {
            assert(enemy.position.z == pausedPosition.z);
            assert(enemy.age == 0.125F);
        });
        game.TogglePause();
        assert(game.state_ == RunState::Playing);
        Bullet* shot = game.bullets_.Spawn(BulletOwner::Player, pausedPosition, {},
                                          0.1F, WHITE, kInvalidVisualId);
        assert(shot);
        shot->damage = 2.0F;
        game.ResolveCollisions();
        assert(!shot->active);
        assert(EnemyCount(game) == 0);
        assert(game.score_ == 500);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);
        const auto pickups = PickupCount(game);
        game.UpdateBattleAnimations();
        game.actorEffects_.Pool().ForEachActive([&](const ActorExitEffect& effect) {
            assert(effect.nativeDefeat);
            const auto& visual=game.assets_.GetVisual(effect.visual);
            assert(visual.animation.atlas==battle::Atlas::BulletEffects);
            assert(visual.animation.script==3 && effect.animation.visible);
            assert(effect.animation.additive && effect.animation.sprite==4);
        });
        Capture(game, prefix, "enemy-defeat");
        game.ResolveCollisions();
        assert(game.score_ == 500 && PickupCount(game) == pickups);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);

        // 槽位可立即复用；新敌人活动，旧快照仍停在旧位置，且不能再被玩家弹命中。
        game.enemies_.Add(std::make_unique<stage1::OrbEnemy>(stage1::OrbParams{{4, 0, 5}}));
        assert(EnemyCount(game) == 1);
        game.actorEffects_.Pool().ForEachActive([&](const ActorExitEffect& effect) {
            assert(effect.position.x == pausedPosition.x && effect.position.z == pausedPosition.z);
            assert(effect.reason == ActorExitReason::Defeated);
        });
        game.BeginStageDialogue(1);
        assert(EnemyCount(game) == 0);
        assert(game.score_ == 500 && PickupCount(game) == pickups);
        assert(game.actorEffects_.Pool().ActiveCount() == 2);
        game.dialogueActive_ = false;
        game.actorEffects_.Update(1.0F);

        game.BeginStageBoss(true, 2);
        game.UpdateBoss(0.3F);
        assert(game.boss_ && game.boss_->active);
        Capture(game, prefix, "boss-entry");
        const float alpha = game.boss_->presentation.Sample().alpha;
        game.boss_->health = 0.0F;
        game.AdvanceBossPhase();
        assert(game.boss_->phase == 2);
        assert(game.boss_->presentation.Sample().alpha == alpha);
        assert(game.actorEffects_.Pool().ActiveCount() == 0);
        for (int phase = 2; phase <= 4; ++phase) {
            game.boss_->health = 0.0F;
            game.AdvanceBossPhase();
        }
        // 末符击破后先完成两句战后独白，再展示关卡奖励；关闭后完成剩余死亡演出与资源释放。
        for(int frame=0;frame<62;++frame)game.UpdateBoss(1.0F/60);
        assert(!game.boss_->active);
        game.BeginStageDialogue(4,true);
        assert(game.state_==RunState::Playing && game.dialogueActive_ && game.dialogueTimer_==0);
        assert(!game.stageClearShown_);
        game.dialogueTimer_=game.DialogueDuration();
        game.Update(0);
        assert(game.state_==RunState::StageClear && !game.dialogueActive_);
        game.ContinueStageClear();
        assert(game.state_ == RunState::Finishing);
        assert(!game.boss_);
        assert(!game.assets_.Stage().visuals.empty());
        assert(game.bullets_.Pool().ActiveCount() == 0 && PickupCount(game) == 0);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);
        const auto finalScore = game.score_;
        game.AdvanceBossPhase();
        assert(game.score_ == finalScore);
        game.Update(0.2F);
        Capture(game, prefix, "boss-defeat");
        game.TogglePause();
        game.Update(2.0F);
        assert(game.state_ == RunState::Paused);
        assert(!game.assets_.Stage().visuals.empty());
        game.actorEffects_.Pool().ForEachActive([](const ActorExitEffect& effect) {
            assert(effect.age == 0.2F);
        });
        game.TogglePause();
        assert(game.state_ == RunState::Finishing);
        game.Update(0.2F);
        assert(game.state_ == RunState::Finishing);
        assert(!game.assets_.Stage().visuals.empty());
        game.Update(0.5F);
        assert(game.state_ == RunState::Won);
        assert(game.assets_.Stage().visuals.empty());
        assert(game.actorEffects_.Pool().ActiveCount() == 0);
        Capture(game, prefix, "stage-clear");

        game.StartPreview();
        game.stage_ = StageDirector{};
        game.enemies_.Add(std::make_unique<stage1::FairyEnemy>(stage1::FairyParams{{0, 0, -13.4F}}));
        game.UpdateEnemies(0.1F);
        assert(EnemyCount(game) == 0 && game.score_ == 0 && PickupCount(game) == 0);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);
        game.UpdateEnemies(0.1F);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);
        game.actorEffects_.Clear();

        game.BeginStageBoss(false, 3);
        for(int frame=0;frame<1503;++frame)game.UpdateBoss(1.0F/60);
        assert(game.state_ == RunState::Playing);
        assert(!game.boss_->active);
        game.actorEffects_.Pool().ForEachActive([](const ActorExitEffect& effect) {
            assert(effect.reason == ActorExitReason::Retreat);
            assert(effect.duration == 0.5F);
        });
        game.actorEffects_.Update(0.2F);
        Capture(game, prefix, "boss-retreat");

        game.StartPreview();
        assert(game.actorEffects_.Pool().ActiveCount() == 0);
        game.BeginStageBoss(true, 4);
        game.EndStage(RunState::GameOver);
        assert(game.state_ == RunState::Finishing);
        assert(!game.assets_.Stage().visuals.empty());
        game.Update(0.6F);
        assert(game.state_ == RunState::GameOver && game.assets_.Stage().visuals.empty());

        game.StartPreview();
        game.BeginStageBoss(true, 5);
        game.EndStage(RunState::Won);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);
        game.Reset();
        assert(game.state_ == RunState::MainMenu);
        assert(game.actorEffects_.Pool().ActiveCount() == 0);
        assert(game.assets_.Stage().visuals.empty());
    }
};

}  // namespace koumakyou

/// @brief 隐藏窗口集成测试；argv[1] 为可选截图前缀，argv[2] 支持 --viewport-only、--dialogue-only、--boss-hud-only、--player-shots-only。
/// 不初始化音频，设置与成绩只写临时目录。
int main(int argc, char** argv) {
    const auto originalDirectory = std::filesystem::current_path();
    const std::string prefix = argc >= 2 ? std::filesystem::absolute(argv[1]).string() : "";
    const auto scratch = std::filesystem::temp_directory_path() /
        ("koumakyou_stage_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(scratch);
    std::filesystem::current_path(scratch); // 配置和成绩只写入临时目录，不能改用户的实际记录。
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNFOCUSED | FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(1100, 760, "Actor lifecycle tests");
    SetExitKey(KEY_NULL);
    {
        auto game = std::make_unique<koumakyou::Game>();
        SetTargetFPS(0);
        if(argc>=3 && std::string(argv[2])=="--viewport-only") {
            koumakyou::GameLifecycleProbe::TestUiFont(*game);
            koumakyou::GameLifecycleProbe::TestFullWindow(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--player-shots-only") {
            koumakyou::GameLifecycleProbe::TestPlayerShots(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--boss-hud-only") {
            koumakyou::GameLifecycleProbe::TestUiFont(*game);
            koumakyou::GameLifecycleProbe::TestBossTransitions(*game,prefix);
            koumakyou::GameLifecycleProbe::TestBossHud(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--dialogue-only") {
            koumakyou::GameLifecycleProbe::TestUiFont(*game);
            koumakyou::GameLifecycleProbe::TestDialogueTransitions(*game,prefix);
            koumakyou::GameLifecycleProbe::TestPortraits(*game,prefix);
        } else {
            koumakyou::GameLifecycleProbe::Run(*game, prefix);
        }
    }
    CloseWindow();
    std::filesystem::current_path(originalDirectory);
    std::filesystem::remove_all(scratch);
    std::cout << "Actor lifecycle integration tests passed\n";
}
