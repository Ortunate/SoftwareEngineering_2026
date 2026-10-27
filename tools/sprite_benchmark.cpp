// 本文件负责测量空场景、高弹量及连续深度弹幕与激光混合场景的渲染帧时间。

#include "resources/asset_manager.hpp"
#include "render/sprite_renderer.hpp"
#include "stages/stage1/content.hpp"

#include "raylib.h"

#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

/// @brief 对给定场景连续绘帧，返回每帧平均耗时（毫秒）；frames 须为正数。
double DrawFrames(koumakyou::SpriteRenderer& renderer,
                  const koumakyou::BulletSystem& bullets,
                  std::span<const koumakyou::VisualResource> visuals,
                  const koumakyou::SpriteShaders& shaders,
                  const Camera3D& camera, int frames) {
    const double start = GetTime();
    const koumakyou::ActorEffectSystem effects;
    for (int frame = 0; frame < frames; ++frame) {
        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        renderer.Draw(camera, bullets, {}, nullptr, {}, visuals, shaders, effects);
        EndMode3D();
        EndDrawing();
    }
    return (GetTime() - start) * 1000.0 / frames;
}

}  // namespace

/// @brief argv[1] 为 runtime 素材根目录；--visible 显示窗口，--stress 追加 64 束激光的极端压力场景。
/// 默认隐藏窗口且不限帧；混合场景每档暖机 2 帧、测量 5 帧，输出毫秒均值及实际激光片段数。
int main(int argc, char** argv) {
    if(argc<2 || argc>4)return 1;
    bool visible=false,stress=false;
    for(int i=2;i<argc;++i) {
        const std::string_view option(argv[i]);
        if(option=="--visible")visible=true;
        else if(option=="--stress")stress=true;
        else return 1;
    }
    SetTraceLogLevel(LOG_WARNING);
    if (!visible) {
        SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNFOCUSED |
                       FLAG_WINDOW_ALWAYS_RUN);
    }
    InitWindow(1100, 760, "Sprite render benchmark");
    SetTargetFPS(0);

    int exitCode = 0;
    {
        koumakyou::AssetManager manager{std::filesystem::path(argv[1])};
        koumakyou::stage1::Stage1Content content;
        manager.LoadStage(content.Assets());
        const auto& assets = manager.Stage().visuals;
        const koumakyou::SpriteShaders& shaders = manager.Stage().shaders;
        if (!IsTextureValid(assets[koumakyou::stage1::kBlueBullet].texture) ||
            !IsTextureValid(assets[koumakyou::stage1::kRedBullet].texture)) {
            exitCode = 1;
        } else {
            koumakyou::SpriteRenderer renderer;
            Camera3D camera{};
            camera.position = {0.0F, 0.0F, -6.5F};
            camera.target = {0.0F, 0.0F, 1.0F};
            camera.up = {0.0F, 1.0F, 0.0F};
            camera.fovy = 72.0F;
            camera.projection = CAMERA_PERSPECTIVE;

            koumakyou::BulletSystem bullets;
            DrawFrames(renderer, bullets, assets, shaders, camera, 10);
            const double emptyMs = DrawFrames(renderer, bullets, assets,
                                              shaders, camera, 60);
            for (int i = 0;
                 i < static_cast<int>(koumakyou::BulletSystem::kCapacity); ++i) {
                const float x = (static_cast<float>(i % 64) - 31.5F) * 0.15F;
                const float y = (static_cast<float>((i / 64) % 64) - 31.5F) * 0.15F;
                const float z = (i / 4096 == 0) ? 0.0F : 2.0F;
                const bool blueKind = (i % 2) == 0;
                bullets.Spawn(koumakyou::BulletOwner::Enemy, {x, y, z}, {}, 0.14F,
                              blueKind ? BLUE : RED,
                              blueKind ? koumakyou::stage1::kBlueBullet
                                       : koumakyou::stage1::kRedBullet);
            }
            DrawFrames(renderer, bullets, assets, shaders, camera, 10);
            const double fullMs = DrawFrames(renderer, bullets, assets,
                                             shaders, camera, 60);
            std::cout << "mode=" << (visible ? "visible" : "hidden")
                      << ", empty=" << emptyMs << " ms/frame, full=" << fullMs
                      << " ms/frame, bullets=" << bullets.Pool().ActiveCount()
                      << '\n';
            // 每颗弹使用独立深度，覆盖激光切分随弹量放大的场景；先测无激光以区分基础精灵开销。
            bullets.Reset();
            for (int i=0;i<2048;++i) {
                const float x=(static_cast<float>((i*37)%127)-63)*0.10F;
                const float y=(static_cast<float>((i*53)%97)-48)*0.08F;
                const float z=-2.0F+i*26.0F/2048;
                bullets.Spawn(koumakyou::BulletOwner::Enemy,{x,y,z},{},0.10F,WHITE,
                              koumakyou::stage1::kBlueBullet);
            }
            for (int count:{0,8,64}) {
                if(count==64 && !stress)break;
                const int additional=count==8 ? 8 : count==64 ? 56 : 0;
                for (int i=count-additional;i<count;++i) {
                    auto* beam=bullets.SpawnBeam({(i%8-3.5F)*0.3F,(i/8-3.5F)*0.15F,-3},
                                                {0.02F,0,1},30,0.12F,0,10,WHITE);
                    // 原生敌激光和自机加色激光交替，覆盖混合状态切换的真实开销。
                    beam->visual=i%2==0 ? koumakyou::stage1::kNativeLaserBase+2
                                        : koumakyou::stage1::kMarisaLaser;
                    beam->age=1;
                }
                // 优化前的混合模式切换可能使每帧耗时达到秒级；少量暖机/测量帧也能稳定复现开销。
                DrawFrames(renderer,bullets,assets,shaders,camera,2);
                const double mixedMs=DrawFrames(renderer,bullets,assets,shaders,camera,5);
                std::cout<<"mixed: sprites=2048, beams="<<count<<", segments="<<renderer.LastBeamSegmentCount()
                         <<", "<<mixedMs<<" ms/frame"<<std::endl;
            }
        }
    }

    CloseWindow();
    return exitCode;
}
