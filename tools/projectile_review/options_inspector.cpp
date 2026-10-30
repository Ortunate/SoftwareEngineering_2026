// 本文件独立查看两款副机的实体、柔光和双侧旋转；显示边界不用于碰撞。
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <filesystem>
#include <cmath>
#include <array>
#include <iostream>

// 模型正面 +Z、上方 +Y；资源生命周期完全位于 OpenGL 上下文内。
int main(int argc,char** argv){
 const std::filesystem::path root=argc>1?argv[1]:".";const bool capture=argc>2;
 SetConfigFlags(capture?FLAG_WINDOW_HIDDEN:FLAG_MSAA_4X_HINT);InitWindow(1440,900,"Options 3D review");SetTargetFPS(60);
 const char* names[]={"reimu_option","marisa_option"};std::array<Model,2> models{};
 for(int i=0;i<2;i++){models[i]=LoadModel((root/"assets/projectiles"/names[i]/(std::string(names[i])+".gltf")).string().c_str());if(!models[i].meshCount){std::cerr<<"Model load failed\n";CloseWindow();return 1;}}
 Texture2D reference=LoadTexture((root/"docs/images/projectiles/options.png").string().c_str());
 Image white=GenImageColor(1,1,WHITE);Texture2D plain=LoadTextureFromImage(white);UnloadImage(white);
 // 径向透明度在独立贴图中归零，避免光晕出现矩形边界或透明硬壳。
 Image haloImage=GenImageColor(256,256,BLANK);
 for(int y=0;y<256;y++)for(int x=0;x<256;x++){float r=std::sqrt(std::pow((x-127.5F)/127.5F,2)+std::pow((y-127.5F)/127.5F,2));float a=std::pow(std::max(0.F,1-r*r),3);ImageDrawPixel(&haloImage,x,y,{255,255,255,(unsigned char)(a*110)});}
 Texture2D halo=LoadTextureFromImage(haloImage);UnloadImage(haloImage);
 RenderTexture2D target=LoadRenderTexture(1440,900);int selected=0,mode=2,frame=0;bool all=true,pair=false,glow=true,bounds=false,light=false,play=false,actual=false;float yaw=.3F,pitch=.15F,distance=3.4F,angle=0;
 const char* views[]={"overview","front","side","back","clay","body","light","pair"};
 while(!WindowShouldClose()){
  if(capture){selected=frame/8;int s=frame%8;all=s==0;pair=s==7;mode=s==4?0:s==5?1:2;light=s==6;yaw=s==2?PI/2:s==3?PI:0;pitch=s==0?.18F:0;}
  else{
   if(IsKeyPressed(KEY_TAB))selected=1-selected;if(IsKeyPressed(KEY_V))all=!all;if(IsKeyPressed(KEY_T))pair=!pair;
   if(IsKeyPressed(KEY_ONE))mode=0;if(IsKeyPressed(KEY_TWO))mode=1;if(IsKeyPressed(KEY_THREE))mode=2;if(IsKeyPressed(KEY_FOUR))bounds=!bounds;
   if(IsKeyPressed(KEY_G))glow=!glow;if(IsKeyPressed(KEY_B))light=!light;if(IsKeyPressed(KEY_A))actual=!actual;if(IsKeyPressed(KEY_SPACE))play=!play;
   if(play)angle+=GetFrameTime()*90;if(IsKeyPressed(KEY_R))angle=0;
   if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)){auto d=GetMouseDelta();yaw-=d.x*.006F;pitch=Clamp(pitch+d.y*.006F,-1.4F,1.4F);}distance=Clamp(distance-GetMouseWheelMove()*.25F,.6F,12.F);
  }
  Camera3D camera{{distance*sinf(yaw)*cosf(pitch),distance*sinf(pitch),distance*cosf(yaw)*cosf(pitch)},{0,0,0},{0,1,0},40,CAMERA_PERSPECTIVE};
  BeginTextureMode(target);ClearBackground(light?Color{185,193,205,255}:Color{15,20,32,255});BeginMode3D(camera);
  for(int i=0;i<2;i++){if(!all&&i!=selected)continue;for(int side=0;side<(pair?2:1);side++){
   Vector3 p{(all?(i-.5F)*1.7F:0)+(pair?(side-.5F)*1.F:0),0,0};float scale=actual?.5F:1.F;
   if(mode!=3){
    if(mode==0){for(int j=0;j<models[i].materialCount;j++){auto saved=models[i].materials[j].maps[MATERIAL_MAP_DIFFUSE].texture;models[i].materials[j].maps[MATERIAL_MAP_DIFFUSE].texture=plain;DrawMesh(models[i].meshes[j],models[i].materials[j],MatrixMultiply(MatrixScale(scale,scale,scale),MatrixTranslate(p.x,p.y,p.z)));models[i].materials[j].maps[MATERIAL_MAP_DIFFUSE].texture=saved;}}
    else DrawModelEx(models[i],p,{0,0,1},angle*(i==0&&side==1?-1:1),{scale,scale,scale},WHITE);
   }
   if(mode==2&&glow){BeginBlendMode(BLEND_ADDITIVE);rlDrawRenderBatchActive();rlDisableDepthMask();DrawBillboard(camera,halo,p,1.15F*scale,i==0?Color{255,220,220,180}:Color{90,235,240,210});rlDrawRenderBatchActive();rlEnableDepthMask();EndBlendMode();}
   if(bounds)DrawBoundingBox({Vector3Add(p,{-scale*.42F,-scale*.42F,-scale*.42F}),Vector3Add(p,{scale*.42F,scale*.42F,scale*.42F})},GREEN);
  }}
  EndMode3D();Color ink=light?BLACK:RAYWHITE;DrawText("OPTIONS / independent model review",25,20,28,ink);DrawText("TAB select | V overview | T pair | drag orbit | wheel zoom",25,65,20,ink);DrawText("1 clay | 2 body | 3 final | G glow | 4 display bounds (NO COLLISION)",25,95,20,ink);DrawText("SPACE preview spin | R reset | A game size | B background | F12 screenshot",25,125,20,ink);DrawText("Spin preview: 90 deg/s; original ANM timing pending",25,155,18,ink);DrawTexture(reference,25,660,WHITE);EndTextureMode();
  BeginDrawing();ClearBackground(BLACK);DrawTextureRec(target.texture,{0,0,1440,-900},{0,0},WHITE);EndDrawing();
  if(capture||IsKeyPressed(KEY_F12)){auto out=root/"output/projectile-review/options";std::filesystem::create_directories(out);Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);ExportImage(image,(out/(capture?std::string(names[selected])+"-"+views[frame%8]+".png":"manual.png")).string().c_str());UnloadImage(image);}
  if(capture&&++frame==16)break;
 }
 UnloadRenderTexture(target);UnloadTexture(reference);UnloadTexture(plain);UnloadTexture(halo);for(auto model:models)UnloadModel(model);CloseWindow();return 0;
}
