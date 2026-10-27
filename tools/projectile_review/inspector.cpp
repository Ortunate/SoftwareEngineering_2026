// 本文件实现游戏外苦无审阅：实体材质、贴图通道、同源碰撞叠加及可移动探针。
#include "collision.hpp"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include <cmath>
namespace fs=std::filesystem;
namespace {
const Color ink{32,42,53,255},muted{118,127,137,255},gold{189,143,77,255},panel{22,30,40,255};
Font font{};
void text(const std::string& s,float x,float y,float size,Color c){DrawTextEx(font,s.c_str(),{x,y},size,.3F,c);}
bool button(const char* title,Rectangle r,bool selected=false){
    bool hover=CheckCollisionPointRec(GetMousePosition(),r);
    DrawRectangleRounded(r,.16F,5,selected?Color{62,74,86,255}:hover?Color{44,55,67,255}:Color{31,41,53,255});
    if(selected)DrawRectangle(static_cast<int>(r.x),static_cast<int>(r.y+7),3,static_cast<int>(r.height-14),gold);
    text(title,r.x+13,r.y+9,17,selected?Color{248,228,194,255}:Color{202,211,220,255});
    return hover&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
void label(const char* title,int y){text(title,24,static_cast<float>(y),12,Color{133,151,169,255});}
struct State {
    int mode=0,background=0,channel=0;
    float yaw=-.42F,pitch=.12F,distance=1.8F,light=.25F,exposure=1;
    Vector3 target{},probe{0,-.410F,0};float radius=.020F;
    bool probeOn=false,textures=false,turn=false;
};
void preset(State& s,int n){
    s.probeOn=true;
    const Vector3 points[]{{0,.499F,0},{0,.20F,.020F},{0,-.20F,.027F},{.071F,-.410F,0},{0,-.410F,0}};
    s.probe=points[n];s.radius=n==4?.020F:.008F;
}
Camera3D cameraFor(const State& s){
    Camera3D c{};c.target=s.target;c.up={0,1,0};c.fovy=40;c.projection=CAMERA_PERSPECTIVE;
    c.position=Vector3Add(c.target,{s.distance*std::cos(s.pitch)*std::sin(s.yaw),s.distance*std::sin(s.pitch),s.distance*std::cos(s.pitch)*std::cos(s.yaw)});
    return c;
}
// 使用真实碰撞三角形画面，线框不会重建成其他近似形状。
void drawCollision(const review::CollisionAsset& shape,bool wire){
    const Color colors[]{{48,177,181,255},{223,166,81,255},{91,149,221,255},{183,130,224,255},{214,151,88,255},{104,184,136,255},{231,125,129,255}};
    int index=0;
    for(const auto& part:shape.Parts()){
        Color color=colors[index++%7];if(wire)color=Fade(color,.58F);
        rlBegin(wire?RL_LINES:RL_TRIANGLES);rlColor4ub(color.r,color.g,color.b,color.a);
        for(const auto& t:part.triangles){
            if(wire){for(auto e:{std::pair{t.a,t.b},std::pair{t.b,t.c},std::pair{t.c,t.a}}){rlVertex3f(e.first.x,e.first.y,e.first.z);rlVertex3f(e.second.x,e.second.y,e.second.z);}}
            else{for(auto p:{t.a,t.b,t.c})rlVertex3f(p.x,p.y,p.z);}
        }rlEnd();
    }
}
void saveTexture(RenderTexture2D target,const fs::path& path){
    Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
    if(!ExportImage(image,path.string().c_str())){UnloadImage(image);throw std::runtime_error("Screenshot export failed");}
    UnloadImage(image);
}
void bindShader(Shader shader){
    shader.locs[SHADER_LOC_MATRIX_MODEL]=GetShaderLocation(shader,"matModel");
    shader.locs[SHADER_LOC_MATRIX_NORMAL]=GetShaderLocation(shader,"matNormal");
    shader.locs[SHADER_LOC_MAP_ALBEDO]=GetShaderLocation(shader,"texture0");
    shader.locs[SHADER_LOC_MAP_NORMAL]=GetShaderLocation(shader,"texture1");
    shader.locs[SHADER_LOC_MAP_METALNESS]=GetShaderLocation(shader,"texture2");
}
} // namespace
int main(int argc,char** argv){try{
    fs::path root=argc>=2?fs::absolute(argv[1]):fs::absolute(argv[0]).parent_path().parent_path().parent_path();
    fs::path asset=root/"assets/projectiles/kunai_silver",output=root/"output/projectile-review";
    bool capture=argc>2&&std::string(argv[2])=="--capture";
    bool benchmark=argc>2&&std::string(argv[2])=="--benchmark";
    fs::create_directories(output);fs::create_directories(root/".local/logs");
    review::CollisionAsset collider(asset/"collision.bin");
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT|((capture||benchmark)?FLAG_WINDOW_HIDDEN:FLAG_WINDOW_RESIZABLE));
    InitWindow(1440,960,"Kunai | Independent asset review");SetWindowMinSize(1200,920);SetTargetFPS(benchmark?0:capture?240:60);
    if(!IsWindowReady())throw std::runtime_error("OpenGL window unavailable");
    font=LoadFontEx((root/".deps/raylib-5.5/examples/text/resources/anonymous_pro_bold.ttf").string().c_str(),32,nullptr,0);
    if(!font.texture.id)font=GetFontDefault();
    SetTextureFilter(font.texture,TEXTURE_FILTER_BILINEAR);
    Model model=LoadModel((asset/"kunai_silver.gltf").string().c_str());
    if(model.meshCount!=7)throw std::runtime_error("Expected seven imported meshes");
    BoundingBox visualBounds=GetModelBoundingBox(model);
    if(visualBounds.max.y<.49F||visualBounds.min.y>-.49F)throw std::runtime_error("Imported axes/bounds differ from collision asset");
    Shader shader=LoadShader((root/"tools/projectile_review/shaders/material.vs").string().c_str(),(root/"tools/projectile_review/shaders/material.fs").string().c_str());
    if(shader.id==rlGetShaderIdDefault())throw std::runtime_error("PBR shader failed to compile");
    bindShader(shader);
    std::set<unsigned> ownedTextures;
    for(int i=0;i<model.materialCount;++i){
        auto& m=model.materials[i];m.shader=shader;
        for(int k=0;k<=MATERIAL_MAP_BRDF;++k)if(m.maps[k].texture.id>1)ownedTextures.insert(m.maps[k].texture.id);
        // raylib 5.5 将 glTF 金属/粗糙度合图载入 roughness 槽；查看器显式绑定采样通道。
        m.maps[MATERIAL_MAP_METALNESS].texture=m.maps[MATERIAL_MAP_ROUGHNESS].texture;
        for(int k:{MATERIAL_MAP_ALBEDO,MATERIAL_MAP_NORMAL,MATERIAL_MAP_METALNESS})if(m.maps[k].texture.id>1){
            GenTextureMipmaps(&m.maps[k].texture);SetTextureFilter(m.maps[k].texture,TEXTURE_FILTER_ANISOTROPIC_8X);
        }
    }
    const char* channels[]{"baseColor","normal","roughness","metallic","ao"};
    const char* channelNames[]{"BASE COLOR","NORMAL","ROUGHNESS","METALLIC","AMBIENT OCCLUSION"};
    Texture2D channelTextures[2][5]{};
    for(int m=0;m<2;++m)for(int c=0;c<5;++c){
        std::string file=std::string(m?"indigo_wrap_":"forged_steel_")+channels[c]+".png";
        channelTextures[m][c]=LoadTexture((asset/"textures"/file).string().c_str());
        if(!channelTextures[m][c].id)throw std::runtime_error("Missing texture "+file);
        SetTextureFilter(channelTextures[m][c],TEXTURE_FILTER_BILINEAR);
    }
    State s;
    if(!capture&&!benchmark){std::ifstream settings(root/".local/projectile-inspector.cfg");float light,exposure;int bg;
        if(settings>>light>>exposure>>bg){s.light=std::isfinite(light)?light:.25F;s.exposure=std::clamp(exposure,.2F,3.F);s.background=std::clamp(bg,0,2);}}
    RenderTexture2D canvas{},stage{};int width=0,height=0,frame=0;float avgMs=0;double measuredMs=0;int measuredFrames=0;
    const char* captures[]{"01-material","02-front","03-side","04-back","05-clay","06-collision","07-overlay","08-ring-hole","09-ring-contact","10-textures","11-dark","12-light"};
    while(!WindowShouldClose()){
        auto start=std::chrono::steady_clock::now();
        if(width!=GetScreenWidth()||height!=GetScreenHeight()){
            width=GetScreenWidth();height=GetScreenHeight();
            if(canvas.id)UnloadRenderTexture(canvas);if(stage.id)UnloadRenderTexture(stage);
            canvas=LoadRenderTexture(width,height);stage=LoadRenderTexture(width-320,height-160);
        }
        if(capture){s=State{};
            if(frame==1)s.yaw=0,s.pitch=0;
            if(frame==2)s.yaw=PI/2,s.pitch=0;
            if(frame==3)s.yaw=PI,s.pitch=0;
            if(frame==4)s.mode=1;
            if(frame==5)s.mode=3;
            if(frame==6)s.mode=4;
            if(frame==7||frame==8){s.mode=4;preset(s,frame==7?4:3);s.target={0,-.37F,0};s.distance=.58F;s.yaw=.18F;s.pitch=.08F;}
            if(frame==9)s.textures=true;
            if(frame==10)s.background=2;
            if(frame==11)s.background=1;
            if(frame>=12)s.yaw=(frame-12)*2*PI/24,s.pitch=.14F;
        }else{
            Vector2 mouse=GetMousePosition(),delta=GetMouseDelta();bool inStage=mouse.x>320&&mouse.y>90&&mouse.y<height-70;
            if(inStage&&!s.textures){
                if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)){s.yaw-=delta.x*.008F;s.pitch=std::clamp(s.pitch+delta.y*.006F,-1.45F,1.45F);}
                if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)){Camera3D c=cameraFor(s);Vector3 right=Vector3Normalize(Vector3CrossProduct(Vector3Subtract(c.target,c.position),c.up));
                    s.target=Vector3Add(s.target,Vector3Add(Vector3Scale(right,-delta.x*s.distance*.001F),Vector3Scale(c.up,delta.y*s.distance*.001F)));}
                s.distance=std::clamp(s.distance*std::pow(.88F,GetMouseWheelMove()),.12F,6.F);
            }
            if(IsKeyPressed(KEY_R))s=State{};
            if(IsKeyPressed(KEY_SPACE))s.turn=!s.turn;
            if(IsKeyPressed(KEY_T))s.textures=!s.textures;
            if(IsKeyPressed(KEY_P))s.probeOn=!s.probeOn;
            for(int i=0;i<5;++i)if(IsKeyPressed(KEY_ONE+i))s.mode=i,s.textures=false;
            if(IsKeyDown(KEY_Q))s.light-=GetFrameTime();if(IsKeyDown(KEY_E))s.light+=GetFrameTime();
            if(s.turn)s.yaw+=GetFrameTime()*.35F;
            if(s.probeOn){float step=GetFrameTime()*(IsKeyDown(KEY_LEFT_SHIFT)?.12F:.035F);
                s.probe.x+=(IsKeyDown(KEY_RIGHT)-IsKeyDown(KEY_LEFT))*step;
                s.probe.y+=(IsKeyDown(KEY_UP)-IsKeyDown(KEY_DOWN))*step;
                s.probe.z+=(IsKeyDown(KEY_PAGE_UP)-IsKeyDown(KEY_PAGE_DOWN))*step;
                if(IsKeyDown(KEY_EQUAL))s.radius=std::min(.15F,s.radius+step*.2F);
                if(IsKeyDown(KEY_MINUS))s.radius=std::max(.001F,s.radius-step*.2F);
            }
        }
        Camera3D camera=cameraFor(s);
        float values[3]{camera.position.x,camera.position.y,camera.position.z};
        SetShaderValue(shader,GetShaderLocation(shader,"eye"),values,SHADER_UNIFORM_VEC3);
        SetShaderValue(shader,GetShaderLocation(shader,"lightAngle"),&s.light,SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader,GetShaderLocation(shader,"exposure"),&s.exposure,SHADER_UNIFORM_FLOAT);
        int clay=s.mode==1||s.mode==2;SetShaderValue(shader,GetShaderLocation(shader,"clay"),&clay,SHADER_UNIFORM_INT);
        bool touching=s.probeOn&&collider.Touches(s.probe,s.radius);
        Color probeColor=touching?Color{236,106,82,255}:Color{47,196,167,255};
        BeginTextureMode(stage);
        Color top=s.background==0?Color{224,229,234,255}:s.background==1?Color{247,247,244,255}:Color{35,43,55,255};
        Color bottom=s.background==0?Color{188,199,210,255}:s.background==1?Color{222,224,224,255}:Color{15,23,34,255};
        ClearBackground(top);DrawRectangleGradientV(0,0,stage.texture.width,stage.texture.height,top,bottom);
        BeginMode3D(camera);
        if(s.mode!=3){DrawModel(model,{0,0,0},1,WHITE);}
        if(s.mode==2){DrawModelWires(model,{0,0,0},1,Fade(Color{30,57,69,255},.3F));}
        if(s.mode==3||s.mode==4){rlDisableBackfaceCulling();if(s.mode==4)rlDisableDepthTest();drawCollision(collider,s.mode==4);rlEnableDepthTest();rlEnableBackfaceCulling();}
        if(s.probeOn){DrawSphere(s.probe,s.radius,Fade(probeColor,.28F));DrawSphereWires(s.probe,s.radius,12,20,probeColor);}
        EndMode3D();
        EndTextureMode();
        BeginTextureMode(canvas);ClearBackground({238,240,242,255});
        DrawRectangle(0,0,300,height,panel);
        text("PROJECTILE / ATELIER",24,27,14,gold);text("KUNAI",24,62,42,{242,245,248,255});
        text("01   /   SILVER FORGED",25,116,14,{154,169,183,255});
        DrawLine(24,154,276,154,{53,65,78,255});
        text("Type 04  -  one authored variant",24,173,14,{178,191,205,255});
        label("INSPECT",214);
        const char* modes[]{"1   Material","2   Clay / geometry","3   Wireframe","4   Collision only","5   Collision overlay"};
        for(int i=0;i<5;++i)if(button(modes[i],{24.F,239.F+i*42,252,36},s.mode==i&&!s.textures)&&!capture)s.mode=i,s.textures=false;
        if(button("T   Texture channels",{24,458,252,36},s.textures)&&!capture)s.textures=!s.textures;
        label("STUDIO",517);
        if(button("Neutral",{24,542,80,36},s.background==0)&&!capture)s.background=0;
        if(button("Light",{110,542,77,36},s.background==1)&&!capture)s.background=1;
        if(button("Dark",{193,542,83,36},s.background==2)&&!capture)s.background=2;
        text("Q / E   Rotate lighting",25,591,14,{176,189,202,255});
        text(TextFormat("Exposure   %.2f",s.exposure),25,617,14,{176,189,202,255});
        if(button("-",{180,611,42,30})&&!capture)s.exposure=std::max(.2F,s.exposure-.1F);
        if(button("+",{231,611,45,30})&&!capture)s.exposure=std::min(3.F,s.exposure+.1F);
        label("CONTACT PROBE",666);
        if(button(s.probeOn?"P   Probe enabled":"P   Enable probe",{24,691,252,36},s.probeOn)&&!capture)s.probeOn=!s.probeOn;
        const char* probes[]{"Tip","Blade","Grip","Ring","Hole"};
        for(int i=0;i<5;++i){float px=24+(i%3)*85,py=737+(i/3)*38;
            if(button(probes[i],{px,py,79,32})&&!capture)preset(s,i);}
        text("Arrows XY   PgUp / PgDn Z",24,825,13,{152,170,186,255});
        text("+ / - radius    Shift: faster",24,847,13,{152,170,186,255});
        text("R reset    Space turntable",24,height-50.F,13,{152,170,186,255});
        text("F12 save review image",24,height-28.F,13,{152,170,186,255});

        text("Independent asset review",332,24,26,ink);
        text("REAL GEOMETRY   /   SEPARATE TEXTURES   /   SHARED COLLISION",333,59,12,muted);
        DrawRectangleRounded({width-232.F,28,208,27},.4F,8,{233,222,204,255});
        text("PENDING HUMAN REVIEW",width-220.F,35,12,{126,92,46,255});
        if(!s.textures){
            DrawTextureRec(stage.texture,{0,0,static_cast<float>(stage.texture.width),-static_cast<float>(stage.texture.height)},{320,90},WHITE);
            text(s.mode==3?"COLLISION / CLOSED PARTS":s.mode==4?"ACTUAL QUERY MESH / OVERLAY":s.mode==1?"CLAY / NO TEXTURES":s.mode==2?"MESH TOPOLOGY":"FORGED STEEL / WOVEN GRIP",342,110,13,s.background==2?LIGHTGRAY:muted);
            if(!capture){
                if(CheckCollisionPointRec(GetMousePosition(),{338,143,63,28})&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))s.yaw=0,s.pitch=0;
                if(CheckCollisionPointRec(GetMousePosition(),{409,143,63,28})&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))s.yaw=PI/2,s.pitch=0;
                if(CheckCollisionPointRec(GetMousePosition(),{480,143,63,28})&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))s.yaw=PI,s.pitch=0;
            }
            for(int i=0;i<3;++i){DrawRectangleRounded({338.F+71*i,143,63,28},.2F,4,Fade(WHITE,.7F));text(i==0?"Front":i==1?"Side":"Back",348.F+71*i,149,14,ink);}
            if(s.probeOn){
                DrawRectangleRounded({width-335.F,height-198.F,303,100},.12F,6,Fade(panel,.92F));
                text(touching?"CONTACT / intersecting":"CLEAR / no contact",width-317.F,height-183.F,18,probeColor);
                text(TextFormat("Radius %.4f    Gap %.4f",s.radius,std::sqrt(collider.DistanceSquared(s.probe))-s.radius),width-317.F,height-151.F,14,{197,209,221,255});
                text(TextFormat("XYZ  %.3f  %.3f  %.3f",s.probe.x,s.probe.y,s.probe.z),width-317.F,height-128.F,13,{197,209,221,255});
            }
        }else{
            for(int c=0;c<5;++c){Rectangle r{338.F+c*(width-364.F)/5,108,(width-374.F)/5,34};
                bool hover=CheckCollisionPointRec(GetMousePosition(),r);DrawRectangleRounded(r,.15F,4,c==s.channel?ink:Color{216,222,228,255});
                text(channelNames[c],r.x+9,r.y+11,12,c==s.channel?WHITE:ink);
                if(hover&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&!capture)s.channel=c;
            }
            float side=std::min((width-390.F)/2,height-300.F);
            for(int m=0;m<2;++m){float tx=340+m*(side+28),ty=196;
                text(m?"02 / INDIGO WOVEN BINDING":"01 / FORGED STEEL",tx,164,16,ink);
                DrawTexturePro(channelTextures[m][s.channel],{0,0,2048,2048},{tx,ty,side,side},{0,0},0,WHITE);
                text("2048 x 2048  /  independently authored PNG",tx,ty+side+18,13,muted);
            }
            text("Color carries pigment. Lighting and reflections are evaluated in 3D.",340,height-122.F,16,ink);
        }
        DrawRectangle(320,height-70,width-320,70,{247,248,249,255});
        text(TextFormat("7 closed parts    /    %i triangles    /    2 material sets",collider.TriangleCount()),340,height-52.F,14,ink);
        text("LMB orbit   |   RMB pan   |   Wheel zoom   |   No game or save data loaded",340,height-28.F,13,muted);
        if(!capture)text(TextFormat("%.1f ms",avgMs),width-92.F,height-49.F,15,muted);
        EndTextureMode();
        BeginDrawing();ClearBackground(BLACK);DrawTextureRec(canvas.texture,{0,0,static_cast<float>(width),-static_cast<float>(height)},{0,0},WHITE);EndDrawing();
        if(capture){std::string name=frame<12?captures[frame]:TextFormat("turntable_%02i",frame-12);saveTexture(canvas,output/(name+".png"));if(++frame>=36)break;}
        else if(IsKeyPressed(KEY_F12))saveTexture(canvas,output/("manual_"+std::to_string(static_cast<long long>(GetTime()*1000))+".png"));
        float ms=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-start).count();avgMs=avgMs==0?ms:avgMs*.95F+ms*.05F;
        if(benchmark){s.yaw+=.015F;if(frame>=60){measuredMs+=ms;++measuredFrames;}if(++frame>=300)break;}
    }
    if(!capture&&!benchmark){std::ofstream settings(root/".local/projectile-inspector.cfg");settings<<s.light<<' '<<s.exposure<<' '<<s.background<<'\n';}
    if(capture){std::ofstream report(output/"capture-report.txt");report<<"Asset: kunai_silver\nHuman acceptance: PENDING\nImported meshes: "<<model.meshCount<<"\nCollision triangles: "<<collider.TriangleCount()<<"\nCaptured: 12 review images + 24 turntable frames\n";}
    if(benchmark){std::ofstream report(output/"benchmark.txt");report<<"1440x960 hidden window, one asset, no FPS limiter; 60 warmup + 240 measured frames.\nMean whole-frame wall time including presentation: "<<measuredMs/measuredFrames<<" ms\nNot an 8192-projectile game benchmark.\n";std::cout<<"Mean frame: "<<measuredMs/measuredFrames<<" ms\n";}
    for(auto& row:channelTextures)for(auto t:row)UnloadTexture(t);
    // LoadModel 的材质可能共享纹理；统一去重释放，避免重复删除同一 GPU 纹理。
    for(unsigned id:ownedTextures)rlUnloadTexture(id);
    for(int i=0;i<model.materialCount;++i){model.materials[i].shader={};for(int k=0;k<=MATERIAL_MAP_BRDF;++k)model.materials[i].maps[k].texture={};}
    UnloadModel(model);UnloadShader(shader);UnloadRenderTexture(stage);UnloadRenderTexture(canvas);
    if(font.texture.id!=GetFontDefault().texture.id)UnloadFont(font);CloseWindow();
    std::cout<<"Independent review completed. Human acceptance remains pending.\n";return 0;
}catch(const std::exception& e){std::cerr<<"Review error: "<<e.what()<<'\n';if(IsWindowReady())CloseWindow();return 1;}}
