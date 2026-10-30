// 本文件提供原版风格苦无的独立人工审阅：颜色、光效、实体碰撞与参考图。
#include "resources/projectile_paths.hpp"
#include "classic_renderer.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <array>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <cmath>
namespace fs=std::filesystem;
void shape(const review::CollisionAsset& asset,bool wire){
    rlDrawRenderBatchActive();rlDisableBackfaceCulling();
    for(const auto& part:asset.Parts()){rlBegin(wire?RL_LINES:RL_TRIANGLES);rlColor4ub(40,210,160,255);
        for(auto t:part.triangles){Vector3 v[]{t.a,t.b,t.c};for(int j=0;j<3;++j){rlVertex3f(v[j].x,v[j].y,v[j].z);if(wire){auto b=v[(j+1)%3];rlVertex3f(b.x,b.y,b.z);}}}rlEnd();}
    rlDrawRenderBatchActive();rlEnableBackfaceCulling();
}
int main(int argc,char** argv){try{
    fs::path root=fs::absolute(argc>1?argv[1]:".");bool capture=argc>2&&std::string(argv[2])=="--capture";
    auto output=root/"output/projectile-review/classic";fs::create_directories(output);
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_MSAA_4X_HINT|(capture?FLAG_WINDOW_HIDDEN:0));
    InitWindow(1440,960,"Classic Kunai | Independent review");SetTargetFPS(capture?0:60);
    {
    review::ClassicRenderer renderer(root/"assets/projectiles/kunai_classic/kunai_classic.gltf",review::ProjectileShaderDirectory(root),16);
    review::CollisionAsset collider(root/"assets/projectiles/kunai_classic/collision.bin");
    // 验收状态来自人工记录；重新生成的资产默认待验收，不由查看器自动认定通过。
    std::ifstream metadata(root/"assets/projectiles/kunai_classic/asset.json");
    std::string record((std::istreambuf_iterator<char>(metadata)),std::istreambuf_iterator<char>());
    bool approved=record.find("\"humanAcceptance\": \"approved\"")!=std::string::npos;
    review::CollisionScratch scratch;scratch.Prepare(collider.MaxPartTriangles());
    Texture2D reference=LoadTexture((root/"kunai.png").string().c_str());
    RenderTexture2D stage=LoadRenderTexture(1080,800);
    RenderTexture2D canvas=LoadRenderTexture(1440,960);
    int mode=0,color=2,frame=0,bg=0;bool glow=true,gallery=false,probeOn=false,turn=false;
    float yaw=-.25F,pitch=.1F,distance=1.8F,strength=.55F,width=.015F;
    Vector3 probe{0,-.335F,0};float radius=.02F;
    const char* names[]{"01-material","02-front","03-side","04-back","05-clay","06-collision","07-overlay","08-hole","09-ring","10-palette","11-glow-off","12-glow-max","13-light"};
    while(!WindowShouldClose()){
        if(capture){mode=0;gallery=false;probeOn=false;yaw=-.25F;pitch=.1F;distance=1.8F;
            if(frame==1)yaw=pitch=0;if(frame==2)yaw=PI/2,pitch=0;if(frame==3)yaw=PI,pitch=0;
            if(frame==4)mode=1;if(frame==5)mode=3;if(frame==6)mode=4;
            if(frame==7||frame==8){mode=4;probeOn=true;probe={frame==7?0.F:.115F,-.335F,0};radius=.02F;}
            if(frame==9)gallery=true;if(frame==10)glow=false;if(frame==11)glow=true,strength=2,width=.025F;if(frame==12)bg=1;
        }else{
            if(GetMousePosition().x>340&&IsMouseButtonDown(MOUSE_BUTTON_LEFT)){yaw-=GetMouseDelta().x*.008F;pitch=Clamp(pitch+GetMouseDelta().y*.006F,-1.4F,1.4F);}
            distance=Clamp(distance*std::pow(.9F,GetMouseWheelMove()),.4F,5.F);
            for(int i=0;i<5;++i)if(IsKeyPressed(KEY_ONE+i))mode=i,gallery=false;
            if(IsKeyPressed(KEY_C))color=(color+1)%16;if(IsKeyPressed(KEY_G))glow=!glow;
            if(IsKeyPressed(KEY_T))gallery=!gallery;if(IsKeyPressed(KEY_B))bg=1-bg;
            if(IsKeyPressed(KEY_SPACE))turn=!turn;if(turn)yaw+=GetFrameTime()*.35F;
            if(IsKeyPressed(KEY_P))probeOn=!probeOn;
            if(IsKeyPressed(KEY_H))probe={0,-.335F,0},probeOn=true;
            if(IsKeyPressed(KEY_J))probe={.115F,-.335F,0},probeOn=true;
            if(IsKeyPressed(KEY_R))yaw=-.25F,pitch=.1F,distance=1.8F;
            if(IsKeyDown(KEY_Q))strength=Clamp(strength-GetFrameTime(),0,2);if(IsKeyDown(KEY_E))strength=Clamp(strength+GetFrameTime(),0,2);
            if(IsKeyDown(KEY_LEFT_BRACKET))width=Clamp(width-GetFrameTime()*.015F,0,.025F);
            if(IsKeyDown(KEY_RIGHT_BRACKET))width=Clamp(width+GetFrameTime()*.015F,0,.025F);
            if(probeOn){float d=GetFrameTime()*.1F;probe.x+=(IsKeyDown(KEY_RIGHT)-IsKeyDown(KEY_LEFT))*d;probe.y+=(IsKeyDown(KEY_UP)-IsKeyDown(KEY_DOWN))*d;probe.z+=(IsKeyDown(KEY_PAGE_UP)-IsKeyDown(KEY_PAGE_DOWN))*d;
                radius=Clamp(radius+(IsKeyDown(KEY_EQUAL)-IsKeyDown(KEY_MINUS))*d*.2F,.001F,.1F);}
        }
        Camera3D camera{{distance*std::cos(pitch)*std::sin(yaw),distance*std::sin(pitch),distance*std::cos(pitch)*std::cos(yaw)},{0,0,0},{0,1,0},40,CAMERA_PERSPECTIVE};
        if(gallery)camera.position={0,0,4.2F};
        std::array<review::ProjectileInstance,16> instances{};
        for(int i=0;i<(gallery?16:1);++i){instances[i].appearance=review::ClassicPalette(gallery?i:color);instances[i].appearance.glowStrength=strength;instances[i].appearance.glowWidth=width;
            if(gallery){instances[i].position={(i%8-3.5F)*.43F,(.5F-i/8)*.8F,0};instances[i].scale=.7F;}}
        renderer.Prepare(std::span(instances.data(),gallery?16:1),camera,1080.F/800);
        bool contact=review::TouchesInstance(collider,scratch,review::ProjectileInstance{},probe,radius);
        BeginTextureMode(stage);ClearBackground(bg?Color{226,230,235,255}:Color{22,29,43,255});BeginMode3D(camera);
        if(mode!=3)renderer.Draw(glow,mode==1||mode==2);
        if(!gallery&&(mode>=2)){if(mode==4)rlDisableDepthTest();shape(collider,mode!=3);rlEnableDepthTest();}
        if(probeOn&&!gallery){DrawSphereWires(probe,radius,12,20,contact?ORANGE:GREEN);}
        EndMode3D();EndTextureMode();
        BeginTextureMode(canvas);ClearBackground({13,19,30,255});
        DrawText("CLASSIC KUNAI",24,28,28,RAYWHITE);DrawText("146 triangles / 1 mesh",24,72,19,LIGHTGRAY);
        DrawText(approved?"HUMAN REVIEW APPROVED":"PENDING HUMAN REVIEW",24,108,18,approved?GREEN:GOLD);
        const char* lines[]{"1  Material","2  Clay","3  Wireframe","4  Collision only","5  Collision overlay","C  Next color / click swatch","G  Glow on/off","T  16-color gallery","B  Light/dark background","P  Collision probe","H  Ring hole / J Ring contact","Arrows XY / PgUp PgDn Z","+/- Probe radius","Q/E Glow strength","[ / ] Glow width","Mouse drag / wheel: camera","Space: rotate / R: reset","F12: capture"};
        for(int i=0;i<18;++i)DrawText(lines[i],24,155+i*25,16,LIGHTGRAY);
        for(int i=0;i<16;++i){Rectangle r{24.F+(i%8)*35,620.F+(i/8)*38,29,29};DrawRectangleRec(r,review::ClassicPalette(i).bodyColor);if(color==i)DrawRectangleLinesEx(r,2,WHITE);if(CheckCollisionPointRec(GetMousePosition(),r)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))color=i;}
        DrawText(TextFormat("C%02i  Glow %s",color,glow?"ON":"OFF"),24,710,19,RAYWHITE);
        DrawText(TextFormat("Strength %.2f / Width %.3f",strength,width),24,741,16,LIGHTGRAY);
        DrawText("SOURCE REFERENCE",24,785,16,GOLD);
        if(reference.id)DrawTexturePro(reference,{0,0,(float)reference.width,(float)reference.height},{24,815,280,100},{0,0},0,WHITE);
        DrawTextureRec(stage.texture,{0,0,1080,-800},{340,80},WHITE);
        DrawText(gallery?"ONE SHARED MESH / 16 PARAMETER COLORS":"SOLID COLOR / VERTEX HIGHLIGHT MASK / NO TEXTURE",355,35,20,RAYWHITE);
        DrawText(TextFormat("%i draw calls | %i triangles | glow never changes collision",renderer.Stats().drawCalls,renderer.Stats().triangles),355,903,18,LIGHTGRAY);
        if(probeOn&&!gallery)DrawText(TextFormat("%s  radius %.3f  XYZ %.3f %.3f %.3f",contact?"CONTACT":"CLEAR",radius,probe.x,probe.y,probe.z),355,934,16,contact?ORANGE:GREEN);
        EndTextureMode();BeginDrawing();ClearBackground(BLACK);DrawTextureRec(canvas.texture,{0,0,1440,-960},{0,0},WHITE);EndDrawing();
        if(capture||IsKeyPressed(KEY_F12)){auto path=output/(capture?std::string(names[frame])+".png":"manual-"+std::to_string((int)(GetTime()*1000))+".png");Image image=LoadImageFromTexture(canvas.texture);ImageFlipVertical(&image);if(!ExportImage(image,path.string().c_str()))throw std::runtime_error("Capture failed");UnloadImage(image);}
        if(capture&&++frame==13)break;
    }
    UnloadTexture(reference);UnloadRenderTexture(stage);UnloadRenderTexture(canvas);
    }CloseWindow();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';if(IsWindowReady())CloseWindow();return 1;}}
