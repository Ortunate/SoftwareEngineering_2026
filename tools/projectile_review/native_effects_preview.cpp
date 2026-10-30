// 本文件用正式 ANM 播放器与原 DDS 输出显现/击破逐帧预览，不修改素材与游戏。
#include "resources/animation.hpp"
#include "resources/dds_image.hpp"
#include "render/animated_sprite.hpp"
#include "rlgl.h"
#include <filesystem>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <cmath>
struct Entry {int atlas,script,offset;std::string label;};
// argv[1] 为仓库根目录；每两帧采样一次，完整保留 60 Hz 脚本时间。
// 每组固定像素倍率，禁止逐帧适配尺寸，否则会抹掉原动画的放缩。
int main(int argc,char**argv){try{
 std::filesystem::path root=argc>1?argv[1]:".";auto output=root/"output/projectile-review/native-effects";std::filesystem::create_directories(output);
 SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(1100,1000,"Native effects export");SetTargetFPS(0);
 std::vector<koumakyou::anm::Archive> archives;std::vector<Texture2D> textures;
 for(auto name:{"etama3","etama4"}){auto base=root/"assets/runtime/battle/native";archives.push_back(koumakyou::anm::LoadArchive(base/(std::string(name)+".anm")));Image image=koumakyou::LoadOriginalDds(base/(std::string(name)+".dds"));textures.push_back(LoadTextureFromImage(image));UnloadImage(image);SetTextureFilter(textures.back(),TEXTURE_FILTER_BILINEAR);SetTextureWrap(textures.back(),TEXTURE_WRAP_REPEAT);}
 for(int group=0;group<2;group++){
  std::vector<Entry> entries;
  if(group==0){for(int script=14;script<=20;script++)for(int c=0;c<5;c++)entries.push_back({0,script,c,"etama3 / "+std::to_string(script)});for(int c=0;c<5;c++)entries.push_back({1,2,c,"etama4 / 2"});}
  else for(int script=3;script<=5;script++)entries.push_back({1,script,0,"etama4 / "+std::to_string(script)});
  std::vector<koumakyou::anm::Player> players(entries.size());
  std::vector<float> scales(entries.size(),1);
  // 先采样全段最大尺寸，为每格选定固定倍率；渲染时不逐帧归一化。
  for(size_t i=0;i<entries.size();i++){
   auto e=entries[i];koumakyou::anm::Player probe;probe.Start(archives[e.atlas],e.script,e.offset);float extent=1;
   for(int tick=0;tick<180;tick++){auto f=probe.Sample();extent=std::max(extent,std::max(std::abs(f.source.width*f.scale.x),std::abs(f.source.height*f.scale.y)));probe.Advance(1./60);}
   scales[i]=(group==0?72.F:260.F)/extent;
  }
  for(size_t i=0;i<entries.size();i++){auto e=entries[i];players[i].Start(archives[e.atlas],e.script,e.offset);}
  const int width=group==0?1100:1000,height=group==0?1000:430;
  auto target=LoadRenderTexture(width,height);auto folder=output/(group==0?"spawn-frames":"defeat-frames");std::filesystem::create_directories(folder);
  // 三秒窗口覆盖短暂效果与结束后的留白；不把显现结束后的常驻弹体误作光圈动画。
  for(int frame=0;frame<180;frame+=2){
   BeginDrawing();BeginTextureMode(target);ClearBackground({22,26,38,255});
   DrawText(group==0?"ORIGINAL BULLET SPAWN / 5 COLOR VARIANTS":"ORIGINAL SHARED DEFEAT EFFECTS",20,16,24,RAYWHITE);
   DrawText(TextFormat("60 Hz source | frame %d | %.2f sec",frame,frame/60.F),20,48,18,LIGHTGRAY);
   for(size_t i=0;i<entries.size();i++){
    auto e=entries[i];int col=group==0?i%5:i,row=group==0?i/5:0;
    int cell=group==0?210:320;Vector2 center{float(40+col*cell+cell/2),float(group==0?130+row*105:245)};
    DrawText(e.label.c_str(),center.x-cell/2+8,group==0?80+row*105:100,16,GRAY);
    if(group==0)DrawText(TextFormat("color %d",e.offset),center.x-cell/2+8,100+row*105,12,GRAY);
    koumakyou::DrawAnimatedSprite2D(textures[e.atlas],archives[e.atlas],players[i].Sample(),center,scales[i]);
   }
   rlDrawRenderBatchActive();EndTextureMode();EndDrawing();Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);bool ok=ExportImage(image,(folder/(std::to_string(frame/2)+".png")).string().c_str());UnloadImage(image);if(!ok)throw std::runtime_error("PNG export failed");
   for(auto& player:players)player.Advance(2./60);
  }
  UnloadRenderTexture(target);
 }
 for(auto texture:textures)UnloadTexture(texture);CloseWindow();return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
