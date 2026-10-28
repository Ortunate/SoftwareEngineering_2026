// 本文件负责在隐藏图形窗口中验证自机射击、本体碰撞、Game 战斗演出、结算及资源生命周期。

#include "gameplay/bosses/rumia.hpp"
#include "boss_fixture.hpp"
#include "actor_fixture.hpp"
#include "app/game.hpp"
#include "rlgl.h"
#include "stages/stage1/behaviors.hpp"
#include "stages/stage1/boss_config.hpp"
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
#include <cstring>

namespace koumakyou {

/// @brief 仅此测试定义的友元探针；调用正式游戏路径验证效果不重复奖励或保留战斗对象。
struct GameLifecycleProbe {
    /// @brief 两种字重均须加载真实中英文字形和等宽数字，避免浮窗加粗静默回退或分数宽度跳动。
    static void TestUiFont(const Game& game) {
        const auto& common = game.assets_.Common();
        assert(common.uiFont.texture.id != common.hudFont.texture.id);
        for (const Font font : {common.uiFont, common.hudFont}) {
            assert(font.texture.id != GetFontDefault().texture.id);
            for (int codepoint : std::array<int, 10>{'0', '9', 'Z', 0x4e2d, 0x300c, 0x2605, 0x5e7b, 0x5377,0x5723,0x9489})
                assert(font.glyphs[GetGlyphIndex(font, codepoint)].value == codepoint);
            const float zeroWidth = ui::MeasureText(font, "000000000", 38).x;
            assert(std::abs(zeroWidth - ui::MeasureText(font, "111111111", 38).x) < 0.001F);
            assert(std::abs(zeroWidth - ui::MeasureText(font, "987654321", 38).x) < 0.001F);
        }
    }
    static std::size_t EnemyCount(const Game& game) {
        std::size_t count = 0;
        game.actors_.ForEachActive([&](const behavior::Actor&) { ++count; });
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

    /// @brief 经过正式碰撞总控，检查本体/弹幕同帧只扣一次、无敌和对话门禁、敌方不受碰撞伤害。
    static void TestActorContacts(Game& game) {
        const auto MakeTarget=[](Vector3 p) {
            auto spawn=testing::SpawnAt(p,stage1::kFairy,100,0.5F);spawn.damageable=false;return spawn;
        };
        struct TargetBoss final:Boss {
            explicit TargetBoss(Vector3 p){active=true;position=p;health=maxHealth=100;visual=stage1::kRumia;}
            bool Update(float,Vector3,BulletSystem&) override {return false;}
            bool AdvanceSkill() override {return false;}
            const char* SpellName() const override {return nullptr;}
            const char* DisplayName() const override {return "Body contact target";}
            bool CanTakeDamage() const override {return false;}
        };
        game.StartPreview();game.stage_=StageDirector{};
        const auto reset=[&] {
            game.ClearPlayerBomb();game.actors_.Clear();game.boss_.reset();game.bullets_.Reset();
            game.actorEffects_.Clear();game.drops_.Clear();game.dialogueActive_=false;game.state_=RunState::Playing;
            game.player_.Reset();game.player_.ConfigureRun(3,3,false,ShotType::HomingAmulet);
            game.player_.SetInvulnerability(0);game.player_.AdjustPower(72);game.player_.SyncCamera(game.camera_);
            game.missCount_=game.graze_=0;game.score_=0;
#ifdef KOUMAKYOU_DEBUG_FEATURES
            game.debugInvincible_=false;
#endif
        };
        reset();const auto position=game.player_.Position();
        assert(game.actors_.Spawn(MakeTarget(position)));
        assert(game.actors_.Spawn(MakeTarget(position)));
        game.boss_=std::make_unique<TargetBoss>(position);
        auto* bullet=game.bullets_.Spawn(BulletOwner::Enemy,position,{},0.1F,RED,stage1::kBlueBullet);assert(bullet);
        game.ResolveCollisions();
        assert(game.missCount_==1 && game.player_.RemainingLives()==3 && game.player_.Power()==64);
        assert(game.score_==0 && game.graze_==0 && PickupCount(game)==0 && game.actorEffects_.Pool().ActiveCount()==0);
        game.actors_.ForEachActive([](const behavior::Actor& enemy){assert(enemy.health==100);});
        assert(EnemyCount(game)==2 && game.boss_->health==100 && !bullet->active);
        game.ResolveCollisions();assert(game.missCount_==1); // 重叠不绕过复活无敌。
        game.player_.SetInvulnerability(0);game.ResolveCollisions();assert(game.missCount_==2);

        reset();game.boss_=std::make_unique<TargetBoss>(position);game.ResolveCollisions();
        assert(game.missCount_==1); // Boss 单独接触也必须生效。
        reset();assert(game.actors_.Spawn(MakeTarget(position)));
        game.dialogueActive_=true;game.ResolveCollisions();assert(game.missCount_==0);
        game.dialogueActive_=false;game.state_=RunState::Paused;game.ResolveCollisions();assert(game.missCount_==0);
#ifdef KOUMAKYOU_DEBUG_FEATURES
        reset();assert(game.actors_.Spawn(MakeTarget(position)));
        game.debugInvincible_=true;game.ResolveCollisions();assert(game.missCount_==0 && game.player_.Power()==80);
#endif
        reset();game.BeginStageBoss(testing::BattleAt(*game.stageContent_,0),3);game.boss_->position=position;
        assert(game.boss_->AdvanceSkill());game.ResolveCollisions();assert(game.missCount_==0);
        // 四面关闭本体接触的晶体仍吸收自机弹；隐藏/非交互发射体则同时退出接触、伤害和弹接触。
        for(int mode=0;mode<3;++mode) {
            reset();auto* actor=game.actors_.Spawn(MakeTarget(position));assert(actor);
            actor->contactEnabled=false;
            if(mode==1)actor->visible=false;
            if(mode==2)actor->interactable=false;
            auto* shot=game.bullets_.Spawn(BulletOwner::Player,position,{},0.1F,WHITE,stage1::kPlayerGoldCard);assert(shot);
            game.ResolveCollisions();assert(game.missCount_==0&&actor->health==100);
            assert(shot->active==(mode!=0));
            if(mode==1) {game.ClearActorsWithEffects();assert(game.actorEffects_.Pool().ActiveCount()==0);}
        }
        game.Reset();std::cout<<"Player/enemy/Boss body contacts and protection gates passed\n";
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
            // 在不同显示帧间隙送入伤害，最多等待一个 120 Hz 战斗步；准备期仍须立即保护。
            for(int alignment=0;alignment<std::max(1,fps/60);++alignment) {
                const float dt=1.0F/fps;
                game.bullets_.Reset(); game.drops_.Clear();
                game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),1);
                const auto ready=[&](int phase) {
                    int frames=0;
                    while(!game.boss_->CanTakeDamage() && frames++<5*fps) {
                        game.Update(dt);
                        assert(game.boss_->attack==phase);
                    }
                    assert(game.boss_->attack==phase && game.boss_->CanTakeDamage());
                };
                const auto hit=[&] {
                    auto* shot=game.bullets_.Spawn(BulletOwner::Player,game.boss_->position,{},0.1F,WHITE,kInvalidVisualId);
                    assert(shot); shot->damage=100000;
                    const int phase=game.boss_->attack;
                    for(int frame=0;frame<std::max(1,(fps+119)/120)&&game.boss_->attack==phase;++frame)
                        game.Update(dt); // 经过实际固定步、弹幕移动与碰撞，不能直接推进阶段。
                };
                ready(1); hit();
                assert(game.boss_->attack==2);
                ready(2);
                for(int frame=0;frame<alignment;++frame)game.Update(dt);
                if(timeout) {
                    int frames=0;
                    while(game.boss_->attack==2 && frames++<30*fps)game.Update(dt);
                } else hit();
                assert(game.boss_->attack==3 && game.boss_->SpellName()==nullptr);
                assert(game.boss_->health==7500);
                assert(game.boss_->maxHealth==7500);
                const auto bar=game.boss_->HealthBar();
                assert(bar.remainingBars==0 && std::abs(bar.segments[1].remaining-6700.0F/7500)<0.00001F);
                for(int frame=0;frame<std::max(1,fps/60);++frame) {
                    game.Update(dt);
                    assert(game.boss_->attack==3 && game.boss_->SpellName()==nullptr);
                }
                ready(3);
                for(int frame=0;frame<static_cast<int>(1.1F*fps);++frame) {
                    game.Update(dt);
                    assert(game.boss_->attack==3);
                }
                int beams=0;
                game.bullets_.Pool().ForEachActive([&](const Bullet& bullet) { if(bullet.beamLength>0)++beams; });
                assert(beams==6 && game.boss_->TimeLimit()==30);
                if(!timeout && fps==240 && alignment==3)Capture(game,prefix,"second-nonspell-restored");
                hit();
                assert(game.boss_->attack==4 && game.boss_->SpellName()!=nullptr);
                assert(std::abs(game.boss_->HealthBar().segments[0].remaining-800.0F/7500)<0.00001F);
            }
        }
        std::cout<<"Boss damage/timeout transitions and second nonspell beams passed at 30/60/120/240 FPS\n";
    }

    /// @brief 通过正式 HUD 读取分段颜色与空白像素，覆盖四个攻击阶段、道中及多种窗口比例。
    static void TestBossHud(Game& game, const std::string& prefix) {
        game.StartPreview();
        const int width=GetScreenWidth(),height=GetScreenHeight();
        struct Scene { int phase; float sourceHealth; const char* name; bool configured=false; };
        const std::array<Scene,11> scenes{{
            {1,7000,"first-full"},{1,3950,"first-normal-half"},{1,900,"first-boundary"},
            {2,900,"first-spell-full"},{2,450,"first-spell-half"},{2,0,"first-spell-empty"},
            {3,7500,"second-full"},{3,4150,"second-normal-half"},
            {4,400,"second-spell-half"},{0,6000,"midboss-full"},{1,3950,"custom-ratios-colors",true}}};
        for(const Vector2 size : {Vector2{1280,720},{900,900},{390,844},{1600,400},{320,240}}) {
            SetWindowSize(static_cast<int>(size.x),static_cast<int>(size.y));
            const GameplayLayout layout(GetScreenWidth(),GetScreenHeight());
            const ui::BossHudLayout row(layout.BossPanel(),layout.scale);
            const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
            for(const auto& scene : scenes) {
                game.BeginStageBoss(testing::BattleAt(*game.stageContent_,static_cast<std::size_t>(scene.phase!=0)),1);
                if(scene.configured) {
                    auto config=stage1::RumiaFinalConfig();
                    config.phases[0].healthBar.segments={{0,0.3F,GREEN},{1,0.7F,BLUE}};
                    config.phases[0].healthBar.background=BLACK;
                    dynamic_cast<bosses::RumiaBoss&>(*game.boss_).StartBoss(config);
                }
                while(game.boss_->attack<scene.phase)assert(game.boss_->AdvanceSkill());
                game.UpdateBoss(1.0F/60);
                game.bullets_.Reset(); game.drops_.Clear(); // 隔离 Sub19 的散落，截图不能累积上一个展示场景的对象。
                game.boss_->skillTimer=5;
                game.boss_->health=scene.sourceHealth;
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
                            const Color expected=segment.color;
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
        // 用不透明纯色替代纹理，直接验证最终像素混合，防止把变暗 tint 误当作原生 Alpha 128。
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
        constexpr float inactive=128.0F/255;
        const float turn=2648.0F/60;
        game.dialogueTimer_=turn-1.0F/60; game.UpdateUiMotion(0);
        pixels(1,inactive);
        game.dialogueTimer_=turn; game.UpdateUiMotion(0);
        pixels(1,inactive);
        game.dialogueTimer_=turn+8.0F/60;game.UpdateUiMotion(0);
        pixels(191.0F/255,191.0F/255);
        const auto paused=game.dialogueMotion_.Sample(false);
        game.TogglePause(); game.Update(0.5F);
        assert(game.dialogueMotion_.Sample(false).opacity==paused.opacity);
        assert(game.dialogueMotion_.Sample(false).focus==paused.focus);
        game.TogglePause();game.dialogueTimer_=turn+16.0F/60;game.UpdateUiMotion(0);
        pixels(inactive,1);
        for(const auto& [id,original]:originals)visuals[id]=original;
        UnloadTexture(red); UnloadTexture(green); UnloadRenderTexture(target);

        // 正式差分逐帧审阅两个方向的过渡，保持其他世界状态不动；Draw 不推进动画。
        game.dialogueTimer_=at(8); game.UpdateUiMotion(0.3F);
        Capture(game,prefix,"dialogue-focus-left");
        for(int direction=0;direction<2;++direction) {
            const float boundary=(direction==0?2648.0F:2949.0F)/60;
            for(int frame=0;frame<8;++frame) {
                game.dialogueTimer_=boundary+frame*0.04F;game.UpdateUiMotion(0);
                const std::string name="dialogue-turn-"+std::to_string(direction)+"-"+std::to_string(frame);
                Capture(game,prefix,name.c_str());
            }
        }
        game.BeginStageDialogue(2,true);
        assert(game.dialogueMotion_.Sample(false).slide==1 && game.dialogueMotion_.Sample(true).opacity==0);
        game.dialogueTimer_=0.5F;game.UpdateUiMotion(0);
        assert(game.dialogueMotion_.Sample(false).opacity==1 && game.dialogueMotion_.Sample(true).opacity==0);
        std::cout<<"Dialogue transition opacity, pause and edge tests passed\n";
    }

    /// @brief 逐句核对两位自机战前/战后真实差分、音乐触发和末尾退场，截图覆盖横屏与竖屏。
    static void TestNativeDialogue(Game& game,const std::string& prefix) {
        const int width=GetScreenWidth(),height=GetScreenHeight();
        for(int character=0;character<2;++character) {
            game.selectedCharacter_=character;game.selectedShot_=0;game.StartPreview();
            game.stage_=StageDirector{};game.actors_.Clear();game.boss_.reset();game.drops_.Clear();
            for(bool after:{false,true}) {
                game.BeginStageDialogue(1,after);
                const auto lines=game.CurrentDialogue();
                float start=0;
                for(std::size_t index=0;index<lines.size();++index) {
                    game.dialogueTimer_=start+std::max(1.1F,lines[index].textDelay+lines[index].line2Delay+0.1F);
                    game.UpdateUiMotion(0);
                    const auto expressions=DialogueExpressionsAt(lines,index,game.dialogueTimer_-start);
                    const auto hero=game.stageContent_->HeroPortraitVisual(expressions[0],game.player_.Shot());
                    assert(IsTextureValid(game.assets_.GetVisual(hero).texture));
                    assert(!lines[index].showBoss || game.dialogueMotion_.Sample(true).opacity>0);
                    for(const auto size:std::array<Vector2,2>{{{1100,760},{390,844}}}) {
                        SetWindowSize(size.x,size.y);
                        const auto name="native-dialogue-"+std::to_string(character)+(after?"-after-":"-before-")+
                            std::to_string(index)+"-"+std::to_string(static_cast<int>(size.x));
                        Capture(game,prefix,name.c_str());
                    }
                    start+=lines[index].duration;
                }
                if(!after) {
                    const float musicFrame=character==0?3852.0F:2467.0F;
                    game.BeginStageDialogue(1);game.dialogueTimer_=(musicFrame-1)/60;game.Update(1.0F/120);
                    assert(!game.dialogueBossMusic_);
                    game.dialogueTimer_=musicFrame/60;game.Update(1.0F/120);assert(game.dialogueBossMusic_);
                    game.dialogueTimer_=game.DialogueDuration()-1.0F/60;game.UpdateUiMotion(0);
                    assert(game.dialogueMotion_.Sample(false).slide>0.99F && game.dialogueMotion_.Sample(true).slide>0.99F);
                }
                game.dialogueActive_=false;
            }
        }
        SetWindowSize(width,height);game.selectedCharacter_=0;game.selectedShot_=0;game.StartPreview();
        std::cout<<"Both native Stage 1 dialogue branches, portraits, BGM and exits passed\n";
    }

    /// @brief 在真实伤害、清弹、死亡掉落和 Boss 保护入口验证 Bomb，避免仅凭画面通过。
    static void TestPlayerBombCombat(Game& game) {
        // 固定靶借用本关外观；不移动或发弹，允许单独控制无敌和原始生命。
        const auto MakeTarget=[](Vector3 p,float hp) {
            return testing::SpawnAt(p,stage1::kFairy,hp,0.4F);
        };
        struct SpellTarget final:Boss {
            bool damageable=true;
            SpellTarget() {active=true;position={0,0,4};health=maxHealth=1000;visual=stage1::kRumia;}
            bool Update(float,Vector3,BulletSystem&) override {return false;}
            bool AdvanceSkill() override {return false;}
            const char* SpellName() const override {return "Bomb test";}
            const char* DisplayName() const override {return "Bomb test";}
            bool CanTakeDamage() const override {return damageable;}
        };
        const auto reset=[&](ShotType shot) {
            game.ClearPlayerBomb();game.stage_=StageDirector{};game.actors_.Clear();game.boss_.reset();
            game.bullets_.Reset();game.drops_.Clear();game.actorEffects_.Clear();
            game.player_.Reset();game.player_.ConfigureRun(3,3,false,shot);game.player_.SyncCamera(game.camera_);
        };
        reset(ShotType::Needle);
        auto* untouched=game.actors_.Spawn(MakeTarget({9,4,4},1000));
        auto* touched=game.actors_.Spawn(MakeTarget({0,0,-6.2F},1000));assert(untouched&&touched);
        game.ApplyBombEffect();
        assert(game.screenFlashTimer_==0 && untouched->health==1000 && touched->health==1000);
        game.UpdatePlayerBomb(1);
        assert(game.playerBomb_.Frame()==60 && untouched->health==1000 && touched->health==1000);
        game.UpdatePlayerBomb(1.0F/60);
        assert(untouched->health==1000 && touched->health<1000); // 先接触施放中心附近，不能倒着从边界清怪。
        const float firstHit=touched->health;
        game.UpdatePlayerBomb(1.0F/60);assert(untouched->health==1000 && touched->health==firstHit);
        game.UpdatePlayerBomb(77.0F/60);assert(untouched->health<1000);

        reset(ShotType::IllusionLaser);
        auto* hit=game.actors_.Spawn(MakeTarget({0,0,4},24));
        auto* miss=game.actors_.Spawn(MakeTarget({0,8,-9},1000));
        auto* covered=game.actors_.Spawn(MakeTarget({9,9,-6.25F},1000));
        auto* safe=game.actors_.Spawn(MakeTarget({0,0,4},1000));
        assert(hit&&miss&&covered&&safe);safe->damageable=false;
        auto* enemy=game.bullets_.Spawn(BulletOwner::Enemy,{0,0,4},{},0.1F,RED,stage1::kBlueBullet);
        auto* back=game.bullets_.Spawn(BulletOwner::Enemy,{0,0,-9},{},0.1F,RED,stage1::kBlueBullet);
        auto* friendly=game.bullets_.Spawn(BulletOwner::Player,{0,0,4},{},0.1F,WHITE,stage1::kPlayerGoldCard);
        auto* beam=game.bullets_.SpawnBeam({0,-5,4},{0,1,0},10,0.1F,0,5,RED);
        assert(enemy && back && friendly && beam);friendly->damage=0;
        game.drops_.SpawnFixedPower({-8,8,2},1);game.ApplyBombEffect();
        assert(hit->health==24 && enemy->active && back->active && beam->active); // 释放时没有全场扣血/清弹。
        game.drops_.ForEachActive([](const Pickup& item) {assert(item.homing);});
        game.UpdatePlayerBomb(1.0F/60);
        assert(hit->health==12 && covered->health==988 && miss->health==1000 && safe->health==1000);
        assert(!enemy->active && !beam->active && back->active && friendly->active);
        game.ResolvePlayerBombFrame();assert(hit->health==12);
        game.TogglePause();game.Update(1);assert(hit->health==12 && game.playerBomb_.Frame()==1);game.TogglePause();
        game.UpdatePlayerBomb(1.0F/60);assert(hit->health==0);
        game.ResolveCollisions();assert(!hit->active && PickupCount(game)==2 && game.actorEffects_.Pool().ActiveCount()==1);
        game.ResolveCollisions();assert(PickupCount(game)==2);

        reset(ShotType::HomingAmulet);
        auto* orbTarget=game.actors_.Spawn(MakeTarget({0,0,4},3000));assert(orbTarget);orbTarget->hitRadius=20;
        game.ApplyBombEffect();
        game.UpdatePlayerBomb(93.0F/60);
        assert(game.playerBomb_.Frame()==93 && orbTarget->health==2826); // 13×8 接触，随后一帧 200 截到 70。
        assert(game.playerBomb_.Attacks().size()==1 && game.playerBomb_.Attacks()[0].damage==200);
        game.UpdatePlayerBomb(1.0F/60);assert(orbTarget->health==2826 && game.playerBomb_.Attacks().empty());

        reset(ShotType::IllusionLaser);
        auto boss=std::make_unique<SpellTarget>();auto* spell=boss.get();game.boss_=std::move(boss);
        game.spellStartBombs_=game.bombCount_;game.ApplyBombEffect();game.UpdatePlayerBomb(1.0F/60);
        assert(spell->health==996);
        spell->damageable=false;game.UpdatePlayerBomb(1.0F/60);assert(spell->health==996);
        spell->damageable=true;game.spellStartBombs_=game.bombCount_; // 模拟新符卡开始，旧 Bomb 不得带入伤害。
        game.UpdatePlayerBomb(1.0F/60);assert(spell->health==996);
        game.ApplyBombEffect();game.UpdatePlayerBomb(1.0F/60);assert(spell->health==992);

        // 每帧重新生成敌弹及两端在外的横穿激光，验证后续来弹也会清除；不能只在释放时清一次。
        // 使用独立帧条件核对数量，跳过伤害帧时不得沿用上一帧残留实体。
        for(int type=0;type<4;++type) {
            const auto shot=static_cast<ShotType>(type);reset(shot);game.ApplyBombEffect();
            for(int frame=0;frame<BombDefinition(shot).durationFrames;++frame) {
                auto next=game.playerBomb_;
                if(frame>0)next.Update(1.0F/60,game.camera_);
                const std::size_t expected=type==0 ? (frame==0 ? 1 : frame<80 || frame>270 ? 0 : std::min(7,(frame-80)/16+1)) :
                    type==1 ? (frame>60 ? 6 : 0) : type==2 ? (frame>0 && frame%3!=0 ? 64 : 0) :
                    (frame>0 && frame%4!=0 ? 1 : 0);
                assert(next.CancelVolumes().size()==expected);
                const auto center=expected ? next.CancelVolumes()[0].origin : game.camera_.position;
                game.bullets_.Reset();
                auto* incoming=game.bullets_.Spawn(BulletOwner::Enemy,center,{},0.1F,RED,stage1::kBlueBullet);
                auto* playerShot=game.bullets_.Spawn(BulletOwner::Player,center,{},0.1F,WHITE,stage1::kPlayerGoldCard);
                auto* crossing=game.bullets_.SpawnBeam({center.x-50,center.y,center.z},{1,0,0},100,0.1F,0,5,RED);
                assert(incoming && playerShot && crossing);
                game.UpdatePlayerBomb(frame==0 ? 0 : 1.0F/60);
                assert(incoming->active==(expected==0) && crossing->active==(expected==0) && playerShot->active);
            }
            game.UpdatePlayerBomb(1.0F/60);
            assert(!game.playerBomb_.Active() && game.playerBomb_.CancelVolumes().empty());
        }
        reset(ShotType::HomingAmulet);
        std::cout<<"Bomb spatial damage, channel union, cancellation, defeat drops, homing burst, spell protection and pause passed\n";
    }

    /// @brief 使用真实 Game 入口绑定四 Bomb；隔离特效像素，验证主/侧视角、暂停及资源卸载。
    static void TestPlayerBombs(Game& game,const std::string& prefix) {
        const int width=GetScreenWidth(),height=GetScreenHeight();
        game.StartPreview();
        TestPlayerBombCombat(game);
        for(int type=0;type<4;++type) {
            SetWindowSize(width,height);
            game.stage_=StageDirector{};game.actors_.Clear();game.boss_.reset();game.bullets_.Reset();
            game.player_.Reset();game.player_.ConfigureRun(3,3,false,static_cast<ShotType>(type));
            game.player_.SyncCamera(game.camera_);
            const bool consumed=game.player_.Update(0,{.bomb=true},game.bullets_,game.stageContent_->PlayerVisuals(game.player_.Shot()),true);
            assert(consumed && game.player_.Bombs()==2);game.ApplyBombEffect();
            assert(game.playerBomb_.Active() && game.playerBomb_.Shot()==game.player_.Shot() && game.playerBomb_.Frame()==0);
            const auto visuals=game.stageContent_->PlayerBombVisuals(game.player_.Shot());
            const auto& definition=BombDefinition(game.player_.Shot());
            for(int layer=0;layer<definition.layers;++layer) {
                const auto& visual=game.assets_.GetVisual(visuals[layer]);
                assert(visual.archive && IsTextureValid(visual.texture));
                assert(visual.animation.script==definition.firstScript+layer);
            }
            for(int frame=1;frame<=120;++frame) {
                game.Update(1.0F/60);
                if(!prefix.empty() && (type==1 || type==3) && (frame==30 || frame==61 || frame==90))
                    Capture(game,prefix,("bomb-"+std::to_string(type)+"-frame-"+std::to_string(frame)).c_str());
            }
            assert(game.playerBomb_.Frame()==120);
            const auto target=LoadRenderTexture(1100,760);
            BeginDrawing();BeginTextureMode(target);ClearBackground(WHITE);BeginMode3D(game.camera_);
            const auto beforeView=rlGetMatrixModelview(),beforeProjection=rlGetMatrixProjection();
            game.DrawPlayerBombBackdrop(1100,760);
            const auto afterView=rlGetMatrixModelview(),afterProjection=rlGetMatrixProjection();
            assert(std::memcmp(&beforeView,&afterView,sizeof(Matrix))==0 && std::memcmp(&beforeProjection,&afterProjection,sizeof(Matrix))==0);
            EndMode3D();EndTextureMode();EndDrawing();
            Image dimmed=LoadImageFromTexture(target.texture);
            for(const auto point:std::array<Vector2,3>{{{0,0},{550,380},{1099,759}}}) {
                const auto pixel=GetImageColor(dimmed,point.x,point.y);
                assert(std::abs(int(pixel.r)-(255-game.playerBomb_.BackdropAlpha()))<=2);
            }
            UnloadImage(dimmed);
            std::vector<BombVolumeSprite> volumes;
            for(const auto& sprite:game.playerBomb_.Sprites())volumes.push_back({&sprite,visuals[sprite.layer]});
            BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);BeginMode3D(game.camera_);
            game.spriteRenderer_.Draw(game.camera_,game.bullets_,{},nullptr,{},game.assets_.Stage().visuals,
                                      game.assets_.Stage().shaders,game.actorEffects_,volumes);
            EndMode3D();EndTextureMode();EndDrawing();
            Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
            const auto* pixels=static_cast<const Color*>(image.data);int ink=0,bright=0;
            for(int i=0;i<image.width*image.height;++i) {
                const int sum=pixels[i].r+pixels[i].g+pixels[i].b;
                if(sum>24)++ink;if(sum>500)++bright;
            }
            std::cout<<"Bomb "<<type<<": "<<game.playerBomb_.Sprites().size()<<" visual layers, "<<ink<<" isolated pixels\n";
            assert(ink>1000); // 不能让背景压暗或宣告文字冒充 Bomb 本体已经渲染。
            if(type==0)assert(bright<image.width*image.height/2); // 近眼盘旋不能把整屏染亮。
            if(type==3) {
                // 保留整个瞄准区域，而非只挖空中心一个像素；真实束内流光不得变成白色圆盘。
                for(int x=image.width/2-30;x<=image.width/2+30;++x)
                    for(int y=image.height/2-30;y<=image.height/2+30;++y) {
                        const auto center=GetImageColor(image,x,y);
                        assert(center.r+center.g+center.b<30);
                    }
            }
            if(!prefix.empty())assert(ExportImage(image,(prefix+"-bomb-"+std::to_string(type)+"-isolated.png").c_str()));
            UnloadImage(image);UnloadRenderTexture(target);
            const auto originalCamera=game.camera_;
            game.camera_=type==3 ? Camera3D{{45,35,-25},{0,0,6},{0,1,0},50,CAMERA_PERSPECTIVE} :
                                 Camera3D{{18,12,-2},{0,0,5},{0,1,0},50,CAMERA_PERSPECTIVE};
            Capture(game,prefix,("bomb-"+std::to_string(type)+"-side").c_str());
            game.camera_=originalCamera;
            if(type==3) {
                const auto begin=std::chrono::steady_clock::now();
                for(int frame=0;frame<24;++frame) {BeginDrawing();game.Draw();EndDrawing();}
                const auto milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/24;
                std::cout<<"Master Spark full scene render: "<<milliseconds<<" ms/frame (24 frames, no image readback)\n";
                const auto saved=game.playerBomb_;game.playerBomb_.Clear();
                const auto baseline=std::chrono::steady_clock::now();
                for(int frame=0;frame<24;++frame) {BeginDrawing();game.Draw();EndDrawing();}
                std::cout<<"Baseline full scene render: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-baseline).count()/24<<" ms/frame\n";
                game.playerBomb_=saved;
            }
            for(const auto size:std::array<Vector2,2>{{{1100,760},{390,844}}}) {
                SetWindowSize(size.x,size.y);
                const auto name="bomb-"+std::to_string(type)+"-"+std::to_string(static_cast<int>(size.x));
                Capture(game,prefix,name.c_str());
            }
            const auto frozen=game.playerBomb_.Frame();
            game.TogglePause();game.Update(0.5F);assert(game.playerBomb_.Frame()==frozen);
            BeginDrawing();game.Draw();game.Draw();EndDrawing();assert(game.playerBomb_.Frame()==frozen);
            game.TogglePause();
            game.playerBomb_.Update((definition.durationFrames-frozen)/60.0F,game.camera_);
            assert(!game.playerBomb_.Active() && game.playerBomb_.Sprites().empty());
            game.ApplyBombEffect();assert(game.playerBomb_.Active());
            game.EndStage(RunState::Won);assert(!game.playerBomb_.Active());
            game.state_=RunState::Playing;
        }
        game.ApplyBombEffect();assert(game.playerBomb_.Active());
        game.EnterMainMenu();assert(!game.playerBomb_.Active() && game.playerBomb_.Sprites().empty());
        SetWindowSize(width,height);
        std::cout<<"Four native Bomb effects, isolated rendering, portrait/landscape, pause and cleanup passed\n";
    }

    /// @brief 静音加载真实 Master Spark 波形，检查暂停、恢复和退出不会遗留五秒长音效。
    static void TestPlayerBombAudio(Game& game) {
        assert(IsAudioDeviceReady());game.StartPreview();
        game.player_.ConfigureRun(3,3,false,ShotType::IllusionLaser);game.ApplyBombEffect();
        assert(game.playerBomb_.Active());
        game.TogglePause();assert(game.playerBombSoundPaused_[3]);
        assert(!game.assets_.PauseSoundEffect("bomb_master"));
        game.TogglePause();assert(!game.playerBombSoundPaused_[3]);
        assert(game.assets_.PauseSoundEffect("bomb_master"));game.assets_.ResumeSoundEffect("bomb_master");
        game.EnterMainMenu();
        assert(!game.playerBomb_.Active() && !game.assets_.PauseSoundEffect("bomb_master"));
        for(bool paused:game.playerBombSoundPaused_)assert(!paused);
        std::cout<<"Native Master Spark WAV pause, resume and exit cancellation passed (muted)\n";
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
        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),2);
        game.boss_->AdvanceSkill();
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
            auto moving=testing::SpawnAt({4,0,5});moving.pattern.movement.speed=1;
            game.actors_.Spawn(moving);
            Bullet* enemyShot = game.bullets_.Spawn(
                BulletOwner::Enemy, {-4, 0, 0}, {1, 0, 0}, 0.1F, WHITE, kInvalidVisualId);
            Bullet* playerShot = game.bullets_.Spawn(
                BulletOwner::Player, {-4, 2, 0}, {1, 0, 0}, 0.1F, WHITE, kInvalidVisualId);
            assert(enemyShot && playerShot);
            game.drops_.SpawnFixedPower({4, 0, 0}, 1);
            const auto pickups = PickupCount(game);
            assert(pickups > 0);

            game.BeginStageBoss(testing::BattleAt(*game.stageContent_,static_cast<std::size_t>(final)), 1);
            assert(game.boss_ && game.boss_->active);
            assert(EnemyCount(game) == 1);
            assert(enemyShot->active && playerShot->active);
            assert(game.bullets_.Pool().ActiveCount() == 2);
            assert(PickupCount(game) == pickups);
            assert(game.actorEffects_.Pool().ActiveCount() == 0);

            game.Update(0.1F);
            assert(EnemyCount(game) == 1);
            game.actors_.ForEachActive([](const behavior::Actor& enemy) {
                assert(std::abs(enemy.age-0.1F)<1e-6F && enemy.position.z < 5.0F);
            });
            assert(enemyShot->active && enemyShot->position.x > -4.0F);
            assert(playerShot->active && playerShot->position.x > -4.0F);
            assert(PickupCount(game) == pickups);
        }
    }

    /// @brief 从真实 Game 更新比较各遭遇的弹池轨迹，确保显示帧率不改变补发、延迟转向或空间位置。
    static void TestSpatialFrameRates(Game& game,const std::string& prefix) {
        // 跨重开只保留数值，不复制借用关卡 ANM 的动画游标，避免期望快照延长素材借用期。
        struct BulletSample {
            Vector3 position,velocity,acceleration,rotationAxis,launchDirection;
            float age;
            int nativeType,nativeColor;
            bool turnApplied;
        };
        std::size_t peak=0;
        for(int phase:{0,1,2,3,4}) {
            std::vector<BulletSample> expected;Vector3 expectedBoss{};float expectedTimer=0;
            for(int fps:{120,30,60,144,240}) {
                game.StartPreview();game.stage_=StageDirector{};game.BeginStageBoss(testing::BattleAt(*game.stageContent_,static_cast<std::size_t>(phase!=0)),3);
                while(game.boss_->attack<phase)game.boss_->AdvanceSkill();
                // 以同频率的原编排给出实际位移预算；包含原边界与插值终点补偿，不能用任意速度上限代替。
                behavior::Actor original;
                original.Reset(phase==0 ? Vector3{0,0,11.4285717F} : Vector3{0,0,5.71428585F},game.boss_->health,phase==0 ? 1882 : 5093);
                stage1::ConfigureBehavior(original,false);
                constexpr std::array<behavior::Entry,5> entries{bosses::RumiaBoss::Behaviors::Midboss,bosses::RumiaBoss::Behaviors::FirstNonspell,
                    bosses::RumiaBoss::Behaviors::NightBird,bosses::RumiaBoss::Behaviors::SecondNonspell,bosses::RumiaBoss::Behaviors::Demarcation};
                original.Start(entries[phase](original));auto referenceBullets=std::make_unique<BulletSystem>();
                double referenceTicks=0;
                for(int frame=0;frame<6*fps;++frame) {
                    referenceTicks+=static_cast<double>(1.0F/fps)*120;
                    float budget=0;
                    while(referenceTicks+0.00001>=1) {
                        referenceTicks-=1;const auto before=original.Position();
                        original.Update(1.0F/120,game.player_.Position(),*referenceBullets);
                        const auto after=original.Position();budget+=BulletSystem::Length({after.x-before.x,after.y-before.y,after.z-before.z});
                    }
                    const Vector3 before=game.boss_->position;game.Update(1.0F/fps);
                    const Vector3 after=game.boss_->position;
                    assert(BulletSystem::Length({after.x-before.x,after.y-before.y,after.z-before.z})<=budget+0.0001F);
                }
                std::vector<BulletSample> actual;
                game.bullets_.Pool().ForEachActive([&](const Bullet& bullet){
                    actual.push_back({bullet.position,bullet.velocity,bullet.acceleration,bullet.rotationAxis,bullet.launchDirection,
                                      bullet.age,bullet.nativeType,bullet.nativeColor,bullet.turnApplied});
                });
                assert(game.bullets_.Pool().DroppedSpawns()==0);peak=std::max(peak,actual.size());
                const auto equal=[](Vector3 a,Vector3 b) {
                    assert(std::abs(a.x-b.x)<1e-5F&&std::abs(a.y-b.y)<1e-5F&&std::abs(a.z-b.z)<1e-5F);
                };
                if(fps==120) {expected=actual;expectedBoss=game.boss_->position;expectedTimer=game.boss_->skillTimer;}
                else {
                    assert(expected.size()==actual.size());equal(expectedBoss,game.boss_->position);
                    assert(expectedTimer==game.boss_->skillTimer);
                    for(std::size_t i=0;i<actual.size();++i) {
                        equal(expected[i].position,actual[i].position);equal(expected[i].velocity,actual[i].velocity);
                        equal(expected[i].acceleration,actual[i].acceleration);equal(expected[i].rotationAxis,actual[i].rotationAxis);
                        equal(expected[i].launchDirection,actual[i].launchDirection);
                        assert(expected[i].age==actual[i].age&&expected[i].turnApplied==actual[i].turnApplied);
                        assert(expected[i].nativeColor==actual[i].nativeColor&&expected[i].nativeType==actual[i].nativeType);
                    }
                }
                if(fps==120) {
                    const auto name="spatial-phase-"+std::to_string(phase);Capture(game,prefix,name.c_str());
                }
            }
        }
        // 高 FPS 尚未到战斗步时保留边沿；暂停清输入且不追赶暂停的墙钟时间。
        game.StartPreview();game.stage_=StageDirector{};
        game.pendingPlayerInput_.mouseDelta={3,4};game.pendingPlayerInput_.bomb=true;
        game.Update(1.0F/240);assert(game.pendingPlayerInput_.bomb&&game.bombCount_==0);
        game.Update(1.0F/240);assert(!game.pendingPlayerInput_.bomb&&game.bombCount_==1);
        game.Update(1.0F/30);assert(game.bombCount_==1);
        const auto paused=game.stage_.Elapsed();game.TogglePause();game.Update(3);assert(game.stage_.Elapsed()==paused);
        game.TogglePause();game.Update(1.0F/120);assert(std::abs(game.stage_.Elapsed()-paused-1.0F/120)<1e-5F);
        std::cout<<"Spatial Game trajectories identical at 30/60/120/144/240 FPS; sampled peak "<<peak<<" bullets\n";
    }

    /// @brief 不跳过事件，按 60 Hz 推进一面到结算并验证正式切关；随后独立回归一面符卡练习和光束。
    static void TestWholeStage(Game& game, const std::string& prefix) {
        game.StartPreview();
        game.previewMode_ = false; // 本用例在临时目录验证标准模式成绩保存。
        bool midbossSeen = false, dialogueSeen = false;
        std::array<bool, 4> phases{};
        int frame = 0;std::size_t peakBullets=0;
        for (; frame < 400 * 60 && game.state_ != RunState::StageClear; ++frame) {
            game.Update(1.0F / 60.0F);
            assert(game.bullets_.Pool().DroppedSpawns()==0);
            peakBullets=std::max(peakBullets,game.bullets_.Pool().ActiveCount());
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
                if (game.bossEventId_==1) midbossSeen = true;
                else if (game.boss_->skillTimer > 5.1F && !phases[game.boss_->attack - 1]) {
                    phases[game.boss_->attack - 1] = true;
                    const auto name = "phase-" + std::to_string(game.boss_->attack);
                    Capture(game, prefix, name.c_str());
                }
            }
            assert(game.state_ != RunState::GameOver);
        }
        if (frame>=400*60) std::cerr << "Stage stalled: state " << static_cast<int>(game.state_)
            << " stage " << game.stage_.Elapsed() << " dialogue " << game.dialogueActive_
            << " after " << game.dialogueAfterBattle_ << " boss phase " << (game.boss_ ? game.boss_->attack : -1)
            << " hp " << (game.boss_ ? game.boss_->health : -1) << " timer " << (game.boss_ ? game.boss_->skillTimer : -1) << "\n";
        assert(frame < 400 * 60 && midbossSeen && dialogueSeen);
        for (bool seen : phases) assert(seen);
        assert(game.state_ == RunState::StageClear && game.bullets_.Pool().ActiveCount() == 0);
        assert(game.stageHighScores_.Best(false,1) >= 120000);
        Capture(game, prefix, "whole-stage-clear");
        std::cout << "Whole Stage 1 cleared at " << frame / 60.0F << " seconds; peak "<<peakBullets<<" / "<<BulletSystem::kCapacity<<" bullets\n";
        game.ContinueStageClear();
        assert(game.state_==RunState::Loading && game.stageId_==StageId::Stage2 && game.assets_.Stage().visuals.empty());
        // 单关回归在二面加载入口结束；连续六关另有 TestCampaignTransitions，符卡用例仍须显式回到一面。
        game.stageId_=StageId::Stage1;

        for (int phase : {2, 4}) {
            game.practiceMode_ = game.spellPractice_ = true;
            game.practiceAttack_ = phase;
            const int bestScore = game.bestScore_;
            game.StartRun(false, true);
            game.player_.SetInvulnerability(1000);
            assert(game.boss_->attack == phase && game.player_.Power() == Player::kMaxPower);
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
        game.bullets_.Pool().ForEachActive([](const Bullet& bullet) { assert(std::abs(bullet.age-0.5F)<1e-5F); });
        Capture(game, prefix, "pause");
        game.TogglePause();
        game.Update(0.6F);
        Capture(game, prefix, "beam-active");
        game.ApplyBombEffect();
        game.UpdatePlayerBomb(0);
        assert(game.bullets_.Pool().ActiveCount() == 0);
        game.StartPreview(); // 恢复整关模式，不能把练习设置泄漏给后续调用方。
        assert(!game.spellPractice_ && !game.practiceMode_ && !game.challengeMode_);
    }

    /// @brief 从二面实际开场运行两位自机的无攻击超时路线，覆盖五阶段、暂停、原始对白及资源卸载。
    static void TestStage2(Game& game,const std::string& prefix) {
        for(int character=0;character<2;++character) {
            game.stageId_=StageId::Stage2;game.selectedCharacter_=character;game.selectedShot_=0;
            game.StartPreview();
            bool midboss=false,before=false,after=false,pauseChecked=false,opening=false,nameplate=false;
            std::array<bool,5> phases{};std::size_t peak=0;int frame=0;
            for(;frame<420*60&&game.state_!=RunState::Won;++frame) {
                game.Update(1.0F/60);assert(game.state_!=RunState::GameOver);
                assert(game.bullets_.Pool().DroppedSpawns()==0&&game.actors_.DroppedSpawns()==0);
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(!opening&&frame>=12*60) {opening=true;Capture(game,prefix,"stage2-opening");}
                if(game.dialogueActive_&&game.dialogueTimer_>1) {
                    bool& seen=game.dialogueAfterBattle_ ? after : before;
                    if(!seen) {
                        seen=true;const auto name="stage2-dialogue-"+std::to_string(character)+(game.dialogueAfterBattle_?"-after":"-before");
                        Capture(game,prefix,name.c_str());
                    }
                    float lineStart=0;const int line=game.DialogueIndex(&lineStart);
                    if(!game.dialogueAfterBattle_&&!nameplate&&game.CurrentDialogue()[line].introduce&&
                       game.dialogueTimer_-lineStart>0.75F&&GetScreenWidth()>500) {
                        nameplate=true;
                        Capture(game,prefix,"stage2-nameplate");
                        const int width=GetScreenWidth(),height=GetScreenHeight();SetWindowSize(390,844);
                        Capture(game,prefix,"stage2-dialogue-portrait");SetWindowSize(width,height);
                    }
                }
                if(game.boss_&&game.boss_->active) {
                    if(game.bossEventId_==1)midboss=true;
                    else if(game.boss_->attack>=1&&game.boss_->attack<=5&&game.boss_->skillTimer>5.1F) {
                        const int phase=game.boss_->attack;
                        if(!phases[phase-1]) {
                            phases[phase-1]=true;const auto name="stage2-phase-"+std::to_string(phase);
                            Capture(game,prefix,name.c_str());
                        }
                        if(phase==4&&!pauseChecked) {
                            pauseChecked=true;const auto time=game.stage_.Elapsed(),timer=game.boss_->skillTimer;
                            game.TogglePause();game.Update(0.5F);
                            assert(game.stage_.Elapsed()==time&&game.boss_->skillTimer==timer);
                            game.TogglePause();
                        }
                    }
                }
            }
            if(frame>=420*60)std::cerr<<"Stage 2 stalled at "<<game.stage_.Elapsed()<<" phase "<<(game.boss_?game.boss_->attack:-1)<<'\n';
            assert(frame<420*60&&midboss&&before&&after&&pauseChecked);
            for(bool seen:phases)assert(seen);
            assert(game.state_==RunState::Won&&game.assets_.Stage().visuals.empty());
            assert(game.bullets_.Pool().ActiveCount()==0&&EnemyCount(game)==0&&game.actorEffects_.Pool().ActiveCount()==0);
            std::cout<<"Whole Stage 2 character "<<character<<" cleared; peak "<<peak<<" projectiles\n";
        }
        // 击破路线独立于超时：真实总控必须只生成一枚 Bomb，并将正 HP 符卡阈值视为取得。
        game.stageId_=StageId::Stage2;game.selectedCharacter_=game.selectedShot_=0;
        game.StartPreview();game.previewMode_=false;game.stage_=StageDirector{};
        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,0),100);
        for(int tick=0;tick<130;++tick)game.Update(1.0F/120);
        assert(game.boss_->CanTakeDamage());game.boss_->health=0;game.ResolveCollisions();
        for(int tick=0;tick<5&&game.boss_->active;++tick)game.Update(1.0F/120);
        assert(!game.boss_->active&&game.boss_->Defeated());
        int bombItems=0;game.drops_.ForEachActive([&](const Pickup& pickup){bombItems+=pickup.kind==PickupKind::Bomb;});
        assert(bombItems==1);Capture(game,prefix,"stage2-bomb-drop");
        game.drops_.MagnetAllItems();
        for(int tick=0;tick<240;++tick)game.UpdatePickups(1.0F/120);
        assert(game.player_.Bombs()==4);
        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),101);
        for(int phase=1;phase<=5;++phase) {
            int ticks=0;
            while(!game.boss_->CanTakeDamage()&&ticks++<600)game.Update(1.0F/120);
            assert(ticks<600&&game.boss_->attack==phase);
            const auto scoreBefore=game.score_;
            game.boss_->health=phase==4 ? 1399 : 0;game.ResolveCollisions();
            if(phase==4)assert(game.score_>scoreBefore&&game.spellBonus_>0&&game.boss_->attack==5);
        }
        for(int tick=0;tick<180&&game.boss_->active;++tick)game.Update(1.0F/120);
        assert(!game.boss_->active&&game.boss_->Defeated());
        for(int number:{11,15,18})assert(game.spellRecords_.Seen(0,number));
        // 跨关切换必须清掉所有借用二面档案的对象，再重新加载一面。
        game.stageId_=StageId::Stage1;game.selectedCharacter_=game.selectedShot_=0;
        game.StartPreview();game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty());
    }

    /// @brief 三面真实整关覆盖道中等待/对白、入场后开战、六阶段和子演员；击破路线另覆盖道中符卡。
    static void TestStage3(Game& game,const std::string& prefix) {
        for(int character=0;character<2;++character) {
            game.stageId_=StageId::Stage3;game.selectedCharacter_=character;game.selectedShot_=0;game.StartPreview();
            bool mid=false,midDialogue=false,before=false,after=false,nameplate=false,children=false,paused=false;
            std::array<bool,6> phases{};std::size_t peak=0;int frame=0;
            for(;frame<550*60&&game.state_!=RunState::Won;++frame) {
                game.Update(1.0F/60);assert(game.state_!=RunState::GameOver);
                assert(game.bullets_.Pool().DroppedSpawns()==0&&game.actors_.DroppedSpawns()==0);
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(game.dialogueActive_&&game.dialogueTimer_>1) {
                    const bool middle=game.dialogueScene_==DialogueScene::Midboss;
                    bool& seen=middle ? midDialogue : game.dialogueAfterBattle_ ? after : before;
                    if(!seen) {
                        seen=true;const auto name="stage3-dialogue-"+std::to_string(character)+(middle ? "-mid" : game.dialogueAfterBattle_ ? "-after" : "-before");
                        Capture(game,prefix,name.c_str());
                    }
                    if(middle)assert(!game.boss_->active&&game.state_==RunState::Playing);
                    else if(!game.dialogueAfterBattle_)assert(game.boss_->attack==0&&!game.boss_->CanTakeDamage());
                    float start=0;const int line=game.DialogueIndex(&start);
                    if(!middle&&!game.dialogueAfterBattle_&&!nameplate&&game.CurrentDialogue()[line].introduce&&game.dialogueTimer_-start>0.75F) {
                        nameplate=true;Capture(game,prefix,"stage3-nameplate");
                        const int w=GetScreenWidth(),h=GetScreenHeight();SetWindowSize(390,844);Capture(game,prefix,"stage3-portrait");SetWindowSize(w,h);
                    }
                }
                if(game.boss_&&game.boss_->active) {
                    if(game.bossEventId_==1)mid=true;
                    else if(game.boss_->attack>0&&game.boss_->attack<=6&&game.boss_->skillTimer>5.1F) {
                        const int phase=game.boss_->attack;
                        if(!phases[phase-1]) {phases[phase-1]=true;const auto name="stage3-phase-"+std::to_string(phase);Capture(game,prefix,name.c_str());}
                        if(phase==3&&EnemyCount(game)>0)children=true;
                        if(phase==5&&!paused) {
                            paused=true;const auto time=game.stage_.Elapsed(),timer=game.boss_->skillTimer;
                            game.TogglePause();game.Update(0.5F);assert(game.stage_.Elapsed()==time&&game.boss_->skillTimer==timer);game.TogglePause();
                        }
                    }
                }
            }
            if(frame>=550*60)std::cerr<<"Stage 3 stalled at "<<game.stage_.Elapsed()<<" phase "<<(game.boss_?game.boss_->attack:-1)<<'\n';
            assert(frame<550*60&&mid&&midDialogue&&before&&after&&nameplate&&children&&paused);for(bool seen:phases)assert(seen);
            assert(game.state_==RunState::Won&&game.assets_.Stage().visuals.empty()&&EnemyCount(game)==0&&game.bullets_.Pool().ActiveCount()==0);
            std::cout<<"Whole Stage 3 character "<<character<<" cleared; peak "<<peak<<" projectiles\n";
        }
        game.stageId_=StageId::Stage3;game.selectedCharacter_=game.selectedShot_=0;game.StartPreview();
        game.previewMode_=false;game.stage_=StageDirector{};game.BeginStageBoss(testing::BattleAt(*game.stageContent_,0),100);
        for(int tick=0;tick<210;++tick)game.Update(1.0F/120);assert(game.boss_->CanTakeDamage());
        game.boss_->health=1299;game.ResolveCollisions();assert(game.boss_->attack==2&&!game.boss_->CanTakeDamage());
        for(int tick=0;tick<245;++tick)game.Update(1.0F/120);assert(game.boss_->CanTakeDamage());
        Capture(game,prefix,"stage3-midboss-spell");game.boss_->health=0;game.ResolveCollisions();
        for(int tick=0;tick<370&&game.boss_->active;++tick)game.Update(1.0F/120);
        assert(!game.boss_->active&&game.boss_->Defeated()&&game.spellRecords_.Seen(0,22));
        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),101);game.ResumeStageBoss();
        for(int phase=1;phase<=6;++phase) {
            int ticks=0;while(!game.boss_->CanTakeDamage()&&ticks++<700)game.Update(1.0F/120);
            assert(ticks<700&&game.boss_->attack==phase);
            if(phase==3) {
                for(int tick=0;tick<450;++tick)game.Update(1.0F/120);
                assert(EnemyCount(game)>=6);Capture(game,prefix,"stage3-summons");
            }
            const auto score=game.score_;game.boss_->health=phase==5 ? 1999 : 0;game.ResolveCollisions();
            if(phase==5)assert(game.score_>score&&game.spellBonus_>0&&game.boss_->attack==6);
        }
        for(int tick=0;tick<180&&game.boss_->active;++tick)game.Update(1.0F/120);
        assert(!game.boss_->active&&game.boss_->Defeated());for(int card:{26,32,35})assert(game.spellRecords_.Seen(0,card));
        game.stageId_=StageId::Stage2;game.StartPreview();game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty());
    }

    /// @brief 新关卡可保留对白开始前的场上对象；默认一面策略仍清除演员/敌弹并保留自机弹。
    static void TestDialogueClearPolicy(Game& game) {
        game.stageId_=StageId::Stage1;game.StartPreview();game.stage_=StageDirector{};
        auto spawn=testing::SpawnAt({1,2,5},stage1::kFairy,100,0.4F);auto* actor=game.actors_.Spawn(spawn);
        auto* enemy=game.bullets_.Spawn(BulletOwner::Enemy,{1,2,3},{},0.1F,WHITE,stage1::kBlueBullet);
        auto* player=game.bullets_.Spawn(BulletOwner::Player,{1,2,3},{},0.1F,WHITE,stage1::kPlayerGoldCard);
        assert(actor&&enemy&&player);game.BeginStageDialogue(1,false,DialogueScene::FinalBoss,DialogueClearPolicy::PreserveWorld);
        assert(actor->active&&enemy->active&&player->active);
        game.BeginStageDialogue(2);assert(!actor->active&&!enemy->active&&player->active);game.EnterMainMenu();
    }

    /// @brief 时停冻结玩家、弹幕、道具和背景，敌人脚本/移动继续；既有持续伤害和 Bomb 区域仍可接触敌人。
    static void TestWorldTimeClock(Game& game,const std::string& prefix) {
        struct ClockBoss final:Boss {
            bool stopped=true;
            ClockBoss() {active=true;phase=1;health=maxHealth=1000;visual=stage1::kRumia;position={0,0,4};}
            bool Update(float seconds,Vector3,BulletSystem&) override {skillTimer+=seconds;return false;}
            bool AdvanceSkill() override {return true;}
            const char* SpellName() const override {return nullptr;}
            const char* DisplayName() const override {return "Clock probe";}
            bool ControlsWorldTime() const override {return true;}
            bool WorldTimeStopped() const override {return stopped;}
            bool CanContactPlayer() const override {return false;}
        };
        game.stageId_=StageId::Stage1;game.selectedCharacter_=game.selectedShot_=0;game.StartPreview();game.stage_=StageDirector{};
        game.boss_=std::make_unique<ClockBoss>();auto* clock=static_cast<ClockBoss*>(game.boss_.get());
        game.player_.SetInvulnerability(0);game.debugInvincible_=false;
        const auto playerPosition=game.player_.Position();const auto visualAge=game.player_.VisualAge();
        auto* contact=game.bullets_.Spawn(BulletOwner::Enemy,playerPosition,{},0.1F,WHITE,stage1::kBlueBullet);
        auto* enemy=game.bullets_.Spawn(BulletOwner::Enemy,{1,2,3},{1,0,0},0.1F,WHITE,stage1::kBlueBullet);
        auto* laser=game.bullets_.Spawn(BulletOwner::Player,clock->position,{},0.1F,WHITE,stage1::kPlayerGoldCard);
        laser->playerShotKind=PlayerShotKind::Laser;laser->damage=0;laser->damagePerSecond=60;laser->age=0.3F;
        auto spawn=testing::SpawnAt({3,1,5},stage1::kFairy,1000,0.4F);spawn.pattern.movement={.angle=0,.speed=1};
        auto* actor=game.actors_.Spawn(spawn);assert(actor);game.drops_.SpawnNativeItem({2,3,5},0,false);
        PlayerInput input;input.moveRight=true;input.fire=true;input.mouseDelta={4,3};
        for(int tick=0;tick<60;++tick)game.UpdateBattleStep(1.0F/120,input,false,false);
        assert(game.player_.Position().x==playerPosition.x&&game.player_.VisualAge()==visualAge&&game.missCount_==0&&game.graze_==0);
        assert(enemy->position.x==1&&enemy->age==0&&laser->age==0.3F&&contact->active&&!contact->grazed);
        assert(actor->position.x>3.4F&&actor->phaseFrames==0&&actor->animationAge>0.49);
        game.drops_.ForEachActive([](const Pickup& item){assert(item.age==0);});
        if(std::abs(clock->health-976)>=0.01F||game.scenerySeconds_!=0||game.stage_.Elapsed()<=0.49F)
            std::cerr<<"World clock witness health="<<clock->health<<" scenery="<<game.scenerySeconds_<<" timeline="<<game.stage_.Elapsed()<<'\n';
        assert(std::abs(clock->health-976)<0.01F&&game.scenerySeconds_==0&&game.stage_.Elapsed()>0.49F);
        Capture(game,prefix,"world-time-stopped");
        contact->active=false;clock->stopped=false;game.UpdateBattleStep(1.0F/120,input,false,false);
        assert(game.player_.Position().x!=playerPosition.x&&enemy->position.x>1&&game.scenerySeconds_>0&&actor->phaseFrames>0);
        game.bullets_.Reset();game.actors_.Clear();game.drops_.Clear();clock->health=clock->maxHealth=1000000;
        game.player_.ConfigureRun(3,3,false,ShotType::Needle);game.ApplyBombEffect();
        for(int tick=0;tick<300&&game.playerBomb_.Attacks().empty();++tick)game.UpdateBattleStep(1.0F/120,{},false,false);
        assert(!game.playerBomb_.Attacks().empty());clock->position=game.playerBomb_.Attacks().front().volume.origin;
        int contactDamage=0;for(int damage:game.playerBomb_.ContactDamage(clock->position,clock->hitRadius))contactDamage+=damage;assert(contactDamage>0);
        clock->stopped=true;const int bombFrame=game.playerBomb_.Frame();const float health=clock->health;
        auto* frozen=game.bullets_.Spawn(BulletOwner::Enemy,clock->position,{},0.1F,WHITE,stage1::kBlueBullet);
        for(int tick=0;tick<60;++tick)game.UpdateBattleStep(1.0F/120,{},false,false);
        assert(game.playerBomb_.Frame()==bombFrame&&clock->health<health&&frozen->active&&frozen->age==0);
        const auto elapsed=game.stage_.Elapsed();game.TogglePause();game.Update(0.5F);assert(game.stage_.Elapsed()==elapsed);game.TogglePause();
        frozen->active=false;clock->stopped=false;for(int tick=0;tick<2;++tick)game.UpdateBattleStep(1.0F/120,{},false,false);
        assert(game.playerBomb_.Frame()>bombFrame);game.EnterMainMenu();assert(!game.WorldTimeStopped()&&game.assets_.Stage().visuals.empty());
    }

    /// @brief 四面两角色整关超时路线及四机体击破路线；覆盖延迟符卡宣告、召唤、激光、暂停与跨关释放。
    static void TestStage4(Game& game,const std::string& prefix) {
        for(int character=0;character<2;++character) {
            game.stageId_=StageId::Stage4;game.selectedCharacter_=character;game.selectedShot_=0;game.StartPreview();
            bool mid=false,before=false,after=false,nameplate=false,children=false,paused=false,beams=false;
            std::array<bool,6> phases{};std::size_t peak=0;int frame=0;
            for(;frame<700*60&&game.state_!=RunState::Won;++frame) {
                game.Update(1.0F/60);assert(game.state_!=RunState::GameOver);
                assert(game.bullets_.Pool().DroppedSpawns()==0&&game.actors_.DroppedSpawns()==0);
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                game.bullets_.Pool().ForEachActive([&](const Bullet& bullet){beams|=bullet.owner==BulletOwner::Enemy&&bullet.IsBeam();});
                if(game.dialogueActive_&&game.dialogueTimer_>1) {
                    bool& seen=game.dialogueAfterBattle_ ? after : before;
                    if(!seen) {seen=true;const auto name="stage4-dialogue-"+std::to_string(character)+(game.dialogueAfterBattle_ ? "-after" : "-before");Capture(game,prefix,name.c_str());}
                    if(!game.dialogueAfterBattle_)assert(game.boss_->attack==0&&!game.boss_->CanTakeDamage());
                    float start=0;const int line=game.DialogueIndex(&start);
                    if(!game.dialogueAfterBattle_&&!nameplate&&game.CurrentDialogue()[line].introduce&&game.dialogueTimer_-start>0.75F) {
                        nameplate=true;Capture(game,prefix,"stage4-nameplate");const int w=GetScreenWidth(),h=GetScreenHeight();
                        SetWindowSize(390,844);Capture(game,prefix,"stage4-portrait");SetWindowSize(w,h);
                    }
                }
                if(game.boss_&&game.boss_->active) {
                    if(game.bossEventId_==1) {mid=true;children|=EnemyCount(game)>0;}
                    else if(game.boss_->attack>0&&game.boss_->attack<=6&&game.boss_->skillTimer>5.1F) {
                        const int phase=game.boss_->attack;
                        if(!phases[phase-1]) {phases[phase-1]=true;const auto name="stage4-character-"+std::to_string(character)+"-phase-"+std::to_string(phase);Capture(game,prefix,name.c_str());}
                        if(phase==5&&!paused) {
                            paused=true;const auto time=game.stage_.Elapsed(),timer=game.boss_->skillTimer;
                            game.TogglePause();game.Update(0.5F);assert(game.stage_.Elapsed()==time&&game.boss_->skillTimer==timer);game.TogglePause();
                        }
                    }
                }
            }
            if(frame>=700*60)std::cerr<<"Stage 4 stalled at "<<game.stage_.Elapsed()<<" phase "<<(game.boss_?game.boss_->attack:-1)<<'\n';
            assert(frame<700*60&&mid&&before&&after&&nameplate&&children&&paused&&beams);for(bool seen:phases)assert(seen);
            assert(game.state_==RunState::Won&&game.assets_.Stage().visuals.empty()&&EnemyCount(game)==0&&game.bullets_.Pool().ActiveCount()==0);
            std::cout<<"Whole Stage 4 character "<<character<<" cleared; peak "<<peak<<" projectiles\n";
        }
        constexpr std::array<std::array<int,4>,4> cards{{{39,58,67,78},{46,51,75,78},{50,40,71,82},{57,63,82,75}}};
        for(int shot=0;shot<4;++shot) {
            game.stageId_=StageId::Stage4;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;game.StartPreview();
            game.previewMode_=false;game.stage_=StageDirector{};game.BeginStageBoss(testing::BattleAt(*game.stageContent_,0),100);
            for(int tick=0;tick<65;++tick)game.Update(1.0F/120);assert(game.boss_->CanTakeDamage());
            game.boss_->health=0;game.ResolveCollisions();
            for(int tick=0;tick<10&&game.boss_->active;++tick)game.Update(1.0F/120);
            assert(!game.boss_->active&&game.boss_->Defeated());
            game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),101);game.ResumeStageBoss();
            for(int phase=1;phase<=6;++phase) {
                int ticks=0;bool announced=false;
                while(!game.boss_->CanTakeDamage()&&ticks++<700) {
                    game.Update(1.0F/120);
                    if(phase>=5&&!announced&&game.boss_->SpellCardNumber()>0) {announced=true;assert(game.spellAttemptOpen_&&game.spellCutInTimer_>1.9F);}
                }
                assert(ticks<700&&game.boss_->attack==phase);
                for(int tick=0;tick<260;++tick)game.Update(1.0F/120);
                const auto name="stage4-shot-"+std::to_string(shot)+"-phase-"+std::to_string(phase);Capture(game,prefix,name.c_str());
                if(phase==5||phase==6)assert(game.spellAttemptOpen_&&announced);
                const auto score=game.score_;game.boss_->health=phase==5 ? 1699 : 0;game.ResolveCollisions();
                if(phase==5)assert(game.score_>score&&game.boss_->attack==6&&!game.boss_->CanTakeDamage());
            }
            for(int tick=0;tick<150&&game.boss_->active;++tick)game.Update(1.0F/120);
            assert(!game.boss_->active&&game.boss_->Defeated());for(int card:cards[shot])assert(game.spellRecords_.Seen(shot,card));
        }
        game.stageId_=StageId::Stage3;game.StartPreview();game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty());
    }


    /// @brief 五面两角色整关超时与四机体击破，覆盖道中对白、时停冻结/恢复、召唤体和完整资源释放。
    static void TestStage5(Game& game,const std::string& prefix) {
        for(int character=0;character<2;++character) {
            game.stageId_=StageId::Stage5;game.selectedCharacter_=character;game.selectedShot_=0;game.StartPreview();
            bool mid=false,midDialogue=false,before=false,after=false,children=false,paused=false;
            std::array<bool,6> phases{};std::array<bool,3> stops{};std::size_t peak=0;int frame=0;
            for(;frame<1100*60&&game.state_!=RunState::Won;++frame) {
                game.Update(1.0F/60);assert(game.state_!=RunState::GameOver);
                assert(game.bullets_.Pool().DroppedSpawns()==0&&game.actors_.DroppedSpawns()==0);
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(game.dialogueActive_&&game.dialogueTimer_>1) {
                    bool& seen=game.dialogueScene_==DialogueScene::Midboss ? midDialogue : game.dialogueAfterBattle_ ? after : before;
                    if(!seen) {seen=true;const auto name="stage5-dialogue-"+std::to_string(character)+(game.dialogueScene_==DialogueScene::Midboss ? "-mid" : game.dialogueAfterBattle_ ? "-after" : "-before");Capture(game,prefix,name.c_str());}
                    if(!game.dialogueAfterBattle_)assert(game.boss_->attack==0&&!game.boss_->CanTakeDamage());
                }
                if(game.boss_&&game.boss_->active) {
                    if(game.bossEventId_==1)mid=true;
                    else if(game.boss_->attack>0&&game.boss_->attack<=6) {
                        const int phase=game.boss_->attack;children|=EnemyCount(game)>0;
                        if(game.boss_->skillTimer>5.1F&&!phases[phase-1]) {
                            phases[phase-1]=true;const auto name="stage5-character-"+std::to_string(character)+"-phase-"+std::to_string(phase);Capture(game,prefix,name.c_str());
                        }
                        if(game.WorldTimeStopped()) {
                            assert(!game.boss_->CanBeTargeted()&&!game.boss_->CanTakeDamage());
                            const int group=phase/2-1;assert(group>=0&&group<3);
                            if(!stops[group]) {stops[group]=true;const auto name="stage5-time-stop-"+std::to_string(group);Capture(game,prefix,name.c_str());}
                            if(!paused) {
                                paused=true;const auto time=game.stage_.Elapsed(),timer=game.boss_->skillTimer;
                                game.TogglePause();game.Update(0.5F);assert(game.stage_.Elapsed()==time&&game.boss_->skillTimer==timer);game.TogglePause();
                            }
                        }
                    }
                }
            }
            if(frame>=1100*60)std::cerr<<"Stage 5 stalled at "<<game.stage_.Elapsed()<<" phase "<<(game.boss_?game.boss_->attack:-1)<<'\n';
            assert(frame<1100*60&&mid&&midDialogue&&before&&after&&children&&paused);
            for(bool seen:phases)assert(seen);for(bool stopped:stops)assert(stopped);
            assert(game.state_==RunState::Won&&game.assets_.Stage().visuals.empty()&&EnemyCount(game)==0&&game.bullets_.Pool().ActiveCount()==0);
            std::cout<<"Whole Stage 5 character "<<character<<" cleared; peak "<<peak<<" projectiles\n";
        }
        for(int shot=0;shot<4;++shot) {
            game.stageId_=StageId::Stage5;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;game.StartPreview();
            game.previewMode_=false;game.stage_=StageDirector{};game.BeginStageBoss(testing::BattleAt(*game.stageContent_,0),100);game.ResumeStageBoss();
            for(int phase=1;phase<=2;++phase) {
                int ticks=0;while(!game.boss_->CanTakeDamage()&&ticks++<700)game.Update(1.0F/120);
                assert(ticks<700&&game.boss_->attack==phase);game.boss_->health=0;game.ResolveCollisions();
            }
            for(int tick=0;tick<400&&game.boss_->active;++tick)game.Update(1.0F/120);
            assert(!game.boss_->active&&game.boss_->Defeated());game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),101);game.ResumeStageBoss();
            for(int phase=1;phase<=6;++phase) {
                int ticks=0;while(!game.boss_->CanTakeDamage()&&ticks++<700)game.Update(1.0F/120);
                assert(ticks<700&&game.boss_->attack==phase);game.boss_->health=0;game.ResolveCollisions();
            }
            for(int tick=0;tick<150&&game.boss_->active;++tick)game.Update(1.0F/120);
            assert(!game.boss_->active&&game.boss_->Defeated());for(int card:{86,90,94,98})assert(game.spellRecords_.Seen(shot,card));
        }
        game.stageId_=StageId::Stage4;game.StartPreview();game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty());
    }


    /// @brief 六面两角色整关超时，验证对白中入场/首次恢复/结束后开战、九阶段、静默收尾和资源释放。
    static void TestStage6(Game& game,const std::string& prefix) {
        for(int character=0;character<2;++character) {
            game.stageId_=StageId::Stage6;game.selectedCharacter_=character;game.selectedShot_=0;game.StartPreview();
            bool mid=false,midDialogue=false,before=false,after=false,signalBirth=false,prelude=false,children=false,beams=false,paused=false;
            std::array<bool,9> phases{};std::size_t peak=0;int frame=0;
            for(;frame<1300*60&&game.state_!=RunState::Won;++frame) {
                game.Update(1.0F/60);assert(game.state_!=RunState::GameOver);
                if(game.bullets_.Pool().DroppedSpawns()||game.actors_.DroppedSpawns())
                    std::cerr<<"Stage 6 dropped at "<<frame<<" stage "<<game.stage_.Elapsed()<<" phase "<<(game.boss_?game.boss_->attack:-1)
                        <<" timer "<<(game.boss_?game.boss_->skillTimer:-1)<<" actors "<<EnemyCount(game)<<" bullets "<<game.bullets_.Pool().ActiveCount()
                        <<" actor drops "<<game.actors_.DroppedSpawns()<<" bullet drops "<<game.bullets_.Pool().DroppedSpawns()<<'\n';
                assert(game.bullets_.Pool().DroppedSpawns()==0&&game.actors_.DroppedSpawns()==0);peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                game.bullets_.Pool().ForEachActive([&](const Bullet& b){beams|=b.owner==BulletOwner::Enemy&&b.IsBeam();});
                if(game.dialogueActive_) {
                    if(game.dialogueScene_==DialogueScene::Midboss)midDialogue=true;
                    else if(game.dialogueAfterBattle_) {
                        assert(game.CurrentDialogue().empty());assert(std::abs(game.DialogueDuration()-1)<1e-5F);if(!after)Capture(game,prefix,"stage6-silent-ending");after=true;
                    } else {
                        if(!before&&game.dialogueTimer_>1) {before=true;const auto name="stage6-dialogue-"+std::to_string(character);Capture(game,prefix,name.c_str());}
                        if(game.boss_&&game.boss_->active&&!(game.bossEventId_==1)) {
                            signalBirth=true;assert(game.dialogueSignalsSeen_>=1&&game.boss_->attack==0&&!game.boss_->CanTakeDamage());
                            if(game.dialogueSignalsSeen_>=2)prelude=true;
                        }
                    }
                }
                if(game.boss_&&game.boss_->active) {
                    if(game.bossEventId_==1)mid=true;
                    else if(game.boss_->attack>0&&game.boss_->attack<=9) {
                        const int phase=game.boss_->attack;children|=EnemyCount(game)>0;
                        if(game.boss_->skillTimer>5.1F&&!phases[phase-1]) {
                            phases[phase-1]=true;const auto name="stage6-character-"+std::to_string(character)+"-phase-"+std::to_string(phase);Capture(game,prefix,name.c_str());
                        }
                        if(phase==9&&!paused) {
                            paused=true;const auto time=game.stage_.Elapsed(),timer=game.boss_->skillTimer;
                            game.TogglePause();game.Update(0.5F);assert(game.stage_.Elapsed()==time&&game.boss_->skillTimer==timer);game.TogglePause();
                        }
                    }
                }
            }
            if(frame>=1300*60)std::cerr<<"Stage 6 stalled at "<<game.stage_.Elapsed()<<" phase "<<(game.boss_?game.boss_->attack:-1)<<'\n';
            assert(frame<1300*60&&mid&&midDialogue&&before&&after&&signalBirth&&prelude&&children&&beams&&paused);for(bool seen:phases)assert(seen);
            assert(game.state_==RunState::Won&&game.assets_.Stage().visuals.empty()&&EnemyCount(game)==0&&game.bullets_.Pool().ActiveCount()==0);
            std::cout<<"Whole Stage 6 character "<<character<<" cleared; peak "<<peak<<" projectiles\n";
        }
        for(int shot=0;shot<4;++shot) {
            game.stageId_=StageId::Stage6;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;game.StartPreview();game.previewMode_=false;
            game.stage_=StageDirector{};game.BeginStageBoss(testing::BattleAt(*game.stageContent_,0),100);game.ResumeStageBoss();
            for(int phase=1;phase<=2;++phase) {
                int ticks=0;while(!game.boss_->CanTakeDamage()&&ticks++<700)game.Update(1.0F/120);
                assert(ticks<700&&game.boss_->attack==phase);game.boss_->health=0;game.ResolveCollisions();
            }
            for(int tick=0;tick<10&&game.boss_->active;++tick)game.Update(1.0F/120);
            assert(!game.boss_->active&&game.boss_->Defeated());game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),101);game.ResumeStageBoss();game.Update(1.0F/60);game.ResumeStageBoss();
            for(int phase=1;phase<=9;++phase) {
                int ticks=0;while(!game.boss_->CanTakeDamage()&&ticks++<700)game.Update(1.0F/120);
                assert(ticks<700&&game.boss_->attack==phase);
                if(phase==9) {
                    const float health=game.boss_->health;game.UpdateBattleStep(1.0F/120,PlayerInput{.bomb=true},false,false);
                    assert(game.playerBomb_.Active()&&!game.boss_->CanBeTargeted()&&!game.boss_->CanTakeDamage()&&game.boss_->health==health);
                    const auto name="stage6-bomb-bat-"+std::to_string(shot);Capture(game,prefix,name.c_str());
                    int recovery=0;while(!game.boss_->CanTakeDamage()&&recovery++<2400)game.Update(1.0F/120);
                    assert(recovery<2400&&!game.playerBomb_.Active()&&game.boss_->health==health);
                }
                game.boss_->health=0;game.ResolveCollisions();
            }
            for(int tick=0;tick<150&&game.boss_->active;++tick)game.Update(1.0F/120);
            assert(!game.boss_->active&&game.boss_->Defeated());for(int card:{101,104,107,110,113,116})assert(game.spellRecords_.Seen(shot,card));
        }
        game.stageId_=StageId::Stage5;game.StartPreview();game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty());
    }


    /// @brief 逐一通过正式结算/异步加载切换所有已登记关卡，保留本局资源并清除旧关借用者；练习与预览独立结束。
    static void TestCampaignTransitions(Game& game,const std::string& prefix) {
        game.stageId_=StageId::Stage1;game.selectedCharacter_=1;game.selectedShot_=1;game.StartPreview();game.previewMode_=false;
        game.player_.ConfigureRun(4,2,false,ShotType::IllusionLaser);game.player_.AdjustPower(97-game.player_.Power());
        game.score_=12345;game.graze_=75;game.pointItems_=25;game.missCount_=2;game.bombCount_=1;game.referenceRank_=23;
        const auto lives=game.player_.RemainingLives(),bombs=game.player_.Bombs(),power=game.player_.Power();
        int visited=0;
        for(const auto stage:kPracticeStages) {
            if(!stage)break;++visited;assert(game.stageId_==*stage&&game.state_==RunState::Playing);
            game.stage_=StageDirector{};game.referenceSubrank_=59;game.rankTime_=3;game.stageGraze_=11;game.stagePointItems_=7;
            auto* actor=game.actors_.Spawn(testing::SpawnAt({0,0,4},stage1::kFairy,100,0.4F));assert(actor);
            game.UpdateActor(*actor,1.0F/120);
            assert(game.actorEffects_.Spawn(actor->visual,actor->position,actor->hitRadius,actor->presentation,ActorExitReason::Retreat,&actor->animation.player.Sample()));
            game.player_.SetView(0.8F,0.3F);const auto before=game.score_;game.BeginStageClear();
            const auto bonus=game.stageClearSummary_.Total();assert(game.score_==before+bonus&&game.stageClearSummary_.graze==11&&game.stageClearSummary_.pointItems==7);
            const auto next=NextStage(*stage);game.ContinueStageClear();
            if(!next) {
                assert(game.state_==RunState::Finishing);for(int tick=0;tick<200&&game.state_==RunState::Finishing;++tick)game.Update(1.0F/60);
                assert(game.state_==RunState::Loading && game.pendingEnding_==ending::Id::MarisaB && game.assets_.Stage().visuals.empty());
                assert(game.clearedMainlineStages_==0x3f);break;
            }
            assert(game.state_==RunState::Loading&&game.stageId_==*next&&game.assets_.Stage().visuals.empty());
            assert(EnemyCount(game)==0&&game.actorEffects_.Pool().ActiveCount()==0&&game.bullets_.Pool().ActiveCount()==0&&PickupCount(game)==0);
            const double deadline=GetTime()+60;
            while(game.state_==RunState::Loading) {
                assert(GetTime()<deadline&&!game.loadingFailed_);game.Update(1.0F/60);
                BeginDrawing();ClearBackground(BLACK);game.Draw();EndDrawing();
            }
            assert(game.state_==RunState::Playing&&game.stage_.Elapsed()==0);
            assert(game.score_==before+bonus&&game.graze_==75&&game.pointItems_==25&&game.missCount_==2&&game.bombCount_==1);
            assert(game.stageGraze_==0&&game.stagePointItems_==0&&game.referenceRank_==23&&game.referenceSubrank_==0&&game.rankTime_==0);
            assert(game.player_.RemainingLives()==lives&&game.player_.Bombs()==bombs&&game.player_.Power()==power&&game.player_.Shot()==ShotType::IllusionLaser);
            assert(game.player_.Position().x==0&&game.player_.Position().y==0&&game.player_.Position().z==-6.5F);
            const auto name="campaign-stage-"+std::to_string(StageNumber(*next));Capture(game,prefix,name.c_str());
        }
        assert(visited==6);game.stageId_=StageId::Stage1;game.StartPreview();game.BeginStageClear();game.ContinueStageClear();
        assert(game.state_==RunState::Finishing&&game.stageId_==StageId::Stage1);
        game.StartPreview();game.previewMode_=false;game.practiceMode_=true;game.BeginStageClear();game.ContinueStageClear();
        assert(game.state_==RunState::Finishing&&game.stageId_==StageId::Stage1);game.EnterMainMenu();
    }

    /// @brief 通过正式开局绑定检查两种御札，并渲染真实追踪弹和侧针，防止生成正常而素材/动画不可见。
    static void TestPlayerShots(Game& game, const std::string& prefix) {
        const int previousShot=game.selectedShot_;
        for(int shot : {0,1})for(int power : {Player::kMinPower,16,128}) {
            game.selectedShot_=shot;
            game.StartPreview();
            game.stage_=StageDirector{};
            game.player_.AdjustPower(power - game.player_.Power());
            const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
            const auto render=[&] {
                BeginDrawing(); BeginTextureMode(target); ClearBackground(BLACK);
                game.DrawWorld(); EndTextureMode(); EndDrawing();
                Image image=LoadImageFromTexture(target.texture); ImageFlipVertical(&image);
                return image;
            };
            Image background=render();
            game.player_.Update(0,{.fire=true},game.bullets_,game.stageContent_->PlayerBulletVisual(),
                                shot==0 ? game.stageContent_->PlayerHomingVisual() : game.stageContent_->PlayerNeedleVisual(),true);
            int homing=0,needles=0;
            float directionBefore=0;
            game.bullets_.Pool().ForEachActive([&](const Bullet& bullet) {
                if(bullet.homingTurnRate>0) { ++homing; directionBefore+=bullet.velocity.x; }
                if(bullet.visual==stage1::kPlayerNeedle) { ++needles; directionBefore+=bullet.velocity.x; }
            });
            assert(homing==(shot==0 ? 2 : 0));
            assert(needles==(shot==1 ? (power==128 ? 6 : 2) : 0));
            // 偏右目标迫使成对弹的横向合速度改变；不能只检查 homingTurnRate 字段存在。
            const std::array<Vector3,1> targets{{{4,0,4}}};
            game.bullets_.Update(0.2F,targets,game.player_.Position());
            game.UpdateBattleAnimations();
            if(homing>0 || needles>0) {
                float directionAfter=0;
                game.bullets_.Pool().ForEachActive([&](Bullet& bullet) {
                    if(bullet.homingTurnRate>0 || bullet.visual==stage1::kPlayerNeedle) {
                        directionAfter+=bullet.velocity.x;
                        const auto& frame=bullet.animation.player.Sample();
                        assert(frame.visible && frame.sprite>=0 && frame.color.a>0);
                    } else bullet.active=false; // 隔离主弹，像素断言只能由当前机体副弹满足。
                });
                if(homing>0)assert(directionAfter>directionBefore+1);
                else assert(std::abs(directionAfter-directionBefore)<0.0001F);
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
                    const std::string path=prefix+(shot==0 ? "-homing-power-" : "-needle-power-")+std::to_string(power)+".png";
                    assert(ExportImage(image,path.c_str()));
                }
                UnloadImage(image);
                std::cout<<(shot==0 ? "Homing Power " : "Needle Power ")<<power<<": "<<(homing+needles)<<" bullets, "<<ink<<" visible pixels, peak "<<peak<<"\n";
            }
            UnloadImage(background); UnloadRenderTexture(target);
        }
        game.selectedShot_=previousShot;
        game.StartPreview();
    }

    /// @brief 隔离真实太极圈资源，检查双圈像素、Focus 收拢、暂停冻结及卸载时动画借用的释放。
    static void TestPlayerOptions(Game& game, const std::string& prefix) {
        game.StartPreview();
        game.stage_=StageDirector{};
        game.actors_.Clear();game.boss_.reset();game.bullets_.Reset();game.drops_.Clear();
        const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
        const auto render=[&](bool hideOptions=false) {
            BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);
            // 场上已清空其他关卡对象，临时移出内容接口只隔离太极圈，HUD 绘制前恢复。
            auto content=hideOptions ? std::move(game.stageContent_) : nullptr;
            game.DrawWorld();
            if(content)game.stageContent_=std::move(content);
            game.DrawHud();EndTextureMode();EndDrawing();
            Image result=LoadImageFromTexture(target.texture);ImageFlipVertical(&result);return result;
        };
        assert(game.player_.Power()==Player::kMinPower && game.player_.OptionsVisible());
        Image background=render(true);
        game.UpdateBattleAnimations();
        const auto outward=game.player_.OptionPositions();
        for(int focused=0;focused<2;++focused) {
            if(focused) {
                game.player_.Update(8.0F/60,{.focus=true},game.bullets_,0,1,true);
                game.UpdateBattleAnimations();
            }
            const auto positions=game.player_.OptionPositions();
            Image result=render();
            for(int side=0;side<2;++side) {
                const auto& cursor=game.playerOptionAnimations_[side];
                assert(cursor.player.Source()!=nullptr && cursor.player.Sample().sprite==128);
                assert(cursor.selection.script==128+side && cursor.player.Sample().visible);
                const auto center=GetWorldToScreen(positions[side],game.camera_);
                int ink=0;
                for(int y=std::max(0,int(center.y)-105);y<std::min(result.height,int(center.y)+105);++y)
                    for(int x=std::max(0,int(center.x)-105);x<std::min(result.width,int(center.x)+105);++x) {
                        const auto a=GetImageColor(result,x,y),b=GetImageColor(background,x,y);
                        if(std::abs(int(a.r)-b.r)+std::abs(int(a.g)-b.g)+std::abs(int(a.b)-b.b)>10)++ink;
                    }
                assert(ink>100);
            }
            if(focused) {
                const auto distance=[](const std::array<Vector3,2>& points) {
                    return BulletSystem::Length({points[1].x-points[0].x,points[1].y-points[0].y,points[1].z-points[0].z});
                };
                assert(std::abs(distance(positions)*3-distance(outward))<0.0001F);
            }
            if(!prefix.empty()) {
                const auto path=prefix+(focused ? "-options-focus.png" : "-options-normal.png");
                assert(ExportImage(result,path.c_str()));
            }
            UnloadImage(result);
        }
        const double age=game.player_.VisualAge();
        const auto frame=game.playerOptionAnimations_[0].player.Sample();
        game.TogglePause();game.Update(0.5F);
        assert(game.player_.VisualAge()==age && game.playerOptionAnimations_[0].player.Sample().rotation.z==frame.rotation.z);
        game.TogglePause();
        game.EnterMainMenu();
        assert(game.playerOptionAnimations_[0].player.Source()==nullptr && game.playerOptionAnimations_[1].player.Source()==nullptr);
        UnloadImage(background);UnloadRenderTexture(target);
        game.StartPreview();
    }

    /// @brief 验证魔理沙独立图集、两种副机状态、真实导弹/激光像素及持续激光穿透与帧率无关伤害。
    static void TestMarisa(Game& game, const std::string& prefix) {
        TestUiFont(game);
        const int previousCharacter=game.selectedCharacter_,previousShot=game.selectedShot_;
        game.selectedCharacter_=1;game.selectedShot_=0;game.StartPreview();
        game.stage_=StageDirector{};game.actors_.Clear();game.boss_.reset();game.drops_.Clear();
        const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
        for(auto shot:{ShotType::MagicMissile,ShotType::IllusionLaser})for(bool focus:{false,true}) {
            game.player_.Reset();game.player_.ConfigureRun(3,3,false,shot);game.player_.AdjustPower(128);
            game.bullets_.Reset();game.playerOptionAnimations_={};
            const auto visuals=game.stageContent_->PlayerVisuals(shot);
            game.player_.Update(0,{.focus=focus,.fire=true},game.bullets_,visuals,true);
            for(int frame=0;frame<20;++frame) {
                game.player_.Update(1.0F/60,{.focus=focus,.fire=true},game.bullets_,visuals,true);
                game.bullets_.Update(1.0F/60);
            }
            game.player_.SyncCamera(game.camera_);game.UpdateBattleAnimations();
            for(int side=0;side<2;++side) {
                const auto& cursor=game.playerOptionAnimations_[side];
                assert(cursor.selection.atlas==battle::Atlas::Marisa && cursor.selection.script==128+side);
                assert(cursor.player.Sample().sprite==128 && cursor.player.Sample().rotation.z>0);
                assert(cursor.player.Source()!=game.assets_.GetVisual(stage1::kPlayerOptionLeft).archive.get());
            }
            const auto positions=game.player_.OptionPositions();
            const float separation=BulletSystem::Length({positions[1].x-positions[0].x,positions[1].y-positions[0].y,positions[1].z-positions[0].z});
            assert(std::abs(separation-(focus?0.52F:1.56F))<0.0001F);
            int secondary=0;
            game.bullets_.Pool().ForEachActive([&](const Bullet& bullet) {
                assert(bullet.animation.selection.atlas==battle::Atlas::Marisa);
                if(bullet.playerShotKind==PlayerShotKind::Missile || bullet.playerShotKind==PlayerShotKind::Laser) {
                    ++secondary;assert(bullet.animation.player.Sample().visible && bullet.animation.player.Sample().color.a>0);
                }
            });
            assert(secondary>0);
            BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);
            game.DrawWorld();game.DrawHud();EndTextureMode();EndDrawing();
            if(!prefix.empty()) {
                Image screenshot=LoadImageFromTexture(target.texture);ImageFlipVertical(&screenshot);
                const auto path=prefix+(shot==ShotType::MagicMissile?"-missile":"-laser")+(focus?"-focus.png":"-normal.png");
                assert(ExportImage(screenshot,path.c_str()));UnloadImage(screenshot);
            }
            // 隔离主弹与副机背景，以像素差验证副弹本身可见；不能让主弹替不可见激光满足截图检查。
            game.bullets_.Pool().ForEachActive([](Bullet& bullet){if(bullet.playerShotKind==PlayerShotKind::Projectile)bullet.active=false;});
            const auto renderSecondary=[&] {
                BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);game.DrawWorld();EndTextureMode();EndDrawing();
                Image result=LoadImageFromTexture(target.texture);ImageFlipVertical(&result);return result;
            };
            Image secondaryImage=renderSecondary();
            std::vector<Bullet*> sideBullets;
            game.bullets_.Pool().ForEachActive([&](Bullet& bullet){sideBullets.push_back(&bullet);bullet.active=false;});
            Image background=renderSecondary();
            for(auto* bullet:sideBullets)bullet->active=true;
            int ink=0;
            for(int y=0;y<secondaryImage.height;++y)for(int x=0;x<secondaryImage.width;++x) {
                const auto a=GetImageColor(secondaryImage,x,y),b=GetImageColor(background,x,y);
                if(std::abs(int(a.r)-b.r)+std::abs(int(a.g)-b.g)+std::abs(int(a.b)-b.b)>5)++ink;
            }
            if(!prefix.empty()) {
                const auto path=prefix+(shot==ShotType::MagicMissile?"-missile":"-laser")+(focus?"-focus-secondary.png":"-normal-secondary.png");
                assert(ExportImage(secondaryImage,path.c_str()));
            }
            UnloadImage(secondaryImage);UnloadImage(background);
            std::cout<<"Marisa "<<(shot==ShotType::MagicMissile?"missile":"laser")<<" focus "<<focus<<": "<<ink<<" secondary pixels\n";
            assert(ink>100);
            if(shot==ShotType::MagicMissile) {
                bool impact=false;
                game.bullets_.Pool().ForEachActive([&](Bullet& bullet){
                    if(!impact && bullet.playerShotKind==PlayerShotKind::Missile) {
                        game.bullets_.ExplodePlayerMissile(bullet);impact=true;
                    }
                });
                assert(impact);game.UpdateBattleAnimations();
                game.bullets_.Pool().ForEachActive([](const Bullet& bullet){
                    if(bullet.playerShotKind==PlayerShotKind::Explosion)
                        assert(bullet.animation.selection.atlas==battle::Atlas::Marisa && bullet.animation.player.Sample().additive);
                });
            } else {
                // 一次跨过淡出边界，原生中断必须只执行一次，年龄相同的重复同步不能重启动画。
                game.bullets_.Pool().ForEachActive([](Bullet& bullet){
                    if(bullet.playerShotKind==PlayerShotKind::Laser)bullet.age=bullet.laserFadeAt+15.0F/60;
                });
                game.UpdateBattleAnimations();game.UpdateBattleAnimations();
                game.bullets_.Pool().ForEachActive([](const Bullet& bullet){
                    if(bullet.playerShotKind==PlayerShotKind::Laser) {
                        assert(bullet.laserFadeStarted && bullet.animation.player.Sample().color.a>0 && bullet.animation.player.Sample().color.a<160);
                    }
                });
            }
        }

        for(int fps:{30,60,144}) {
            game.bullets_.Reset();game.actors_.Clear();
            game.player_.Reset();game.player_.ConfigureRun(3,3,false,ShotType::IllusionLaser);
            game.player_.Update(0,{.fire=true},game.bullets_,game.stageContent_->PlayerVisuals(game.player_.Shot()),true);
            Vector3 origin{},direction{};
            game.bullets_.Pool().ForEachActive([&](Bullet& bullet){
                if(bullet.playerShotKind!=PlayerShotKind::Laser)bullet.active=false;
                else if(bullet.playerSlot==0){origin=bullet.position;direction=bullet.beamDirection;}
            });
            for(float depth:{4.0F,7.0F}) {
                auto* enemy=game.actors_.Spawn(testing::SpawnAt({origin.x+direction.x*depth,origin.y+direction.y*depth,origin.z+direction.z*depth},stage1::kFairy,1000,0.1F));assert(enemy);
            }
            game.boss_=game.stageContent_->CreateBoss(testing::BattleAt(*game.stageContent_,1));game.boss_->hitRadius=0.1F;
            game.boss_->position={origin.x+direction.x*7,origin.y+direction.y*7,origin.z+direction.z*7};
            game.score_=0;
            for(int frame=0;frame<fps;++frame) {game.bullets_.Update(1.0F/fps);game.ResolveCollisions();game.ResolveCollisions();}
            int enemies=0;
            game.actors_.ForEachActive([&](const behavior::Actor& enemy){++enemies;assert(std::abs(enemy.health-820)<0.002F);});
            assert(enemies==2); // 一束激光同时穿透两个目标，伤害不随 FPS 或重复结算次数增加。
            if(game.score_!=600)std::cerr<<"Marisa laser score at "<<fps<<" FPS: "<<game.score_<<"\n";
            assert(game.score_==600); // 同一束持续命中的原始 60 个逻辑帧，不能按显示帧重复加分。
            game.boss_.reset();
            game.bullets_.Reset();game.actors_.Clear();
            auto* enemy=game.actors_.Spawn(testing::SpawnAt({},stage1::kFairy,100,0.1F));assert(enemy);
            auto* burst=game.bullets_.Spawn(BulletOwner::Player,{}, {1,0,0},0.5F,WHITE,stage1::kMarisaExplosion);
            assert(burst);burst->playerShotKind=PlayerShotKind::Explosion;burst->damage=0;burst->damagePerSecond=20;
            for(int frame=0;frame<fps/6;++frame){game.bullets_.Update(1.0F/fps);game.ResolveCollisions();}
            assert(std::abs(burst->velocity.x-1.0F/1024)<0.00001F); // 1/6 秒恰为十个原始减速帧。
            game.actors_.ForEachActive([](const behavior::Actor& enemy){assert(std::abs(enemy.health-(100-20.0F/6))<0.002F);});
        }

        // 以实际遭遇入口记录魔/恋组合，不能误写到灵梦的两栏。
        game.previewMode_=false;game.spellPractice_=false;game.practiceMode_=true;
        for(int combination:{2,3}) {
            game.FinishSpellRecord(false);game.selectedCharacter_=1;game.selectedShot_=combination%2;
            game.player_.ConfigureRun(3,3,false,static_cast<ShotType>(combination));
            const int before=game.spellRecords_.Get(combination,3).mainAttempts;
            game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),3);game.AdvanceBossSkill();
            assert(game.player_.IsMarisa() && game.spellRecords_.Get(combination,3).mainAttempts==before+1);
            game.FinishSpellRecord(false);
        }
        UnloadRenderTexture(target);
        game.selectedCharacter_=previousCharacter;game.selectedShot_=previousShot;game.StartPreview();
        std::cout<<"Marisa native atlases, Focus, missiles, lasers, piercing damage and combination records passed\n";
    }

    static void Run(Game& game, const std::string& prefix) {
        TestUiFont(game);
        TestPlayerOptions(game,prefix);
        TestPlayerShots(game,prefix);
        TestMarisa(game,prefix);
        TestBossTransitions(game,prefix);
        TestBossHud(game,prefix);
        TestFullWindow(game,prefix);
        TestDialogueTransitions(game,prefix);
        TestPortraits(game,prefix);
        TestNativeDialogue(game,prefix);
        TestSpatialFrameRates(game,prefix);
        TestWholeStage(game, prefix);
        TestBossEntryPreservesField(game);
        TestActorLifecycle(game,prefix);
    }

    /// @brief 单独覆盖演员池复用、完整退场、暂停冻结及资源卸载，便于定位整关结束后的生命周期问题。
    static void TestActorLifecycle(Game& game,const std::string& prefix) {
        // 经过真实 Game 更新验证高帧率下直接行为演员的入场与独立视觉时钟。
        game.StartPreview();game.stage_=StageDirector{};
        game.actors_.Spawn(testing::SpawnAt({0,-0.5F,8.8F}));
        game.actors_.Spawn(testing::SpawnAt({9,3.15F,7.8F}));
        for (int frame = 0; frame < 90; ++frame) game.Update(1.0F / 240.0F);
        assert(EnemyCount(game) == 2);
        game.actors_.ForEachActive([](const behavior::Actor& enemy) {
            assert(enemy.presentation.Sample().alpha == 1.0F);
        });
        Capture(game, prefix, "fairy-visible");

        game.StartPreview();
        game.stage_ = StageDirector{};
        game.actors_.Spawn(testing::SpawnAt({0,0,4}));
        game.UpdateActors(0.125F);
        Capture(game, prefix, "enemy-entry");
        game.TogglePause();
        const Vector3 pausedPosition = [&] {
            Vector3 position{};
            game.actors_.ForEachActive([&](const behavior::Actor& enemy) { position = enemy.position; });
            return position;
        }();
        game.Update(2.0F);
        game.actors_.ForEachActive([&](const behavior::Actor& enemy) {
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
        game.actors_.Spawn(testing::SpawnAt({4,0,5},stage1::kOrb));
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

        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1), 2);
        game.UpdateBoss(0.3F);
        assert(game.boss_ && game.boss_->active);
        Capture(game, prefix, "boss-entry");
        const float alpha = game.boss_->presentation.Sample().alpha;
        game.boss_->health = 0.0F;
        game.AdvanceBossSkill();
        assert(game.boss_->attack == 2);
        assert(game.boss_->presentation.Sample().alpha == alpha);
        assert(game.actorEffects_.Pool().ActiveCount() == 0);
        for (int phase = 2; phase <= 4; ++phase) {
            game.boss_->health = 0.0F;
            game.AdvanceBossSkill();
        }
        // 末符击破后先完成两句战后独白，再展示关卡奖励；关闭后完成剩余死亡演出与资源释放。
        for(int frame=0;frame<62;++frame)game.UpdateBoss(1.0F/60);
        assert(!game.boss_->active);
        game.BeginStageDialogue(4,true);
        assert(game.state_==RunState::Playing && game.dialogueActive_ && game.dialogueTimer_==0);
        assert(!game.stageClearShown_);
        game.dialogueTimer_=game.DialogueDuration();
        game.Update(1.0F/120);
        assert(game.state_==RunState::StageClear && !game.dialogueActive_);
        game.ContinueStageClear();
        assert(game.state_ == RunState::Finishing);
        assert(!game.boss_);
        assert(!game.assets_.Stage().visuals.empty());
        assert(game.bullets_.Pool().ActiveCount() == 0 && PickupCount(game) == 0);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);
        const auto finalScore = game.score_;
        game.AdvanceBossSkill();
        assert(game.score_ == finalScore);
        game.Update(0.2F);
        Capture(game, prefix, "boss-defeat");
        float pausedEffectAge=0;
        game.actorEffects_.Pool().ForEachActive([&](const ActorExitEffect& effect) {
            assert(effect.age>=0.2F&&effect.age<0.22F);pausedEffectAge=effect.age;
        });
        game.TogglePause();
        game.Update(2.0F);
        assert(game.state_ == RunState::Paused);
        assert(!game.assets_.Stage().visuals.empty());
        game.actorEffects_.Pool().ForEachActive([&](const ActorExitEffect& effect) {
            assert(effect.age==pausedEffectAge);
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
        auto retreat=testing::SpawnAt(Vector3{0,0,-10.5357141F});
        retreat.pattern=stage1::kLateTurningFairy;retreat.retireOutside=true;retreat.sourceScore=0;game.actors_.Spawn(retreat);
        game.UpdateActors(0.1F);
        assert(EnemyCount(game) == 0 && game.score_ == 0 && PickupCount(game) == 0);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);
        game.UpdateActors(0.1F);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);
        game.actorEffects_.Clear();

        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,0), 3);
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
        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1), 4);
        game.EndStage(RunState::GameOver);
        assert(game.state_ == RunState::Finishing);
        assert(!game.assets_.Stage().visuals.empty());
        game.Update(0.6F);
        assert(game.state_ == RunState::GameOver && game.assets_.Stage().visuals.empty());

        game.StartPreview();
        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1), 5);
        game.EndStage(RunState::Won);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);
        game.Reset();
        assert(game.state_ == RunState::MainMenu);
        assert(game.actorEffects_.Pool().ActiveCount() == 0);
        assert(game.assets_.Stage().visuals.empty());
    }
};

}  // namespace koumakyou

