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
        for (int codepoint : std::array<int, 6>{'0', '9', 'Z', 0x4e2d, 0x300c, 0x2605})
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
                const Color sidebar=GetImageColor(windowImage,windowImage.width*9/10,windowImage.height/2);
                assert(sidebar.r+sidebar.g+sidebar.b>0);
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
            game.StartRun(false);
            game.player_.SetInvulnerability(1000);
            assert(game.boss_->phase == phase && game.player_.Power() == Player::kMaxPower);
            for (int step = 0; step < 30 * 60 && game.state_ != RunState::Won; ++step)
                game.Update(1.0F / 60);
            assert(game.state_ == RunState::Won && game.bestScore_ == bestScore);
            // 超时练习只保留实际擦弹分数，不能获得旧版固定 25000 分捕获奖励。
            assert(game.score_ == game.graze_*100 && EnemyCount(game) == 0);
            game.StartRun(false); game.player_.SetInvulnerability(1000);
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

/// @brief 使用隐藏窗口运行集成测试；可选 argv[1] 是截图路径前缀，不初始化音频。
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
        koumakyou::GameLifecycleProbe::Run(*game, prefix);
    }
    CloseWindow();
    std::filesystem::current_path(originalDirectory);
    std::filesystem::remove_all(scratch);
    std::cout << "Actor lifecycle integration tests passed\n";
}
