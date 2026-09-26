#include "game/sprite_renderer.hpp"

#include "raylib.h"

#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

double DrawFrames(redmist::SpriteRenderer& renderer,
                  const redmist::BulletSystem& bullets,
                  const redmist::SpriteAssets& assets,
                  const Camera3D& camera, int frames) {
    const redmist::RumiaBoss inactiveBoss;
    const double start = GetTime();
    for (int frame = 0; frame < frames; ++frame) {
        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        renderer.Draw(camera, bullets, {}, inactiveBoss, camera.position,
                      assets, false);
        EndMode3D();
        EndDrawing();
    }
    return (GetTime() - start) * 1000.0 / frames;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 && (argc != 3 ||
                      std::string_view(argv[2]) != "--visible")) return 1;
    const bool visible = argc == 3;
    SetTraceLogLevel(LOG_WARNING);
    if (!visible) {
        SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNFOCUSED |
                       FLAG_WINDOW_ALWAYS_RUN);
    }
    InitWindow(1100, 760, "Sprite render benchmark");
    SetTargetFPS(0);

    const std::filesystem::path assetsPath = argv[1];
    const Texture2D blue = LoadTexture(
        (assetsPath / "bullet_pearl_runtime.png").string().c_str());
    const Texture2D red = LoadTexture(
        (assetsPath / "bullet_red_runtime.png").string().c_str());
    if (!IsTextureValid(blue) || !IsTextureValid(red)) {
        CloseWindow();
        return 1;
    }
    {
        redmist::SpriteRenderer renderer;
        renderer.Load(assetsPath);
        const redmist::SpriteAssets assets{blue, red};
        Camera3D camera{};
        camera.position = {0.0F, 0.0F, -6.5F};
        camera.target = {0.0F, 0.0F, 1.0F};
        camera.up = {0.0F, 1.0F, 0.0F};
        camera.fovy = 72.0F;
        camera.projection = CAMERA_PERSPECTIVE;

        redmist::BulletSystem bullets;
        DrawFrames(renderer, bullets, assets, camera, 10);
        const double emptyMs = DrawFrames(renderer, bullets, assets, camera, 60);
        for (int i = 0; i < static_cast<int>(redmist::BulletSystem::kCapacity); ++i) {
            const float x = (static_cast<float>(i % 64) - 31.5F) * 0.15F;
            const float y = (static_cast<float>((i / 64) % 64) - 31.5F) * 0.15F;
            const float z = (i / 4096 == 0) ? 0.0F : 2.0F;
            const bool blueKind = (i % 2) == 0;
            bullets.Spawn(redmist::BulletOwner::Enemy, {x, y, z}, {}, 0.14F,
                          blueKind ? BLUE : RED,
                          blueKind ? redmist::BulletVisualKind::BluePearl
                                   : redmist::BulletVisualKind::RedOrb);
        }
        DrawFrames(renderer, bullets, assets, camera, 10);
        const double fullMs = DrawFrames(renderer, bullets, assets, camera, 60);
        std::cout << "mode=" << (visible ? "visible" : "hidden")
                  << ", empty=" << emptyMs << " ms/frame, full=" << fullMs
                  << " ms/frame, bullets=" << bullets.Pool().ActiveCount()
                  << '\n';
    }

    UnloadTexture(blue);
    UnloadTexture(red);
    CloseWindow();
    return 0;
}
