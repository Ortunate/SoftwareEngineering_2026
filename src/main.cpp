// 本文件负责初始化窗口与游戏循环，并处理静默截图等启动参数。

#include "app/game.hpp"
#include "platform/window_icon.hpp"
#include "platform/window_input.hpp"

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
    std::string replayPath;int replayView=0;float replaySeconds=0;
    bool captureFrame = false;
    bool captureGameplay = false;
    bool captureOptions = false;
    bool captureStage = false;
    float captureStageTime = 0.0F;
    float captureYaw = 0.0F;
    float capturePitch = 0.0F;
#ifdef KOUMAKYOU_DEBUG_FEATURES
    bool captureDebug = false;
#endif
    std::string capturePath = "koumakyou_capture.png";
    if(argc>=3 && (std::string_view(argv[1])=="--replay" || std::string_view(argv[1])=="--capture-replay")) {
        replayPath=argv[2];captureFrame=std::string_view(argv[1])=="--capture-replay";
        if(argc>=4)capturePath=argv[3];if(argc>=5)replaySeconds=std::strtof(argv[4],nullptr);if(argc>=6)replayView=std::atoi(argv[5]);
    } else if (argc >= 3 && std::string_view(argv[1]) == "--capture-stage") {
        captureFrame = true;
        captureStage = true;
        // 原文对话自动等待与所有 Boss 超时约需 301 秒，截图范围须覆盖战后独白。
        captureStageTime = std::clamp(std::strtof(argv[2], nullptr), 0.0F, 400.0F);
        if (argc >= 4) capturePath = argv[3];
        if (argc >= 5) captureYaw = std::strtof(argv[4], nullptr);
        if (argc >= 6) capturePitch = std::strtof(argv[5], nullptr);
#ifdef KOUMAKYOU_DEBUG_FEATURES
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
    InitWindow(screenWidth, screenHeight, "koumakyou - Stage 1 Normal Demo");
    if (!koumakyou::SetApplicationIcon())
        TraceLog(LOG_WARNING, "WINDOW: Could not load application icon");
    if (!koumakyou::DisableWindowInputMethods(GetWindowHandle()))
        TraceLog(LOG_WARNING, "INPUT: Could not disable window input methods");
    if (!captureFrame) InitAudioDevice();
    SetExitKey(KEY_NULL);
    // 页面布局按实际窗口尺寸缩放/重排，不再用最小窗口限制掩盖裁切问题。
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
        koumakyou::Game game;
        if(!replayPath.empty()) {
            if(!game.OpenReplayFile(replayPath,replayView))return 1;
            while(game.IsLoading() && !game.HasLoadingFailed())game.Update(1.0F/60);
            if(game.HasLoadingFailed())return 1;
            if(captureFrame)for(float t=0;t<replaySeconds;t+=1.0F/60)game.Update(1.0F/60);
        } else if (captureStage) {
            // 以固定步长先推进关卡逻辑，再渲染目标时刻的画面。
            game.StartPreview();
#ifdef KOUMAKYOU_DEBUG_FEATURES
            game.SetDebugDraw(captureDebug);
#endif
            for (float elapsed = 0.0F; elapsed < captureStageTime;
                 elapsed += 1.0F / 30.0F) {
                game.Update(1.0F / 30.0F);
            }
            game.SetPreviewView(captureYaw, capturePitch);
        } else if (captureGameplay) {
            game.Start(false);
            // 截图仍执行实际异步入口；加载帧不计入后面的六秒战斗预演。
            while (game.IsLoading() && !game.HasLoadingFailed() && !WindowShouldClose()) {
                game.Update(1.0F / 60.0F);
                BeginDrawing();
                ClearBackground(BLACK);
                game.Draw();
                EndDrawing();
            }
            for (int frame = 0; frame < 360; ++frame) {
                game.Update(1.0F / 60.0F);
            }
        } else if (captureOptions) {
            game.OpenOptions();
        }
        int renderedFrames = 0;
        while (!WindowShouldClose() && !game.ShouldQuit() &&
               !(captureFrame && game.HasLoadingFailed())) {
            // 主循环每帧先更新状态，再绘制；截图模式改画到离屏纹理。
            const float elapsed = GetFrameTime();
            const float deltaTime = std::min(elapsed, 1.0F / 20.0F);
            // 回放预算保留真实墙钟时间，低帧率下仍逐步消费录制的时间步。
            game.Update(deltaTime, elapsed);
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
