// 本文件独立审阅灵梦三款实体、光效和同源碰撞，不依赖游戏场景。
#include "collision.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <array>
#include <memory>
#include <iostream>
#include <cmath>
// 资源均在 OpenGL 上下文内创建及释放；模型以 +Y 为前方，单位为本地模型长度。
int main(int argc,char**argv){try{
 std::filesystem::path root=argc>1?argv[1]:".";bool capture=argc>2;SetConfigFlags(capture?FLAG_WINDOW_HIDDEN:FLAG_MSAA_4X_HINT);InitWindow(1440,900,"Reimu projectile review");SetTargetFPS(60);
 const char* names[]={"reimu_gold","reimu_cyan","reimu_needle"};std::array<Model,3> models;std::array<std::unique_ptr<review::CollisionAsset>,3> collisions;
 for(int i=0;i<3;i++){auto dir=root/"assets/projectiles"/names[i];models[i]=LoadModel((dir/(std::string(names[i])+".gltf")).string().c_str());if(!models[i].meshCount)throw std::runtime_error("model load failed");collisions[i]=std::make_unique<review::CollisionAsset>(dir/"collision.bin");}
 Model cone=LoadModel((root/"assets/projectiles/reimu_needle/glow.gltf").string().c_str());
 Shader coneShader=LoadShader((root/"tools/projectile_review/shaders/reimu_cone.vs").string().c_str(),(root/"tools/projectile_review/shaders/reimu_cone.fs").string().c_str());
 if(!cone.meshCount||!IsShaderValid(coneShader))throw std::runtime_error("cone glow load failed");
 for(int j=0;j<cone.materialCount;j++)cone.materials[j].shader=coneShader;
 Texture2D ref=LoadTexture((root/"docs/images/projectiles/reimu-shots.png").string().c_str());RenderTexture2D target=LoadRenderTexture(1440,900);
 Image whiteImage=GenImageColor(1,1,WHITE);Texture2D whiteTexture=LoadTextureFromImage(whiteImage);UnloadImage(whiteImage);
 int selected=0,mode=0,frame=0;bool all=true,glow=true,light=false,gameAlpha=false;float yaw=.3F,pitch=.15F,distance=5;Vector3 probe{0,0,0};
 const char* shots[]={"overview","front","side","back","collision","collision-only","light","game-alpha"};
 while(!WindowShouldClose()){
 if(capture){selected=frame/8;int s=frame%8;all=s==0;mode=s==4?1:s==5?2:0;light=s==6;gameAlpha=s==7;yaw=s==2?PI/2:s==3?PI:0;pitch=0;glow=true;}
 else {if(IsKeyPressed(KEY_ONE))mode=3;if(IsKeyPressed(KEY_TWO)){mode=0;glow=false;}if(IsKeyPressed(KEY_THREE)){mode=0;glow=true;}if(IsKeyPressed(KEY_TAB))selected=(selected+1)%3;if(IsKeyPressed(KEY_V))all=!all;if(IsKeyPressed(KEY_FOUR))mode=(mode+1)%3;if(IsKeyPressed(KEY_G))glow=!glow;if(IsKeyPressed(KEY_B))light=!light;if(IsKeyPressed(KEY_A))gameAlpha=!gameAlpha;
 if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)){auto d=GetMouseDelta();yaw-=d.x*.006F;pitch=Clamp(pitch+d.y*.006F,-1.4F,1.4F);}distance=Clamp(distance-GetMouseWheelMove()*.3F,1,15);
 float step=.01F;if(IsKeyDown(KEY_LEFT))probe.x-=step;if(IsKeyDown(KEY_RIGHT))probe.x+=step;if(IsKeyDown(KEY_UP))probe.y+=step;if(IsKeyDown(KEY_DOWN))probe.y-=step;if(IsKeyDown(KEY_PAGE_UP))probe.z+=step;if(IsKeyDown(KEY_PAGE_DOWN))probe.z-=step;}
 Camera3D camera{{distance*sinf(yaw)*cosf(pitch),distance*sinf(pitch),distance*cosf(yaw)*cosf(pitch)},{0,0,0},{0,1,0},40,CAMERA_PERSPECTIVE};
 BeginTextureMode(target);ClearBackground(light?Color{185,193,205,255}:Color{15,20,32,255});BeginMode3D(camera);
 for(int i=0;i<3;i++){if(!all&&i!=selected)continue;Vector3 pos{all?float(i-1)*1.5F:0,0,0};Color tint=WHITE;tint.a=gameAlpha?51:255;
 if(mode!=2){if(mode==3){for(int j=0;j<models[i].materialCount;j++){auto saved=models[i].materials[j].maps[MATERIAL_MAP_DIFFUSE].texture;models[i].materials[j].maps[MATERIAL_MAP_DIFFUSE].texture=whiteTexture;DrawMesh(models[i].meshes[j],models[i].materials[j],MatrixTranslate(pos.x,pos.y,pos.z));models[i].materials[j].maps[MATERIAL_MAP_DIFFUSE].texture=saved;}}else DrawModel(models[i],pos,1,tint);if(glow&&mode!=3){BeginBlendMode(BLEND_ADDITIVE);Color color=i==0?GOLD:i==1?SKYBLUE:VIOLET;color.a=gameAlpha?12:45;if(i==2){
 // 圆锥两面均可见，透明层保留深度测试但不写深度，避免遮挡内部针体。
 rlDrawRenderBatchActive();rlDisableBackfaceCulling();rlDisableDepthMask();color={210,185,255,static_cast<unsigned char>(gameAlpha?46:230)};DrawModel(cone,pos,1,color);rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableBackfaceCulling();
 }else DrawModel(models[i],pos,1.055F,color);EndBlendMode();}}
 if(mode==1||mode==2){rlDrawRenderBatchActive();rlDisableDepthTest();for(auto&p:collisions[i]->Parts())for(auto&t:p.triangles){auto a=Vector3Add(t.a,pos),b=Vector3Add(t.b,pos),c=Vector3Add(t.c,pos);DrawLine3D(a,b,GREEN);DrawLine3D(b,c,GREEN);DrawLine3D(c,a,GREEN);}rlDrawRenderBatchActive();rlEnableDepthTest();}
 }
 bool contact=collisions[selected]->Touches(probe,.025F);if(!all)DrawSphere(probe,.025F,contact?RED:GREEN);EndMode3D();
 DrawText(names[selected],25,20,28,light?BLACK:RAYWHITE);DrawText("TAB select | V all | drag orbit | wheel zoom | 4 collision / only | G glow | B background | A game alpha",25,62,18,light?BLACK:RAYWHITE);DrawText("Probe: arrows XY, PgUp/PgDn Z | red CONTACT / green CLEAR",25,89,18,light?BLACK:RAYWHITE);
 DrawTextureEx(ref,{25,715},0,.75F,WHITE);DrawText(contact?"CONTACT":"CLEAR",1100,30,22,contact?RED:GREEN);EndTextureMode();
 BeginDrawing();ClearBackground(BLACK);DrawTextureRec(target.texture,{0,0,1440,-900},{0,0},WHITE);EndDrawing();
 if(capture||IsKeyPressed(KEY_F12)){auto out=root/"output/projectile-review/reimu";std::filesystem::create_directories(out);Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);auto file=out/(capture?std::string(names[selected])+"-"+shots[frame%8]+".png":"manual.png");ExportImage(image,file.string().c_str());UnloadImage(image);}
 if(capture&&++frame==24)break;
 }
 UnloadModel(cone);UnloadShader(coneShader);UnloadTexture(whiteTexture);UnloadRenderTexture(target);UnloadTexture(ref);for(auto m:models)UnloadModel(m);CloseWindow();return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
