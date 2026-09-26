#include "game/game.hpp"

#include "raylib.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

bool ExportCapture(RenderTexture2D target, const std::string& path) {
    Image image = LoadImageFromTexture(target.texture);
    if (!IsImageValid(image)) return false;
    ImageFlipVertical(&image);
    const bool exported = ExportImage(image, path.c_str());
    UnloadImage(image);
    return exported;
}

}  // namespace

int main(int argc, char** argv) {
    constexpr int screenWidth = 1100;
    constexpr int screenHeight = 760;

    bool captureFrame = false;
    bool captureGameplay = false;
    bool captureOptions = false;
    bool captureStage = false;
    float captureStageTime = 0.0F;
    float captureYaw = 0.0F;
    float capturePitch = 0.0F;
    bool captureDebug = false;
    std::string capturePath = "red_mist_capture.png";
    if (argc >= 3 && std::string_view(argv[1]) == "--capture-stage") {
        captureFrame = true;
        captureStage = true;
        captureStageTime = std::clamp(std::strtof(argv[2], nullptr), 0.0F, 180.0F);
        if (argc >= 4) capturePath = argv[3];
        if (argc >= 5) captureYaw = std::strtof(argv[4], nullptr);
        if (argc >= 6) capturePitch = std::strtof(argv[5], nullptr);
        if (argc >= 7) captureDebug = std::string_view(argv[6]) == "--debug-render";
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
        windowFlags |= FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNFOCUSED |
                       FLAG_WINDOW_ALWAYS_RUN;
        SetTraceLogLevel(LOG_WARNING);
    }
    SetConfigFlags(windowFlags);
    InitWindow(screenWidth, screenHeight, "Red Mist 3D - Stage 1 Normal Demo");
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
            game.StartPreview();
            game.SetDebugDraw(captureDebug);
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
    CloseWindow();
    return captureSaved ? 0 : 1;
}
