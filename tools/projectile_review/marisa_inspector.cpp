// 本文件提供魔理沙四种造型及十二个档位的独立查看、动画冻结、碰撞探针和截图。
#include "resources/projectile_paths.hpp"
#include "collision.hpp"
#include "marisa_asset.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <array>
#include <memory>
#include <iostream>
#include <algorithm>
#include <cmath>
namespace {
const char* names[]={"marisa_primary","marisa_missile","marisa_laser","marisa_explosion"};
const int counts[]={1,4,3,4};
const float missileScale[]={.65F,.75F,.9F,1};
const float laserScale[]={.5F,.75F,1};
const float explosionWidth[]={32,42,48,48};
using Instance=review::MarisaInstance;
void integer(Shader s,const char*name,int v){SetShaderValue(s,GetShaderLocation(s,name),&v,SHADER_UNIFORM_INT);}
void real(Shader s,const char*name,float v){SetShaderValue(s,GetShaderLocation(s,name),&v,SHADER_UNIFORM_FLOAT);}
// 调用方持有 GL 上下文；本工具仅借用 shader，不对模型默认材质的资源所有权做转移。
void drawModel(Model model,Shader shader,Vector3 position,float scale){for(int i=0;i<model.materialCount;i++)model.materials[i].shader=shader;DrawModel(model,position,scale,WHITE);}
}
int main(int argc,char**argv){try{
 std::filesystem::path root=argc>1?argv[1]:".";bool capture=argc>2&&std::string(argv[2])=="--capture";SetConfigFlags(capture?FLAG_WINDOW_HIDDEN:FLAG_MSAA_4X_HINT);InitWindow(1440,900,"Marisa independent projectile review");SetTargetFPS(capture?0:60);
 std::array<Model,4> body,glow;std::array<std::unique_ptr<review::CollisionAsset>,4> collision;
 for(int k=0;k<4;k++){auto dir=root/"assets/projectiles"/names[k];body[k]=LoadModel((dir/(std::string(names[k])+".gltf")).string().c_str());glow[k]=LoadModel((dir/"glow.gltf").string().c_str());if(!body[k].meshCount||!glow[k].meshCount)throw std::runtime_error("Missing Marisa mesh");if(k!=2)collision[k]=std::make_unique<review::CollisionAsset>(dir/"collision.bin");}
 Shader shader=LoadShader((review::ProjectileShaderDirectory(root)/"marisa.vs").string().c_str(),(review::ProjectileShaderDirectory(root)/"marisa.fs").string().c_str());if(!IsShaderValid(shader))throw std::runtime_error("Marisa shader invalid");
 Texture2D reference=LoadTexture((root/"docs/images/projectiles/marisa-shots.png").string().c_str());RenderTexture2D target=LoadRenderTexture(1440,900);
 Font font=LoadFontEx((root/".deps/raylib-5.5/examples/text/resources/anonymous_pro_bold.ttf").string().c_str(),28,nullptr,0);
 auto label=[&](const char*text,int x,int y,int size,Color c){DrawTextEx(font,text,{float(x),float(y)},float(size),.2F,c);};
 Instance selected;int mode=0,frame=0;bool all=true,tiers=false,grid=false,effects=true,light=false,paused=true,gameAlpha=false;float yaw=.25F,pitch=.12F,distance=9;Vector3 probe{};
 const char* shots[]={"overview","front","side","back","clay","body","effects","collision","collision-only","tiers","light","frame00","frame06","frame12","frame19","game-alpha"};
 while(!WindowShouldClose()){
  if(capture){grid=frame==64;selected={};selected.kind=grid?0:frame/16;int s=frame%16;all=s==0;tiers=s==9;mode=s==4?3:s==6?4:s==7?1:s==8?2:0;effects=s!=5;light=s==10;gameAlpha=s==15;yaw=s==2?PI/2:s==3?PI:0;pitch=0;distance=all?10:tiers?10:6;selected.age=(s==11?0:s==12?6:s==13?12:s==14?19:6)/60.F;}
  else{
   if(IsKeyPressed(KEY_TAB)){selected.kind=(selected.kind+1)%4;selected.tier=0;}
   if(IsKeyPressed(KEY_Y)){grid=!grid;all=tiers=false;}if(IsKeyPressed(KEY_C))selected.tier=(selected.tier+1)%counts[selected.kind];if(IsKeyPressed(KEY_V)){all=!all;tiers=grid=false;}if(IsKeyPressed(KEY_T)){tiers=!tiers;all=grid=false;}
   if(IsKeyPressed(KEY_ONE))mode=3;if(IsKeyPressed(KEY_TWO)){mode=0;effects=false;}if(IsKeyPressed(KEY_THREE)){mode=0;effects=true;}if(IsKeyPressed(KEY_FOUR))mode=(mode+1)%3;if(IsKeyPressed(KEY_FIVE))mode=4;
   if(IsKeyPressed(KEY_G))effects=!effects;if(IsKeyPressed(KEY_B))light=!light;if(IsKeyPressed(KEY_A))gameAlpha=!gameAlpha;if(IsKeyPressed(KEY_SPACE))paused=!paused;if(IsKeyPressed(KEY_R))selected.age=.1F;
   if(IsKeyPressed(KEY_COMMA))selected.age=std::max(0.F,selected.age-1/60.F);if(IsKeyPressed(KEY_PERIOD))selected.age+=1/60.F;
   if(!paused)selected.age=std::fmod(selected.age+GetFrameTime(),selected.kind==3?20/60.F:2.5F);
   if(IsKeyDown(KEY_U))selected.trailLengthScale=std::max(.4F,selected.trailLengthScale-.015F);if(IsKeyDown(KEY_I))selected.trailLengthScale=std::min(2.F,selected.trailLengthScale+.015F);
   if(IsKeyDown(KEY_J))selected.trailWidthScale=std::max(.5F,selected.trailWidthScale-.015F);if(IsKeyDown(KEY_K))selected.trailWidthScale=std::min(1.5F,selected.trailWidthScale+.015F);
   if(IsKeyPressed(KEY_ZERO))selected.trailLengthScale=selected.trailWidthScale=1;
   if(IsKeyDown(KEY_Q))selected.length=std::max(.1F,selected.length-.03F);if(IsKeyDown(KEY_E))selected.length=std::min(32.F,selected.length+.03F);
   if(IsKeyDown(KEY_Z))selected.radius=std::max(.02F,selected.radius-.001F);if(IsKeyDown(KEY_X))selected.radius=std::min(.5F,selected.radius+.001F);
   if(IsKeyDown(KEY_LEFT))probe.x-=.01F;if(IsKeyDown(KEY_RIGHT))probe.x+=.01F;if(IsKeyDown(KEY_UP))probe.y+=.01F;if(IsKeyDown(KEY_DOWN))probe.y-=.01F;if(IsKeyDown(KEY_PAGE_UP))probe.z+=.01F;if(IsKeyDown(KEY_PAGE_DOWN))probe.z-=.01F;
   if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)){auto d=GetMouseDelta();yaw-=d.x*.006F;pitch=Clamp(pitch+d.y*.006F,-1.4F,1.4F);}distance=Clamp(distance-GetMouseWheelMove()*.4F,1,30);
  }
  if(grid)distance=11;
  Camera3D camera{{distance*sinf(yaw)*cosf(pitch),distance*sinf(pitch),distance*cosf(yaw)*cosf(pitch)},{0,0,0},{0,1,0},40,CAMERA_PERSPECTIVE};
  SetShaderValue(shader,GetShaderLocation(shader,"eye"),&camera.position,SHADER_UNIFORM_VEC3);
  BeginTextureMode(target);ClearBackground(light?Color{175,185,199,255}:Color{13,18,29,255});BeginMode3D(camera);
  int n=grid?12:all?4:tiers?counts[selected.kind]:1;bool touched=false;
  for(int index=0;index<n;index++){
   Instance item=selected;item.kind=all?index:selected.kind;if(all&&item.kind==2)item.age=1;item.tier=tiers?index:selected.tier;int k=item.kind,t=item.tier;
   if(all)item.tier=t=0;if(grid){k=index==0?0:index<=4?1:index<=7?2:3;t=index==0?0:index<=4?index-1:index<=7?index-5:index-8;item.kind=k;item.tier=t;item.age=k==2?1:.1F;}float size=k==1?missileScale[t]:k==3?explosionWidth[t]/32:1;size*=item.scale;
   item.position={(index-(n-1)*.5F)*1.9F,k==2?-item.length/2:0,0};
   if(tiers&&k==3)size*=.45F;
   if(grid){size*=k==3?.42F:.6F;item.length=1.2F;if(k==2){size=1;item.radius*=.6F;}item.position={float(index%4-1.5F)*1.8F,float(1-index/4)*2.1F-(k==2?.6F:0),0};}
   auto state=review::SampleMarisa(item);float opacity=gameAlpha?state.nativeOpacity*(51/255.F):state.opacity;float growth=state.growth;
   SetShaderValue(shader,GetShaderLocation(shader,"tintMultiplier"),&item.color,SHADER_UNIFORM_VEC3);
   integer(shader,"kind",k);integer(shader,"clay",mode==3);real(shader,"beamLength",item.length);real(shader,"radius",item.radius);real(shader,"phase",item.age);real(shader,"growth",growth);real(shader,"strength",item.strength);real(shader,"trailLengthScale",item.trailLengthScale);real(shader,"trailWidthScale",item.trailWidthScale);real(shader,"opacity",mode==3?1:opacity);
   if(mode!=2&&mode!=4){integer(shader,"effect",0);if(k==3||opacity<1)rlDisableDepthMask();drawModel(body[k],shader,item.position,size);rlDrawRenderBatchActive();rlEnableDepthMask();}
   if(effects&&mode!=2&&mode!=3){integer(shader,"effect",1);real(shader,"radius",item.radius*state.haloScale);rlDrawRenderBatchActive();if(k<2)rlEnableBackfaceCulling();else rlDisableBackfaceCulling();rlDisableDepthMask();BeginBlendMode(BLEND_ADDITIVE);drawModel(glow[k],shader,item.position,size);EndBlendMode();rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableBackfaceCulling();}
   if(k==2){Vector3 end=Vector3Add(item.position,{0,item.length,0});Vector3 nearest{item.position.x,Clamp(probe.y,item.position.y,end.y),0};if(!all&&!tiers&&!grid)touched=Vector3Distance(probe,nearest)<=item.radius+.025F;
    if(mode==1||mode==2){rlDrawRenderBatchActive();rlDisableDepthTest();DrawCapsuleWires(item.position,end,item.radius,8,12,GREEN);rlDrawRenderBatchActive();rlEnableDepthTest();}
   }else{if(!all&&!tiers&&!grid)touched=collision[k]->Touches(Vector3Scale(Vector3Subtract(probe,item.position),1/size),.025F/size);
    if(mode==1||mode==2){rlDrawRenderBatchActive();rlDisableDepthTest();for(auto&p:collision[k]->Parts())for(auto&tri:p.triangles){auto a=Vector3Add(item.position,Vector3Scale(tri.a,size)),b=Vector3Add(item.position,Vector3Scale(tri.b,size)),c=Vector3Add(item.position,Vector3Scale(tri.c,size));DrawLine3D(a,b,GREEN);DrawLine3D(b,c,GREEN);DrawLine3D(c,a,GREEN);}rlDrawRenderBatchActive();rlEnableDepthTest();}
   }
  }
  if((selected.kind==3&&selected.age>=20/60.F)||(selected.kind==2&&selected.age>=selected.fadeAt+.5F))touched=false;
  if(!all&&!tiers&&!grid)DrawSphere(probe,.025F,touched?RED:GREEN);EndMode3D();Color text=light?BLACK:RAYWHITE;
  label(names[selected.kind],24,18,28,text);label("TAB kind | C tier | V four shapes | T tiers | Y all 12 | drag orbit | wheel zoom",24,55,20,text);label("1 clay | 2 body | 3 final | 4 collision/only | 5 effects | G glow | B background | A game alpha",24,82,18,text);label("Space play | ,/. step | R reset | Q/E beam length | Z/X radius | arrows + PgUp/Dn probe",24,108,18,text);
  label(TextFormat("Tier %d / %d    frame %.1f    %s",selected.tier+1,counts[selected.kind],selected.age*60,touched?"CONTACT":"CLEAR"),24,136,20,text);
  if(grid){for(int i=0;i<12;i++){int k=i==0?0:i<=4?1:i<=7?2:3;int t=i==0?0:i<=4?i-1:i<=7?i-5:i-8;Vector2 p=GetWorldToScreen({float(i%4-1.5F)*1.8F,float(1-i/4)*2.1F-.9F,0},camera);label(TextFormat("%s %d",k==0?"Primary":k==1?"Missile":k==2?"Laser":"Explosion",t+1),int(p.x)-60,int(p.y),18,text);}}
  label(TextFormat("Trail: U/I length %.2f | J/K width %.2f | 0 reset",selected.trailLengthScale,selected.trailWidthScale),24,166,18,text);
  DrawTextureEx(reference,{24,635},0,.33F,WHITE);EndTextureMode();BeginDrawing();ClearBackground(BLACK);DrawTextureRec(target.texture,{0,0,1440,-900},{0,0},WHITE);EndDrawing();
  if(capture||IsKeyPressed(KEY_F12)){auto dir=root/"output/projectile-review/marisa";std::filesystem::create_directories(dir);Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);auto path=dir/(capture?(grid?std::string("all-12-variants"):std::string(names[selected.kind])+"-"+shots[frame%16])+".png":"manual.png");ExportImage(image,path.string().c_str());UnloadImage(image);}
  if(capture&&++frame==65)break;
 }
 for(auto&m:body)UnloadModel(m);for(auto&m:glow)UnloadModel(m);UnloadShader(shader);UnloadFont(font);UnloadTexture(reference);UnloadRenderTexture(target);CloseWindow();return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
