// 本文件使用十款现有敌弹审阅传入模型收缩显现；不启动游戏或修改已验收模型。
#include "spawn_renderer.hpp"
#include "collection_renderer.hpp"
#include "raymath.h"
#include <filesystem>
#include <array>
#include <iostream>
// GPU 资源在窗口内释放。每次 Draw 直接传入对应的 Model，不绘制替代光圈。
int main(int argc,char**argv){try{
 std::filesystem::path root=argc>1?argv[1]:".";bool capture=argc>2;
 SetConfigFlags(capture?FLAG_WINDOW_HIDDEN:FLAG_MSAA_4X_HINT);InitWindow(1440,900,"Shared spawn on projectiles");SetTargetFPS(capture?0:60);
 {
 review::SpawnRenderer spawn(root);std::array<Model,10> models{};
 for(int i=0;i<10;i++){auto name=review::ProjectileKinds()[i].directory;models[i]=LoadModel((root/"assets/projectiles"/name/(std::string(name)+".gltf")).string().c_str());if(!models[i].meshCount)throw std::runtime_error("Input model load failed");}
 RenderTexture2D target=LoadRenderTexture(1440,900);
 int frame=0,selected=0;bool all=true,light=false,paused=false,effect=true;float seconds=0,yaw=.12F,pitch=.12F,distance=8,duration=.65F;
 while(!WindowShouldClose()){
  if(capture)seconds=frame/30.F;
  else{if(!paused)seconds+=GetFrameTime();if(IsKeyPressed(KEY_SPACE))paused=!paused;if(IsKeyPressed(KEY_R))seconds=0;if(IsKeyPressed(KEY_TAB))selected=(selected+1)%10;if(IsKeyPressed(KEY_V))all=!all;if(IsKeyPressed(KEY_B))light=!light;if(IsKeyPressed(KEY_G))effect=!effect;
   if(IsKeyPressed(KEY_ONE)){duration=10.F/60;seconds=0;}if(IsKeyPressed(KEY_TWO)){duration=16.F/60;seconds=0;}if(IsKeyPressed(KEY_THREE)){duration=32.F/60;seconds=0;}
   if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)){auto d=GetMouseDelta();yaw-=d.x*.006F;pitch=Clamp(pitch+d.y*.006F,-1.3F,1.3F);}distance=Clamp(distance-GetMouseWheelMove()*.3F,2.F,15.F);
  }
  float age=capture?seconds:std::fmod(seconds,2.4F);
  Camera3D camera{{distance*sinf(yaw)*cosf(pitch),distance*sinf(pitch),distance*cosf(yaw)*cosf(pitch)},{0,0,0},{0,1,0},42,CAMERA_PERSPECTIVE};
  std::array<review::CollectionInstance,10> bullets{};int count=all?10:1;
  for(int i=0;i<count;i++){auto& b=bullets[i];b.type=all?i:selected;b.transform.appearance=review::AppearanceFor(b.type,b.type==6||b.type==8?1:b.type==9?0:2);b.transform.scale=.7F;b.transform.position=all?Vector3{(i%5-2)*1.65F,(.5F-i/5)*1.8F,0}:Vector3{};}
  Color bg=light?Color{100,110,125,255}:Color{15,21,33,255};
  BeginTextureMode(target);ClearBackground(bg);BeginMode3D(camera);
  for(int i=0;i<count;i++){auto& b=bullets[i];review::SpawnParams p;p.position=b.transform.position;p.scale=.40F;p.age=age;p.duration=duration;p.tint=b.transform.appearance.bodyColor;p.projectileMask=true;
   if(effect)spawn.Draw(models[b.type],p);else DrawModel(models[b.type],p.position,p.scale,WHITE);
  }
  EndMode3D();
  DrawText("SHARED SPAWN / INPUT MODEL SHRINKS AND BECOMES OPAQUE",25,22,26,RAYWHITE);
  DrawText("TAB select | V overview | SPACE pause | R restart | G effect | B background | drag orbit | wheel zoom",25,65,18,LIGHTGRAY);
  DrawText(TextFormat("age %.2f s | duration %.3f s | 1/2/3 = 10/16/32 frames | visual only, collision unchanged",age,duration),25,98,18,LIGHTGRAY);
  for(int i=0;i<count;i++){auto screen=GetWorldToScreen(bullets[i].transform.position,camera);DrawText(review::ProjectileKinds()[bullets[i].type].directory,screen.x-70,screen.y+80,16,LIGHTGRAY);}
  EndTextureMode();BeginDrawing();ClearBackground(BLACK);DrawTextureRec(target.texture,{0,0,1440,-900},{0,0},WHITE);EndDrawing();
  if(capture||IsKeyPressed(KEY_F12)){auto out=root/"output/projectile-review/effects-3d"/(capture?"spawn-models-frames":"manual");std::filesystem::create_directories(out);Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);if(!ExportImage(image,(out/(std::to_string(capture?frame:0)+".png")).string().c_str()))throw std::runtime_error("Spawn capture failed");UnloadImage(image);}
  if(capture&&++frame==90)break;
 }
 for(auto model:models)UnloadModel(model);UnloadRenderTexture(target);
 }
 CloseWindow();return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
