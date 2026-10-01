// 本文件负责初始化窗口与游戏循环，并处理静默截图等启动参数。

#include "app/game.hpp"

#include "raylib.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

/// @brief 将离屏渲染纹理导出为图片。
/// @param target 已渲染完成的目标纹理。
/// @param path 输出文件路径。
/// @return 图片读取和导出均成功时返回 true。
bool ExportCapture(RenderTexture2D target, const std::string& path) {
    Image image = LoadImageFromTexture(target.texture);
    if (!IsImageValid(image)) return false;
    // OpenGL 纹理读取方向与 PNG 行顺序相反，导出前翻转。
    ImageFlipVertical(&image);
    const bool exported = ExportImage(image, path.c_str());
    UnloadImage(image);
    return exported;
}

}  // namespace

/// @brief 解析截图选项、运行游戏主循环并释放窗口资源。
/// @param argc 命令行参数数量。
/// @param argv 命令行参数数组。
/// @return 运行成功返回 0，截图初始化或导出失败返回 1。
int main(int argc, char** argv) {
    constexpr int screenWidth = 1100;
    constexpr int screenHeight = 760;

    // 命令行截图入口复用同一套游戏逻辑，可指定关卡时间和观察角度。
    bool captureFrame = false;
    bool captureGameplay = false;
    bool captureOptions = false;
    bool captureStage = false;
    float captureStageTime = 0.0F;
    float captureYaw = 0.0F;
    float capturePitch = 0.0F;
#ifdef RED_MIST_DEBUG_FEATURES
    bool captureDebug = false;
#endif
    std::string capturePath = "red_mist_capture.png";
    if (argc >= 3 && std::string_view(argv[1]) == "--capture-stage") {
        captureFrame = true;
        captureStage = true;
        captureStageTime = std::clamp(std::strtof(argv[2], nullptr), 0.0F, 180.0F);
        if (argc >= 4) capturePath = argv[3];
        if (argc >= 5) captureYaw = std::strtof(argv[4], nullptr);
        if (argc >= 6) capturePitch = std::strtof(argv[5], nullptr);
#ifdef RED_MIST_DEBUG_FEATURES
        if (argc >= 7) captureDebug = std::string_view(argv[6]) == "--debug-render";
#endif
    } else if (argc >= 2 && (std::string_view(argv[1]) == "--capture" ||
                      std::string_view(argv[1]) == "--capture-gameplay" ||
                      std::string_view(argv[1]) == "--capture-options")) {
        captureFrame = true;
        captureGameplay = std::string_view(argv[1]) == "--capture-gameplay";
        captureOptions = std::string_view(argv[1]) == "--capture-options";
        if (argc >= 3) {
            capturePath = argv[2];
        }
    }

    unsigned int windowFlags = FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE;
    if (captureFrame) {
        // 截图模式在创建窗口之前禁用显示和焦点，避免打断桌面操作。
        windowFlags |= FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNFOCUSED |
                       FLAG_WINDOW_ALWAYS_RUN;
        SetTraceLogLevel(LOG_WARNING);
    }
    SetConfigFlags(windowFlags);
    InitWindow(screenWidth, screenHeight, "Red Mist 3D - Stage 1 Normal Demo");
    if (!captureFrame) InitAudioDevice();
    SetExitKey(KEY_NULL);
    if (!captureFrame) SetWindowMinSize(900, 620);
    SetTargetFPS(60);

    RenderTexture2D captureTarget{};
    if (captureFrame) {
        captureTarget = LoadRenderTexture(screenWidth, screenHeight);
        if (!IsRenderTextureValid(captureTarget)) {
            CloseWindow();
            return 1;
        }
    }
    bool captureSaved = !captureFrame;
    {
        redmist::Game game;
        if (captureStage) {
            // 以固定步长先推进关卡逻辑，再渲染目标时刻的画面。
            game.StartPreview();
#ifdef RED_MIST_DEBUG_FEATURES
            game.SetDebugDraw(captureDebug);
#endif
            for (float elapsed = 0.0F; elapsed < captureStageTime;
                 elapsed += 1.0F / 30.0F) {
                game.Update(1.0F / 30.0F);
            }
            game.SetPreviewView(captureYaw, capturePitch);
        } else if (captureGameplay) {
            game.Start(false);
            for (int frame = 0; frame < 360; ++frame) {
                game.Update(1.0F / 60.0F);
            }
        } else if (captureOptions) {
            game.OpenOptions();
        }
        int renderedFrames = 0;
        while (!WindowShouldClose() && !game.ShouldQuit()) {
            // 主循环每帧先更新状态，再绘制；截图模式改画到离屏纹理。
            const float deltaTime = std::min(GetFrameTime(), 1.0F / 20.0F);
            game.Update(deltaTime);
            if (captureStage) game.SetPreviewView(captureYaw, capturePitch);

            BeginDrawing();
            if (captureFrame) BeginTextureMode(captureTarget);
            ClearBackground(Color{7, 9, 20, 255});
            game.Draw();
            if (captureFrame) EndTextureMode();
            EndDrawing();

            if (captureFrame && ++renderedFrames >= 4) {
                captureSaved = ExportCapture(captureTarget, capturePath);
                break;
            }
        }
    }

    if (captureFrame) UnloadRenderTexture(captureTarget);
    if (IsAudioDeviceReady()) CloseAudioDevice();
    CloseWindow();
    return captureSaved ? 0 : 1;
}
