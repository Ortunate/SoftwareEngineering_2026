// 本文件以隐藏窗口回放键鼠输入，检查设置界面、录入防误触及配置保存。
#include "app/game.hpp"
#include "config/settings.hpp"
#include "raylib.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace {
/// @brief 检查可观察结果；失败抛出异常，由 main 输出错误并返回非零值。
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
/// @brief 回放 raylib 自动化事件；编号沿用 raylib 5.5/6.0 的事件格式。
void Event(unsigned int type, int a, int b = 0) {
    PlayAutomationEvent(AutomationEvent{0, type, {a, b, 0, 0}});
}
/// @brief 推进一帧，EndDrawing 更新输入的上一帧状态；窗口保持隐藏。
void Frame(redmist::Game& game) {
    game.Update(1.0F / 60.0F);
    BeginDrawing();
    ClearBackground(BLACK);
    game.Draw();
    EndDrawing();
}
/// @brief 模拟一次完整键盘按下和松开。
void Key(redmist::Game& game, int key) {
    Event(2, key); Frame(game);
    Event(1, key); Frame(game);
}
/// @brief 点击一个窗口坐标并释放，检验实际鼠标命中区域而非直接调用调整函数。
void Click(redmist::Game& game, int x, int y) {
    Event(7, x, y); Event(6, MOUSE_BUTTON_LEFT); Frame(game);
    Event(5, MOUSE_BUTTON_LEFT); Frame(game);
}
/// @brief 保存当前页面用于最小窗口尺寸的排版检查。
void Capture(redmist::Game& game, const char* path) {
    RenderTexture2D target = LoadRenderTexture(900, 620);
    BeginTextureMode(target);
    ClearBackground(BLACK);
    game.Draw();
    EndTextureMode();
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    Check(ExportImage(image, path), "screenshot export failed");
    UnloadImage(image);
    UnloadRenderTexture(target);
}
}

