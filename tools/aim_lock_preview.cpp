// 本文件通过正式 Game 绘制路径导出辅助锁定验收帧，并验证提示使用被选敌机的快照。
#include "app/game.hpp"
#include "app/math.hpp"
#include "stages/stage1/enemies.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
namespace koumakyou {
/// @brief 仅预览工具使用的友元探针；冻结关卡调度，控制敌机位置和辅助开关，不写玩家记录。
struct GameLifecycleProbe {
    /// @brief 导出 6 秒、24 FPS 的真实游戏画面；folder 为调用方创建的绝对输出目录。
    static void Capture(Game& game,const std::filesystem::path& folder) {
        game.StartPreview();
        game.stage_=StageDirector{};game.enemies_.Clear();game.boss_.reset();game.bullets_.Reset();
        game.dialogueActive_=false;game.spellCutInTimer_=0;game.state_=RunState::Playing;
        game.player_.Reset();game.player_.ConfigureRun(2,3,false,ShotType::Needle);
        auto enemy=std::make_unique<stage1::FairyEnemy>(stage1::FairyParams{{1.1F,0,5.5F}});
        auto* selected=enemy.get();game.enemies_.Add(std::move(enemy));
        auto decoy=std::make_unique<stage1::FairyEnemy>(stage1::FairyParams{{-4,1,6}});
        decoy->presentation.Update(1);game.enemies_.Add(std::move(decoy));
        const auto render=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
        assert(IsRenderTextureValid(render));
        for(int frame=0;frame<144;++frame) {
            const float t=frame/24.0F;
            const bool enabled=frame>=24 && frame<120;
            selected->position={1.1F+0.28F*std::sin(t*1.8F),0.3F*std::sin(t*1.2F),5.5F};
            selected->age=t;selected->presentation.Update(1.0F/24);
            const std::array<PlayerAimTarget,1> targets{{{selected->position,selected->hitRadius}}};
            game.player_.Update(1.0F/24,{.aimAssistEnabled=enabled},game.bullets_,0,1,true,targets);
            game.player_.SyncCamera(game.camera_);game.UpdateBattleAnimations();
            assert(game.player_.Aim().assisted==enabled);
            if(enabled)assert(game_math::DistanceSquared(game.player_.Aim().targetPosition,selected->position)<0.000001F);
            BeginDrawing();BeginTextureMode(render);ClearBackground(BLACK);game.Draw();
            // 验收字幕只属于导出工具，不进入产品 HUD。
            DrawRectangle(315,28,270,36,Color{15,9,19,210});
            DrawText(enabled?"TARGET LOCK / ASSIST ON":"ASSIST OFF",330,37,20,enabled?Color{255,221,143,255}:Color{190,180,180,255});
            EndTextureMode();EndDrawing();
            Image image=LoadImageFromTexture(render.texture);ImageFlipVertical(&image);
            const auto path=folder/("frame-"+std::to_string(1000+frame)+".png");
            assert(ExportImage(image,path.string().c_str()));UnloadImage(image);
        }
        UnloadRenderTexture(render);
    }
};
}
/// @brief argv[1] 指定 PNG 输出目录；设置放入其独立子目录，避免更改玩家的配置和成绩。
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    const auto folder=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(folder/"settings");
    const auto original=std::filesystem::current_path();std::filesystem::current_path(folder/"settings");
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_UNFOCUSED|FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(900,620,"Aim lock acceptance");SetTargetFPS(0);
    {auto game=std::make_unique<koumakyou::Game>();koumakyou::GameLifecycleProbe::Capture(*game,folder);}
    CloseWindow();std::filesystem::current_path(original);
    std::cout<<"Aim lock acceptance: 144 frames exported, target tracking and toggle assertions passed\n";
}
