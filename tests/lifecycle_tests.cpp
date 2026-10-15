// 本文件负责在隐藏图形窗口中验证 Game 的击破、Boss 入场保留战场、清场及资源生命周期。

#include "app/game.hpp"
#include "rlgl.h"
#include "stages/stage1/enemies.hpp"
#include "render/ui_typography.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <filesystem>
#include <chrono>
#include <array>

namespace koumakyou {

/// @brief 仅此测试定义的友元探针；调用正式游戏路径验证效果不重复奖励或保留战斗对象。
struct GameLifecycleProbe {
    /// @brief 确认正式资源加载了中英文字形且数字等宽，避免界面静默退回默认像素字体。
    static void TestUiFont(const Game& game) {
        const Font font = game.assets_.Common().uiFont;
        assert(font.texture.id != GetFontDefault().texture.id);
        for (int codepoint : std::array<int, 8>{'0', '9', 'Z', 0x4e2d, 0x300c, 0x2605, 0x5e7b, 0x5377})
            assert(font.glyphs[GetGlyphIndex(font, codepoint)].value == codepoint);
        const float zeroWidth = ui::MeasureText(font, "000000000", 38).x;
        assert(std::abs(zeroWidth - ui::MeasureText(font, "111111111", 38).x) < 0.001F);
        assert(std::abs(zeroWidth - ui::MeasureText(font, "987654321", 38).x) < 0.001F);
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
        game.BeginStageDialogue(1);
        const auto lines=game.CurrentDialogue();
        float start=0;
        for(std::size_t index=0;index<lines.size();++index) {
            game.dialogueTimer_=start+std::max(1.1F,lines[index].textDelay+lines[index].line2Delay+0.1F);
            assert(game.DialogueIndex()==static_cast<int>(index));
            const auto name="portrait-dialogue-"+std::to_string(index);
            Capture(game,prefix,name.c_str());
            start+=lines[index].duration;
        }
        // 重建计时器时应恢复双方表情，而不是只设置当前说话人的图。
        game.dialogueTimer_=31;
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

    static void Run(Game& game, const std::string& prefix) {
        TestUiFont(game);
        TestFullWindow(game,prefix);
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
        // 末符击破后保留原作死亡演出和两句战后独白；完成独白后才进入结算。
        for(int frame=0;frame<62;++frame)game.UpdateBoss(1.0F/60);
        assert(!game.boss_->active);
        game.BeginStageDialogue(4,true);
        game.dialogueTimer_=game.DialogueDuration();
        game.Update(0);
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

/// @brief 使用隐藏窗口运行集成测试；argv[1] 可选为截图前缀，argv[2]=--viewport-only 仅复核视口。
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
        } else {
            koumakyou::GameLifecycleProbe::Run(*game, prefix);
        }
    }
    CloseWindow();
    std::filesystem::current_path(originalDirectory);
    std::filesystem::remove_all(scratch);
    std::cout << "Actor lifecycle integration tests passed\n";
}
