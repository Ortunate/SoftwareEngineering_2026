// 本文件负责测量空场景与高弹量场景的精灵渲染帧时间。

#include "resources/asset_manager.hpp"
#include "render/sprite_renderer.hpp"
#include "stages/stage1/stage1_content.hpp"

#include "raylib.h"

#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

double DrawFrames(redmist::SpriteRenderer& renderer,
                  const redmist::BulletSystem& bullets,
                  std::span<const redmist::VisualResource> visuals,
                  const redmist::SpriteShaders& shaders,
                  const Camera3D& camera, int frames) {
    const double start = GetTime();
    for (int frame = 0; frame < frames; ++frame) {
        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        renderer.Draw(camera, bullets, {}, nullptr, visuals, shaders);
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

    int exitCode = 0;
    {
        redmist::AssetManager manager{std::filesystem::path(argv[1])};
        redmist::stage1::Stage1Content content;
        manager.LoadStage(content.Assets());
        const auto& assets = manager.Stage().visuals;
        const redmist::SpriteShaders& shaders = manager.Stage().shaders;
        if (!IsTextureValid(assets[redmist::stage1::kBlueBullet].texture) ||
            !IsTextureValid(assets[redmist::stage1::kRedBullet].texture)) {
            exitCode = 1;
        } else {
            redmist::SpriteRenderer renderer;
            Camera3D camera{};
            camera.position = {0.0F, 0.0F, -6.5F};
            camera.target = {0.0F, 0.0F, 1.0F};
            camera.up = {0.0F, 1.0F, 0.0F};
            camera.fovy = 72.0F;
            camera.projection = CAMERA_PERSPECTIVE;

            redmist::BulletSystem bullets;
            DrawFrames(renderer, bullets, assets, shaders, camera, 10);
            const double emptyMs = DrawFrames(renderer, bullets, assets,
                                              shaders, camera, 60);
            for (int i = 0;
                 i < static_cast<int>(redmist::BulletSystem::kCapacity); ++i) {
                const float x = (static_cast<float>(i % 64) - 31.5F) * 0.15F;
                const float y = (static_cast<float>((i / 64) % 64) - 31.5F) * 0.15F;
                const float z = (i / 4096 == 0) ? 0.0F : 2.0F;
                const bool blueKind = (i % 2) == 0;
                bullets.Spawn(redmist::BulletOwner::Enemy, {x, y, z}, {}, 0.14F,
                              blueKind ? BLUE : RED,
                              blueKind ? redmist::stage1::kBlueBullet
                                       : redmist::stage1::kRedBullet);
            }
            DrawFrames(renderer, bullets, assets, shaders, camera, 10);
            const double fullMs = DrawFrames(renderer, bullets, assets,
                                             shaders, camera, 60);
            std::cout << "mode=" << (visible ? "visible" : "hidden")
                      << ", empty=" << emptyMs << " ms/frame, full=" << fullMs
                      << " ms/frame, bullets=" << bullets.Pool().ActiveCount()
                      << '\n';
        }
    }

    CloseWindow();
    return exitCode;
}
