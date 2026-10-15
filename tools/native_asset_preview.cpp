// 本文件通过正式素材和绘制接口输出全部原 ANM 脚本的多时刻图集，供动画接入审阅。
#include "resources/asset_manager.hpp"
#include "resources/dds_image.hpp"
#include "render/animated_sprite.hpp"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace {
/// @brief 用固定相机与 3.2 世界单位高度绘制同一逻辑帧，导出实际贴图并返回几何覆盖的屏幕边界。
/// boundsShader 将片元设为白色，保留正式绘制器的顶点/投影；纹理与 shader 只借用。
/// 透明边缘变清晰时，模糊光圈可减少几个像素；尺寸检查必须测几何，不能把清晰度差异当成缩放。
std::array<int,4> MeasureWorldFootprint(Texture2D texture,const koumakyou::anm::Archive& archive,
                                      const std::filesystem::path& path,Shader boundsShader) {
    koumakyou::anm::Frame frame;
    frame.sprite=archive.sprites.front().id;frame.source=archive.sprites.front().source;frame.visible=true;
    constexpr Color background{17,31,47,255};
    const Camera3D camera{{0,0,-10},{0,0,0},{0,1,0},45,CAMERA_PERSPECTIVE};
    auto target=LoadRenderTexture(256,256);
    BeginDrawing();BeginTextureMode(target);ClearBackground(background);BeginMode3D(camera);
    koumakyou::DrawAnimatedBillboard(camera,texture,archive,frame,{},3.2F);
    EndMode3D();rlEnableBackfaceCulling();EndTextureMode();EndDrawing();
    Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
    const bool exported=ExportImage(image,path.string().c_str());UnloadImage(image);
    BeginDrawing();BeginTextureMode(target);ClearBackground(background);BeginMode3D(camera);BeginShaderMode(boundsShader);
    koumakyou::DrawAnimatedBillboard(camera,texture,archive,frame,{},3.2F);
    EndShaderMode();EndMode3D();rlEnableBackfaceCulling();EndTextureMode();EndDrawing();
    image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
    std::array<int,4> bounds{256,256,-1,-1};
    for(int y=0;y<256;++y)for(int x=0;x<256;++x) {
        const Color pixel=GetImageColor(image,x,y);
        const int difference=std::abs(int(pixel.r)-background.r)+std::abs(int(pixel.g)-background.g)+std::abs(int(pixel.b)-background.b);
        if(difference<=15)continue;
        bounds[0]=std::min(bounds[0],x);bounds[1]=std::min(bounds[1],y);
        bounds[2]=std::max(bounds[2],x);bounds[3]=std::max(bounds[3],y);
    }
    UnloadImage(image);UnloadRenderTexture(target);
    if(!exported || bounds[2]<bounds[0])throw std::runtime_error("Invalid world footprint review");
    return bounds;
}
}

/// @brief argv[1] 为截图目录，argv[2] 可选为原标准 DDS 目录；导出 0、12、32 帧并验证升级前后世界尺寸。
/// 两次采样的几何边界必须完全一致；不初始化音频、不写设置，失败返回非零。
int main(int argc,char** argv) {
    if(argc!=2 && argc!=3){std::cerr<<"usage: native_asset_preview OUTPUT_DIRECTORY [STANDARD_DDS_DIRECTORY]\n";return 1;}
    const auto output=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(output);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_UNFOCUSED|FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(1024,1024,"Native animation review");SetTargetFPS(0);
    // 只替换片元颜色，顶点仍使用 raylib 默认 shader，校验实际公告板尺寸与投影。
    Shader boundsShader{};
    if(argc==3)boundsShader=LoadShaderFromMemory(nullptr,"#version 330\nout vec4 finalColor;\nvoid main(){finalColor=vec4(1.0);}\n");
    int result=0;
    try {
        if(argc==3 && (!IsShaderValid(boundsShader) || boundsShader.id==rlGetShaderIdDefault()))
            throw std::runtime_error("Cannot create world footprint shader");
        for(const auto& definition:koumakyou::battle::Catalog()) {
            koumakyou::AssetManager assets(std::filesystem::path(GetApplicationDirectory())/"assets");
            const auto& atlas=assets.LoadBattleAtlas(definition.id);
            if(assets.LoadBattleAtlas(definition.id).texture.id!=atlas.texture.id)
                throw std::runtime_error("Atlas cache duplicated texture");
            if(argc==3) {
                const auto standardPath=std::filesystem::path(argv[2])/(std::string(definition.name)+".dds");
                Image standardImage=koumakyou::LoadOriginalDds(standardPath);
                const Texture2D standard=LoadTextureFromImage(standardImage);UnloadImage(standardImage);
                SetTextureFilter(standard,TEXTURE_FILTER_BILINEAR);SetTextureWrap(standard,TEXTURE_WRAP_REPEAT);
                std::array<int,4> before{},after{};
                try {
                    if(atlas.texture.width!=standard.width*2 || atlas.texture.height!=standard.height*2)
                        throw std::runtime_error("High-resolution atlas is not exactly doubled");
                    before=MeasureWorldFootprint(standard,*atlas.archive,output/(std::string(definition.name)+"-size-before.png"),boundsShader);
                    after=MeasureWorldFootprint(atlas.texture,*atlas.archive,output/(std::string(definition.name)+"-size-after.png"),boundsShader);
                    for(int i=0;i<4;++i)if(before[i]!=after[i])
                        throw std::runtime_error("World footprint changed: "+std::string(definition.name));
                } catch(...) {UnloadTexture(standard);throw;}
                UnloadTexture(standard);
                std::cout<<definition.name<<" size before "<<before[0]<<","<<before[1]<<","<<before[2]<<","<<before[3]
                         <<" after "<<after[0]<<","<<after[1]<<","<<after[2]<<","<<after[3]<<"\n";
            }
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
    if(IsShaderValid(boundsShader) && boundsShader.id!=rlGetShaderIdDefault())UnloadShader(boundsShader);
    CloseWindow();return result;
}
