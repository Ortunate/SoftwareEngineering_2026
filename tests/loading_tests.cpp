// 本文件用隐藏图形窗口验证加载期间可绘制动画、关卡时钟冻结、取消与失败资源清理。
#include "app/game.hpp"
#include "ui/typography.hpp"
#include "resources/dialogue_crop.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace koumakyou {
namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

struct GameLifecycleProbe {
    /// @brief 离屏绘制当前页并返回像素摘要；可选路径写 PNG，图片只由本函数持有。
    static std::uint64_t Capture(const Game& game, const std::filesystem::path& path = {}) {
        const auto target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        Check(IsRenderTextureValid(target), "Cannot create capture target");
        BeginDrawing();
        BeginTextureMode(target);
        ClearBackground(BLACK);
        game.Draw();
        EndTextureMode();
        EndDrawing();
        Image image = LoadImageFromTexture(target.texture);
        Check(IsImageValid(image), "Cannot read loading screen");
        ImageFlipVertical(&image);
        const Color* pixels = static_cast<const Color*>(image.data);
        std::uint64_t hash = 1469598103934665603ULL;
        for (int index = 0; index < image.width * image.height; ++index) {
            hash = (hash ^ pixels[index].r) * 1099511628211ULL;
            hash = (hash ^ pixels[index].g) * 1099511628211ULL;
        }
        if (!path.empty()) Check(ExportImage(image, path.string().c_str()), "Cannot export loading screen");
        UnloadImage(image);
        UnloadRenderTexture(target);
        return hash;
    }

    /// @brief 推进实际后台加载并持续绘制，检查加载期间时钟不走、进度不倒退；限时 60 秒。
    static void Finish(Game& game) {
        const double start = GetTime();
        float previous = 0;
        int frames = 0;
        while (game.IsLoading()) {
            Check(GetTime() - start < 60, "Loading timed out");
            Check(!game.loadingFailed_, "Stage loading failed");
            Check(game.stage_.Elapsed() == 0, "Stage time advanced during loading");
            game.Update(1.0F / 120.0F);
            const float progress = game.assets_.StageLoadProgress();
            Check(progress >= previous, "Loading progress went backwards");
            previous = progress;
            BeginDrawing();
            ClearBackground(BLACK);
            game.Draw();
            EndDrawing();
            ++frames;
        }
        Check(frames > 2 && game.state_ == RunState::Playing, "Missing loading frames or playing state");
        Check(game.stage_.Elapsed() == 0 && game.assets_.HasStage(), "Battle started before loading completed");
        std::cout << "Loading rendered " << frames << " frames in " << GetTime() - start << " seconds\n";
    }

