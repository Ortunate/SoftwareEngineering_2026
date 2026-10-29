// 本文件独立查看七款道具并导出固定步长旋转帧；GPU 资源在窗口关闭前释放。
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <array>
#include <filesystem>
#include <string>
#include <set>
// argv[1] 为仓库根目录；argv[2]=--capture 时输出七款和总览各 100 帧，不推进游戏。
int main(int argc,char**argv){
 const std::filesystem::path root=argc>1?argv[1]:".";
 const bool capture=argc>2;const char*names[]={"power_small","point","power_large","bomb","full_power","life","star"};
 const char*labels[]={"Small P","Point","Large P","Bomb","Full Power F","Life 1UP","Star"};
 SetConfigFlags(capture?FLAG_WINDOW_HIDDEN:FLAG_MSAA_4X_HINT);InitWindow(1280,720,"Items review");SetTargetFPS(60);
 std::array<Model,7>models{};for(int i=0;i<7;i++){models[i]=LoadModel((root/"assets/items"/names[i]/(std::string(names[i])+".gltf")).string().c_str());if(!models[i].meshCount)return 1;}
 int selected=0,mode=0,frame=0;bool all=true,play=true;float angle=0;
 RenderTexture2D target=LoadRenderTexture(1280,720);
 while(!WindowShouldClose()){
  int sequence=capture?frame/100:0;bool grid=capture?sequence==7:all;int width=grid?1280:512,height=grid?720:512;
  if(capture){selected=sequence%7;angle=(frame%100)*3.6F;}
  else{if(IsKeyPressed(KEY_TAB))selected=(selected+1)%7;if(IsKeyPressed(KEY_V))all=!all;if(IsKeyPressed(KEY_SPACE))play=!play;if(IsKeyPressed(KEY_ONE))mode=0;if(IsKeyPressed(KEY_TWO))mode=1;if(IsKeyPressed(KEY_THREE))mode=2;if(IsKeyPressed(KEY_R))angle=0;if(IsKeyPressed(KEY_F))angle=0;if(IsKeyPressed(KEY_S))angle=90;if(IsKeyPressed(KEY_B))angle=180;if(play)angle+=GetFrameTime()*90;}
  if(target.texture.width!=width||target.texture.height!=height){UnloadRenderTexture(target);target=LoadRenderTexture(width,height);}
  BeginTextureMode(target);ClearBackground({22,26,36,255});
  for(int i=0;i<7;i++){
   if(!grid&&i!=selected)continue;
   int x=grid?(i%4)*320:0,y=grid?(i/4)*360:0,cw=grid?320:512,ch=grid?360:512;
   Camera3D cam{{0,0,4},{0,0,0},{0,1,0},grid?1.8F:1.65F,CAMERA_ORTHOGRAPHIC};
   // 每格在相同投影下旋转自身，不绕总览中心公转；用视口限制保持各格比例。
   rlDrawRenderBatchActive();rlViewport(x,height-y-ch,cw,ch);BeginMode3D(cam);
   rlMatrixMode(RL_PROJECTION);rlLoadIdentity();double half=cam.fovy/2;rlOrtho(-half*cw/ch,half*cw/ch,-half,half,.01,100);rlMatrixMode(RL_MODELVIEW);
   Matrix rotation=MatrixRotateY(angle*DEG2RAD);models[i].transform=rotation;
   if(mode==2)DrawModelWires(models[i],{0,0,0},1,SKYBLUE);
   else if(mode==1){for(int m=0;m<models[i].meshCount;m++)if(models[i].meshMaterial[m]==0)DrawMesh(models[i].meshes[m],models[i].materials[0],rotation);}
   else DrawModel(models[i],{0,0,0},1,WHITE);
   EndMode3D();rlDrawRenderBatchActive();rlViewport(0,0,width,height);
   DrawText(labels[i],x+12,y+12,20,RAYWHITE);
   DrawText(TextFormat("%05.1f deg",fmodf(angle,360)),x+12,y+38,16,GRAY);
  }
  DrawText(capture?"Local Y axis / 360 degrees / 4 seconds":"TAB select | V grid | SPACE pause | 1 texture 2 solid 3 wire | F/S/B views | R reset",12,height-26,grid?16:14,GRAY);
  EndTextureMode();
  if(capture){auto dir=root/"output/projectile-review/items"/(sequence==7?"overview":names[selected]);std::filesystem::create_directories(dir);Image im=LoadImageFromTexture(target.texture);ImageFlipVertical(&im);ExportImage(im,(dir/(std::to_string(frame%100)+".png")).string().c_str());UnloadImage(im);if(++frame==800)break;}
  else{BeginDrawing();ClearBackground(BLACK);DrawTexturePro(target.texture,{0,0,(float)width,-(float)height},{grid?0.F:280.F,0,grid?1280.F:720.F,720},{0,0},0,WHITE);EndDrawing();}
 }
 UnloadRenderTexture(target);std::set<unsigned>textures;
 for(auto&m:models){for(int i=0;i<m.materialCount;i++)for(int j=0;j<12;j++){auto t=m.materials[i].maps[j].texture;if(t.id>1&&textures.insert(t.id).second)UnloadTexture(t);}UnloadModel(m);}CloseWindow();
}