/// @brief 运行图形/音频集成测试；参数可指定截图目录，需支持 raylib 自动化事件和 OpenGL。
int main(int argc, char** argv) {
    const auto original = std::filesystem::current_path();
    const auto scratch = std::filesystem::temp_directory_path() /
        ("red_mist_ui_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(scratch);
    const auto screenshots = argc > 1 ? std::filesystem::absolute(argv[1]) : scratch;
    std::filesystem::create_directories(screenshots);
    std::filesystem::current_path(scratch);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNFOCUSED);
    InitWindow(900, 620, "Settings integration test");
    InitAudioDevice();
    SetMasterVolume(0.0F); // 不向用户播放测试音频。
    SetExitKey(KEY_NULL);
    int result = 0;
    try {
        {
            redmist::Game game;
            SetTargetFPS(0);
            game.OpenOptions();
            Event(7, 0, 0); Frame(game);
            // 在最小/默认窗口宽度下，数字、文字、按钮外一像素及行空白均不能调值。
            for (int width : {900, 1100}) {
                SetWindowSize(width, 620); Frame(game);
                const int center = GetScreenWidth() / 2;
                for (int row = 0; row < 3; ++row) {
                    const int y = 185 + row * 62;
                    const auto before = redmist::LoadOrCreateSettings(redmist::kSettingsFilename);
                    for (int x : {center - 200, center + 55, center + 100, center + 153, center + 251})
                        Click(game, x, y + 25);
                    Click(game, center + 70, y + 2);
                    Click(game, center + 234, y + 48);
                    auto after = redmist::LoadOrCreateSettings(redmist::kSettingsFilename);
                    Check(after.fpsLimit == before.fpsLimit && after.musicVolume == before.musicVolume &&
                          after.mouseSensitivity == before.mouseSensitivity, "click outside arrows changed setting");
                    Click(game, center + 249, y + 25);
                    after = redmist::LoadOrCreateSettings(redmist::kSettingsFilename);
                    Check(row == 0 ? after.fpsLimit == 120 : row == 1 ? after.musicVolume == 55 :
                          after.mouseSensitivity == 110, "right arrow did not increase");
                    Click(game, center + 57, y + 25);
                    after = redmist::LoadOrCreateSettings(redmist::kSettingsFilename);
                    Check(after.fpsLimit == before.fpsLimit && after.musicVolume == before.musicVolume &&
                          after.mouseSensitivity == before.mouseSensitivity, "left arrow did not decrease");
                }
            }
            SetWindowSize(900, 620); Frame(game);
            Click(game, 520, 210); Click(game, 520, 210);
            Check(redmist::LoadOrCreateSettings(redmist::kSettingsFilename).fpsLimit == 30, "FPS lower boundary wrapped");
            for (int i = 0; i < 7; ++i) Click(game, 684, 210);
            Check(redmist::LoadOrCreateSettings(redmist::kSettingsFilename).fpsLimit == 0, "FPS upper boundary wrapped");
            for (int i = 0; i < 4; ++i) Click(game, 520, 210);
            game.OpenOptions(); Event(7, 0, 0); Frame(game);
            Key(game, KEY_DOWN);
            Key(game, KEY_RIGHT);
            auto settings = redmist::LoadOrCreateSettings(redmist::kSettingsFilename);
            Check(settings.musicVolume == 55, "music volume not saved");
            for (int i = 0; i < 25; ++i) Key(game, KEY_LEFT);
            Check(redmist::LoadOrCreateSettings(redmist::kSettingsFilename).musicVolume == 0, "mute boundary failed");
            for (int i = 0; i < 25; ++i) Key(game, KEY_RIGHT);
            Check(redmist::LoadOrCreateSettings(redmist::kSettingsFilename).musicVolume == 100, "volume upper boundary failed");
            Key(game, KEY_DOWN);
            for (int i = 0; i < 35; ++i) Key(game, KEY_RIGHT);
            Check(redmist::LoadOrCreateSettings(redmist::kSettingsFilename).mouseSensitivity == 300, "sensitivity maximum failed");
            for (int i = 0; i < 35; ++i) Key(game, KEY_LEFT);
            Check(redmist::LoadOrCreateSettings(redmist::kSettingsFilename).mouseSensitivity == 10, "sensitivity minimum failed");
            for (int i = 0; i < 9; ++i) Key(game, KEY_RIGHT);
            Capture(game, (screenshots / "options.png").string().c_str());
            Key(game, KEY_DOWN); Key(game, KEY_ENTER);
            // 点击 Focus 行打开录入；持续按住的激活点击不能立即变成新绑定。
            Event(7, 300, 316); Event(6, MOUSE_BUTTON_LEFT); Frame(game); Frame(game);
            Check(redmist::LoadOrCreateSettings(redmist::kSettingsFilename).bindings.focus == redmist::MouseBinding(MOUSE_BUTTON_SIDE), "opening click captured");
            Capture(game, (screenshots / "binding-prompt.png").string().c_str());
            Event(5, MOUSE_BUTTON_LEFT); Frame(game);
            Event(6, MOUSE_BUTTON_EXTRA); Frame(game);
            Event(5, MOUSE_BUTTON_EXTRA); Frame(game);
            Check(redmist::LoadOrCreateSettings(redmist::kSettingsFilename).bindings.focus == redmist::MouseBinding(MOUSE_BUTTON_EXTRA), "mouse side binding failed");
            // 将 Focus 改成左键后，射击副键应交换为刚才的侧键，不能保留硬编码左键。
            Event(6, MOUSE_BUTTON_LEFT); Frame(game);
            Event(5, MOUSE_BUTTON_LEFT); Frame(game);
            Event(6, MOUSE_BUTTON_LEFT); Frame(game);
            Event(5, MOUSE_BUTTON_LEFT); Frame(game);
            settings = redmist::LoadOrCreateSettings(redmist::kSettingsFilename);
            Check(settings.bindings.focus == redmist::MouseBinding(MOUSE_BUTTON_LEFT), "left binding failed");
            Check(settings.bindings.fireAlt == redmist::MouseBinding(MOUSE_BUTTON_EXTRA), "mouse conflict swap failed");
            Capture(game, (screenshots / "bindings.png").string().c_str());
            Event(7, 0, 0); Frame(game);
            // Esc 取消不会修改绑定；静止鼠标不应抢走键盘列表选择。
            Key(game, KEY_ENTER); Key(game, KEY_ESCAPE);
            Check(redmist::LoadOrCreateSettings(redmist::kSettingsFilename).bindings.focus == settings.bindings.focus, "cancel altered binding");
            Key(game, KEY_DOWN); Key(game, KEY_ENTER); Key(game, KEY_F1);
            Check(redmist::LoadOrCreateSettings(redmist::kSettingsFilename).bindings.fire == KEY_F1, "keyboard rebind failed");
            game.Start(false); Frame(game);
            // 统一编码必须正确分发到鼠标查询；按住/首次按下分别用于移动和 Bomb。
            Event(6, MOUSE_BUTTON_EXTRA);
            Check(redmist::BindingDown(settings.bindings.fireAlt), "mouse held dispatch failed");
            Check(redmist::BindingPressed(settings.bindings.fireAlt), "mouse press dispatch failed");
            Frame(game);
            Check(!redmist::BindingPressed(settings.bindings.fireAlt), "mouse press repeated while held");
            Event(5, MOUSE_BUTTON_EXTRA); Frame(game);
            Check(!redmist::BindingDown(settings.bindings.fireAlt), "mouse release failed");
        }
        // 第二个实例模拟重启：读取配置后仍可加载关卡音乐与进入游戏。
        {
            redmist::Game restarted;
            const auto loaded = redmist::LoadOrCreateSettings(redmist::kSettingsFilename);
            Check(loaded.musicVolume == 100 && loaded.mouseSensitivity == 100, "restart persistence failed");
            restarted.Start(false); Frame(restarted);
        }
        std::puts("Settings UI integration passed.");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Settings UI integration failed: %s\n", error.what());
        result = 1;
    }
    if (IsAudioDeviceReady()) CloseAudioDevice();
    CloseWindow();
    std::filesystem::current_path(original);
    std::filesystem::remove(scratch / redmist::kSettingsFilename);
    if (scratch != screenshots) std::filesystem::remove(scratch);
    return result;
}