    static void Run(Game& game, const std::filesystem::path& output) {
        const Font font = game.assets_.Common().uiFont;
        for (int codepoint : {0x5c11, 0x5973, 0x7948, 0x7977, 0x4e2d})
            Check(font.glyphs[GetGlyphIndex(font, codepoint)].value == codepoint, "Loading text has missing glyphs");

        game.Start(false);
        Check(game.IsLoading() && !game.assets_.HasStage(), "Start blocked instead of entering Loading");
        const auto first = Capture(game, output.empty() ? output : output / "loading-0.png");
        game.Update(0.37F);
        const auto second = Capture(game, output.empty() ? output : output / "loading-1.png");
        Check(first != second, "Loading animation did not move");
        Finish(game);
        // 后台解码和同步加载必须得到相同可见边界，不能仅在截图预览路径修复贴边。
        const auto assetRoot=std::filesystem::path(GetApplicationDirectory())/"assets";
        for (const auto& definition : game.stageContent_->Assets().visuals) {
            if (definition.path.empty() || definition.animation.atlas!=battle::Atlas::None) continue;
            Image image=LoadImage((assetRoot/definition.path).string().c_str());
            const auto expected=FindDialogueContent(image,definition.dialogueCrop);
            UnloadImage(image);
            const auto actual=game.assets_.GetVisual(definition.id).dialogueCrop;
            Check(std::abs(actual.x-expected.x)<0.00001F && std::abs(actual.y-expected.y)<0.00001F &&
                  std::abs(actual.width-expected.width)<0.00001F && std::abs(actual.height-expected.height)<0.00001F,
                  "Async portrait content bounds differ from the decoded image");
        }
        game.Update(1.0F / 60.0F);
        Check(game.stage_.Elapsed() > 0, "Battle failed to start after loading");

        // 从已有战斗重开，旧纹理借用须先清理；符卡练习在加载结束后才建 Boss。
        game.practiceMode_ = game.spellPractice_ = true;
        game.practiceAttack_ = 4;
        game.StartRun(false);
        Check(game.IsLoading() && !game.boss_, "Practice started before assets were ready");
        Finish(game);
        Check(game.boss_ && game.boss_->attack == 4 && game.player_.Power() == Player::kMaxPower,
              "Practice setup was lost during loading");

        game.Start(false);
        game.Update(0.01F); // 在 CPU 解码尚未上传时离开，结果必须随任务一起释放。
        game.Reset();
        Check(game.state_ == RunState::MainMenu && !game.assets_.HasStage(), "Cancelled load leaked stage state");
        game.Start(false);
        Finish(game);
        for (const auto size : std::array<Vector2, 2>{{{1280, 720}, {390, 844}}}) {
            game.state_ = RunState::Loading;
            SetWindowSize(static_cast<int>(size.x), static_cast<int>(size.y));
            Capture(game, output.empty() ? output : output / ("loading-" + std::to_string(static_cast<int>(size.x)) + ".png"));
        }
        game.Reset();

#ifdef KOUMAKYOU_DEBUG_FEATURES
        game.StartShotTest();
        game.captureCursor_ = false;
        Finish(game);
        int targets = 0;
        game.actors_.ForEachActive([&](const behavior::Actor&) { ++targets; });
        Check(game.shotTestActive_ && targets == 1, "Shot test target lost during loading");
        for (int frame = 0; frame < 20 * 60; ++frame) game.Update(1.0F / 60.0F);
        targets = 0;
        game.actors_.ForEachActive([&](const behavior::Actor&) { ++targets; });
        Check(targets == 1 && !game.boss_ && !game.dialogueActive_,
              "Shot test started the normal stage events");
        game.Reset();
#endif

        // 缺失素材的异常只在主线程取 future 时传播，任务销毁后仍可重新正常加载。
        AssetManager missing(std::filesystem::temp_directory_path() / "koumakyou-missing-loading-root");
        const std::array<VisualDefinition, 1> invalid{{{0, "missing.png"}}};
        missing.BeginStageLoad({invalid, {}, {}, {}});
        bool failed = false;
        const double deadline = GetTime() + 5;
        while (!failed && GetTime() < deadline) {
            try { (void)missing.AdvanceStageLoad(); }
            catch (const std::runtime_error&) { failed = true; }
            BeginDrawing(); EndDrawing();
        }
        missing.UnloadStage();
        Check(failed && !missing.HasStage(), "Missing asset failure was not reported or cleaned");
        game.Start(false);
        Finish(game);
        game.Reset();
    }
};
}

/// @brief 隐藏窗口测试，无音频；argv[1] 可指定加载页截图目录，设置文件仅写入临时目录。
int main(int argc, char** argv) {
    const auto original = std::filesystem::current_path();
    const auto output = argc > 1 ? std::filesystem::absolute(argv[1]) : std::filesystem::path{};
    if (!output.empty()) std::filesystem::create_directories(output);
    const auto scratch = std::filesystem::temp_directory_path() /
        ("koumakyou_loading_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(scratch);
    std::filesystem::current_path(scratch);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNFOCUSED | FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(1100, 760, "Loading tests");
    SetExitKey(KEY_NULL);
    int result = 0;
    try {
        koumakyou::Game game;
        SetTargetFPS(120);
        koumakyou::GameLifecycleProbe::Run(game, output);
        std::cout << "Loading integration tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    CloseWindow();
    std::filesystem::current_path(original);
    std::filesystem::remove_all(scratch);
    return result;
}