/// @brief 隐藏窗口集成测试；argv[1] 为可选截图前缀，argv[2] 支持 --campaign-only、--stage6-only、--stage5-only、--world-clock-only、--stage4-only、--stage3-only、--stage2-only、--stage-only、--lifecycle-only、--spatial-only、--actor-contacts-only、--viewport-only、--dialogue-only、--boss-hud-only、--player-shots-only、--player-options-only、--marisa-only、--bomb-only、--bomb-audio-only。
/// 仅 --bomb-audio-only 初始化音频且强制静音，设置与成绩只写临时目录。
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
    const bool audioTest=argc>=3 && std::string(argv[2])=="--bomb-audio-only";
    if(audioTest) {InitAudioDevice();SetMasterVolume(0);}
    {
        auto game = std::make_unique<koumakyou::Game>();
        SetTargetFPS(0);
        if(audioTest) {
            koumakyou::GameLifecycleProbe::TestPlayerBombAudio(*game);
        } else if(argc>=3 && std::string(argv[2])=="--world-clock-only") {
            koumakyou::GameLifecycleProbe::TestDialogueClearPolicy(*game);
            koumakyou::GameLifecycleProbe::TestWorldTimeClock(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--campaign-only") {
            koumakyou::GameLifecycleProbe::TestCampaignTransitions(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage6-only") {
            koumakyou::GameLifecycleProbe::TestStage6(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage5-only") {
            koumakyou::GameLifecycleProbe::TestStage5(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage4-only") {
            koumakyou::GameLifecycleProbe::TestStage4(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage3-only") {
            koumakyou::GameLifecycleProbe::TestStage3(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage2-only") {
            koumakyou::GameLifecycleProbe::TestStage2(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage-only") {
            koumakyou::GameLifecycleProbe::TestWholeStage(*game,prefix);
            koumakyou::GameLifecycleProbe::TestActorLifecycle(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--lifecycle-only") {
            koumakyou::GameLifecycleProbe::TestBossEntryPreservesField(*game);
            koumakyou::GameLifecycleProbe::TestActorLifecycle(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--spatial-only") {
            koumakyou::GameLifecycleProbe::TestSpatialFrameRates(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--actor-contacts-only") {
            koumakyou::GameLifecycleProbe::TestActorContacts(*game);
        } else if(argc>=3 && std::string(argv[2])=="--viewport-only") {
            koumakyou::GameLifecycleProbe::TestUiFont(*game);
            koumakyou::GameLifecycleProbe::TestFullWindow(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--bomb-only") {
            koumakyou::GameLifecycleProbe::TestPlayerBombs(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--marisa-only") {
            koumakyou::GameLifecycleProbe::TestMarisa(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--player-options-only") {
            koumakyou::GameLifecycleProbe::TestPlayerOptions(*game,prefix);
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
            koumakyou::GameLifecycleProbe::TestNativeDialogue(*game,prefix);
        } else {
            koumakyou::GameLifecycleProbe::Run(*game, prefix);
        }
    }
    if(audioTest)CloseAudioDevice();
    CloseWindow();
    std::filesystem::current_path(originalDirectory);
    std::filesystem::remove_all(scratch);
    std::cout << "Actor lifecycle integration tests passed\n";
}
