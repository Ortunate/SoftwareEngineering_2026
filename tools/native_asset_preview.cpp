// 本文件通过正式素材和绘制接口输出全部原 ANM 脚本的多时刻图集，供动画接入审阅。
#include "resources/asset_manager.hpp"
#include "render/animated_sprite.hpp"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>

/// @brief argv[1] 为新建/复用的截图目录；在隐藏窗口按 0、12、32 帧导出所有图集的动画脚本。
/// 不初始化音频、不写玩家设置，资源加载失败会返回非零。
int main(int argc,char** argv) {
    if(argc!=2){std::cerr<<"usage: native_asset_preview OUTPUT_DIRECTORY\n";return 1;}
    const auto output=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(output);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_UNFOCUSED|FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(1024,1024,"Native animation review");SetTargetFPS(0);
    int result=0;
    try {
        for(const auto& definition:koumakyou::battle::Catalog()) {
            koumakyou::AssetManager assets(std::filesystem::path(GetApplicationDirectory())/"assets");
            const auto& atlas=assets.LoadBattleAtlas(definition.id);
            if(assets.LoadBattleAtlas(definition.id).texture.id!=atlas.texture.id)
                throw std::runtime_error("Atlas cache duplicated texture");
            const int columns=8,cell=144,rows=(atlas.archive->scripts.size()+columns-1)/columns;
            for(int at:{0,12,32}) {
                auto target=LoadRenderTexture(columns*cell,rows*cell);
                BeginDrawing();BeginTextureMode(target);ClearBackground({32,36,46,255});rlDisableBackfaceCulling();
                int index=0;
                for(const auto& script:atlas.archive->scripts) {
                    koumakyou::anm::Player animation;
                    animation.Start(*atlas.archive,script.id,0,atlas.archive->sprites.front().id);
                    animation.Advance(at/60.0);
                    const auto& frame=animation.Sample();
                    const float width=frame.source.width*std::max(0.01F,std::abs(frame.scale.x));
                    const float height=frame.source.height*std::max(0.01F,std::abs(frame.scale.y));
                    const float scale=std::min(112/width,112/height);
                    const float x=(index%columns)*cell+cell/2,y=(index/columns)*cell+80;
                    // 背景脚本的绝对原场地坐标只决定放置；审阅时居中，仍保留局部位移和原锚点。
                    Vector2 origin{x-frame.position.x*scale,y-frame.position.y*scale};
                    if(frame.topLeft) {
                        origin.x-=frame.source.width*scale*frame.scale.x/2;
                        origin.y-=frame.source.height*scale*frame.scale.y/2;
                    }
                    koumakyou::DrawAnimatedSprite2D(atlas.texture,*atlas.archive,frame,origin,scale);
                    DrawText(TextFormat("script %d",script.id),x-62,(index/columns)*cell+8,16,RAYWHITE);
                    ++index;
                }
                rlEnableBackfaceCulling();EndTextureMode();EndDrawing();
                Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
                // 已知循环角色三个时刻都应在首格身体区域有像素；文字不计入，防止空白截图假通过。
                if(definition.id==koumakyou::battle::Atlas::Rumia) {
                    int ink=0;
                    for(int y=32;y<140;++y)for(int x=8;x<136;++x) {
                        const Color pixel=GetImageColor(image,x,y);
                        if(pixel.r!=32 || pixel.g!=36 || pixel.b!=46)++ink;
                    }
                    if(ink<100) {UnloadImage(image);UnloadRenderTexture(target);throw std::runtime_error("Rumia sprite did not render");}
                }
                const auto path=output/(std::string(definition.name)+"-frame-"+std::to_string(at)+".png");
                if(!ExportImage(image,path.string().c_str()))throw std::runtime_error("Cannot export animation review");
                UnloadImage(image);UnloadRenderTexture(target);
            }
            std::cout<<definition.name<<" rendered "<<atlas.archive->scripts.size()<<" scripts\n";
        }
    } catch(const std::exception& error) { std::cerr<<error.what()<<"\n";result=1; }
    CloseWindow();return result;
}
