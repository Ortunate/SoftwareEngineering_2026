#include "game/game.hpp"

#include "raylib.h"

#include <algorithm>
#include <string>
#include <string_view>

int main(int argc, char** argv) {
    constexpr int screenWidth = 1100;
    constexpr int screenHeight = 760;

    bool captureFrame = false;
    bool captureGameplay = false;
    bool captureOptions = false;
    std::string capturePath = "red_mist_capture.png";
    if (argc >= 2 && (std::string_view(argv[1]) == "--capture" ||
                      std::string_view(argv[1]) == "--capture-gameplay" ||
                      std::string_view(argv[1]) == "--capture-options")) {
        captureFrame = true;
        captureGameplay = std::string_view(argv[1]) == "--capture-gameplay";
        captureOptions = std::string_view(argv[1]) == "--capture-options";
        if (argc >= 3) {
            capturePath = argv[2];
        }
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "Red Mist 3D - Stage 1 Normal Demo");
    SetExitKey(KEY_NULL);
    SetWindowMinSize(900, 620);
    SetTargetFPS(60);

    redmist::Game game;
    if (captureGameplay) {
        game.Start();
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

        BeginDrawing();
        ClearBackground(Color{7, 9, 20, 255});
        game.Draw();
        EndDrawing();

        if (captureFrame && ++renderedFrames >= 4) {
            TakeScreenshot(capturePath.c_str());
            break;
        }
    }

    CloseWindow();
    return 0;
}
