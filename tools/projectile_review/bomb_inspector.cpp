// 本文件独立审阅三种 Bomb 的单件/组合、分层光效与模型/游戏两套体积，绘制不改变游戏数据。
#include "resources/projectile_paths.hpp"
#include "collision.hpp"
#include "bomb_asset.hpp"
#include "raymath.h"
#include "rlgl.h"
#include "gameplay/effects/bomb_volume.hpp"
#include <array>
#include <memory>
#include <iostream>
#include <cmath>
namespace {
// kind 沿用 0/2/3 编号；已取消的 1 号槽为空，所有遍历均显式跳过。
const char* names[]={"bomb_seal",nullptr,"bomb_star","bomb_spark"};
const char* refs[]={"bomb-seal.png",nullptr,"bomb-stars.png","bomb-spark.png"};
const float durations[]={5,140/60.F,250/60.F,5};
// 光炮参数为世界长度；三维演出为独立美术预览，攻击范围对照不参与游戏结算。
using BombPreview=review::BombAssetInstance;
void integer(Shader s,const char*n,int v){SetShaderValue(s,GetShaderLocation(s,n),&v,SHADER_UNIFORM_INT);}
void real(Shader s,const char*n,float v){SetShaderValue(s,GetShaderLocation(s,n),&v,SHADER_UNIFORM_FLOAT);}
// 绘制借用模型和材质；顺向预览相机在发射端后方，沿 +Y 看向远端，完整显示光束沿程。
void draw(Model m,Shader shader,Vector3 position,float scale,Vector3 axis,float angle){for(int i=0;i<m.materialCount;i++)m.materials[i].shader=shader;DrawModelEx(m,position,axis,angle,{scale,scale,scale},WHITE);}
}
int main(int argc,char**argv){try{
 std::filesystem::path root=argc>1?argv[1]:".";bool capture=argc>2;SetConfigFlags(capture?FLAG_WINDOW_HIDDEN:FLAG_MSAA_4X_HINT);InitWindow(1440,900,"Bomb assets | independent review");SetTargetFPS(capture?0:60);
 std::array<std::array<Model,3>,4> models{};std::array<std::unique_ptr<review::CollisionAsset>,4> collisions;std::array<Texture2D,4> references{};
 for(int k=0;k<4;k++){if(k==1)continue;auto d=root/"assets/projectiles"/names[k];for(int l=0;l<3;l++){if((k==0&&l==1)||(k==2&&l==2))continue;auto file=l==0?std::string(names[k]):l==1?"shell":"effect";models[k][l]=LoadModel((d/(file+".gltf")).string().c_str());if(!models[k][l].meshCount)throw std::runtime_error("Bomb asset missing");}if(k<3)collisions[k]=std::make_unique<review::CollisionAsset>(d/"collision.bin");references[k]=LoadTexture((root/"docs/images/projectiles"/refs[k]).string().c_str());}
 Shader shader=LoadShader((review::ProjectileShaderDirectory(root)/"bombs.vs").string().c_str(),(review::ProjectileShaderDirectory(root)/"bombs.fs").string().c_str());if(!IsShaderValid(shader))throw std::runtime_error("Bomb shader invalid");
 RenderTexture2D target=LoadRenderTexture(1440,900);Font font=LoadFontEx((root/".deps/raylib-5.5/examples/text/resources/anonymous_pro_bold.ttf").string().c_str(),28,nullptr,0);
 auto text=[&](const char*s,int x,int y,int size,Color c){DrawTextEx(font,s,{float(x),float(y)},float(size),.2F,c);};
 BombPreview item;int mode=0,frame=0;bool all=true,paused=true,light=false,inside=false,showShell=true,showFx=true;float yaw=.3F,pitch=.2F,distance=8;Vector3 probe{};
 const char* shots[]={"overview","front","side","back","clay","body","shell","effect","model-collision","game-volume","combination","light","start","middle","ending","inside"};
 while(!WindowShouldClose()){
  if(capture){item={};item.kind=frame/16==0?0:frame/16==1?2:3;int s=frame%16;item.seconds=s==12?0:s==14?durations[item.kind]-.2F:s==13?durations[item.kind]*.6F:2;all=s==0;mode=s==4?1:s==5?2:s==6?3:s==7?4:s==8?5:s==9?6:0;item.combination=s==10;light=s==11;inside=s==15&&item.kind==3;if(inside){item.length=12;item.nearRadius=.22F;item.farRadius=.7F;}yaw=s==2?PI/2:s==3?PI:0;pitch=0;distance=all?11:item.combination?12:7;}
  else{
   if(IsKeyPressed(KEY_TAB)){item.kind=item.kind==0?2:item.kind==2?3:0;item.seconds=durations[item.kind]*.5F;}if(IsKeyPressed(KEY_C))item.palette=(item.palette+2)%5-1;if(IsKeyPressed(KEY_V))all=!all;if(IsKeyPressed(KEY_T)){item.combination=!item.combination;all=false;distance=item.combination?12:8;}
   if(IsKeyPressed(KEY_ONE))mode=1;if(IsKeyPressed(KEY_TWO))mode=2;if(IsKeyPressed(KEY_THREE))mode=0;if(IsKeyPressed(KEY_FOUR))mode=mode==5?0:5;if(IsKeyPressed(KEY_FIVE))mode=mode==6?0:6;if(IsKeyPressed(KEY_SIX))mode=3;if(IsKeyPressed(KEY_SEVEN))mode=4;
   if(IsKeyPressed(KEY_G))showShell=!showShell;if(IsKeyPressed(KEY_H))showFx=!showFx;if(IsKeyPressed(KEY_B))light=!light;if(IsKeyPressed(KEY_I))inside=!inside;if(IsKeyPressed(KEY_SPACE))paused=!paused;if(IsKeyPressed(KEY_R))item.seconds=0;if(IsKeyPressed(KEY_COMMA))item.seconds=std::max(0.F,item.seconds-1/60.F);if(IsKeyPressed(KEY_PERIOD))item.seconds+=1/60.F;if(IsKeyPressed(KEY_N))item.seconds=std::fmod(item.seconds+durations[item.kind]/4,durations[item.kind]);
   if(!paused)item.seconds=std::fmod(item.seconds+GetFrameTime(),durations[item.kind]);if(IsKeyDown(KEY_Q))item.length=std::max(.5F,item.length-.04F);if(IsKeyDown(KEY_E))item.length=std::min(40.F,item.length+.04F);if(IsKeyDown(KEY_A))item.nearRadius=std::max(.05F,item.nearRadius-.01F);if(IsKeyDown(KEY_D))item.nearRadius=std::min(item.farRadius,item.nearRadius+.01F);if(IsKeyDown(KEY_Z))item.farRadius=std::max(item.nearRadius,item.farRadius-.01F);if(IsKeyDown(KEY_X))item.farRadius=std::min(4.F,item.farRadius+.01F);
   if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)){auto d=GetMouseDelta();yaw-=d.x*.006F;pitch=Clamp(pitch+d.y*.006F,-1.4F,1.4F);}distance=Clamp(distance-GetMouseWheelMove()*.4F,1,35);if(IsKeyDown(KEY_LEFT))probe.x-=.02F;if(IsKeyDown(KEY_RIGHT))probe.x+=.02F;if(IsKeyDown(KEY_UP))probe.y+=.02F;if(IsKeyDown(KEY_DOWN))probe.y-=.02F;if(IsKeyDown(KEY_PAGE_UP))probe.z+=.02F;if(IsKeyDown(KEY_PAGE_DOWN))probe.z-=.02F;
  }
  review::ValidateBomb(item);
  Camera3D camera{{distance*sinf(yaw)*cosf(pitch),distance*sinf(pitch),distance*cosf(yaw)*cosf(pitch)},{0,0,0},{0,1,0},40,CAMERA_PERSPECTIVE};if(inside&&!all&&item.kind==3)camera={{0,-.9F,.55F},{0,14,0},{0,0,1},65,CAMERA_PERSPECTIVE};
  SetShaderValue(shader,GetShaderLocation(shader,"eye"),&camera.position,SHADER_UNIFORM_VEC3);BeginTextureMode(target);ClearBackground(light?Color{170,180,195,255}:Color{13,18,29,255});BeginMode3D(camera);bool contact=false;
  if(inside&&!all&&item.kind==3){
   // 场景参照显示光束确实从视点向前射出；中央方块为远处目标，不是光炮端盖。
   for(int i=-4;i<=4;i++){DrawLine3D({float(i),0,-1.5F},{float(i),18,-1.5F},{55,65,85,255});DrawLine3D({-4,float((i+4)*2),-1.5F},{4,float((i+4)*2),-1.5F},{55,65,85,255});}
   DrawCube({0,15,0},.35F,.35F,.35F,{170,185,205,255});DrawCubeWires({0,15,0},.35F,.35F,.35F,WHITE);
  }
  for(int k=0;k<4;k++){if(k==1)continue;if(!all&&k!=item.kind)continue;int count=item.combination&&!all?(k==0?7:k==2?64:1):1;
   for(int j=0;j<count;j++){float age=item.seconds;float scale=item.scale;Vector3 pos{all?((k==0?0:k==2?1:2)-1.F)*2.4F:0,k==3?-item.length/2:0,0};if(inside&&!all&&k==3)pos={0,0,0};Vector3 axis{0,1,0};float angle=0;int palette=item.palette;
    if(count>1){if(k==0){if(age*60<80+j*16)continue;float a=j*2*PI/7;pos={cosf(a)*2,sinf(a)*1.5F,sinf(a*2)*.6F};scale=.65F;}if(k==2){float progress=Clamp(age*60/180,0,1);pos={(j%4-1.5F)*1.6F*progress,((j/4)%4-1.5F)*1.6F*progress,(j/16-1.5F)*1.6F*progress};scale=.35F;palette=j%3;}}
    float opacity=Clamp(age*60/20,0,1)*Clamp((durations[k]-age)*60/30,0,1);if(mode==1)opacity=1;
    integer(shader,"kind",k);integer(shader,"palette",palette);integer(shader,"clay",mode==1);real(shader,"seconds",age);real(shader,"opacity",opacity*item.opacity);real(shader,"strength",item.strength);SetShaderValue(shader,GetShaderLocation(shader,"tintMultiplier"),&item.color,SHADER_UNIFORM_VEC3);real(shader,"beamLength",item.length);real(shader,"nearRadius",item.nearRadius);real(shader,"farRadius",item.farRadius);
    for(int l=0;l<3;l++){if((k==0&&l==1)||(k==2&&l==2))continue;if(mode==5||mode==6)break;if(mode==1&&l>0)continue;if(mode==2&&l>0)continue;if(mode==3&&l!=1)continue;if(mode==4&&l!=2)continue;if(l==1&&!showShell)continue;if(l==2&&!showFx)continue;
     integer(shader,"layer",l);Vector3 p=pos;float size=scale;if(l==2&&k==0)size*=1+std::max(0.F,(age*60-270)/30)*2;
     rlDrawRenderBatchActive();if(l>0||k==3){rlDisableBackfaceCulling();rlDisableDepthMask();BeginBlendMode(BLEND_ADDITIVE);}draw(models[k][l],shader,p,size,axis,angle);if(l>0||k==3){EndBlendMode();rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableBackfaceCulling();}
    }
    Matrix transform=MatrixMultiply(MatrixScale(scale,scale,scale),MatrixMultiply(MatrixRotate(axis,angle*DEG2RAD),MatrixTranslate(pos.x,pos.y,pos.z)));
    koumakyou::BombVolume volume;if(k==3){volume.shape=koumakyou::BombVolume::Shape::Frustum;volume.origin=pos;volume.end=Vector3Add(pos,{0,item.length,0});volume.radius=item.nearRadius;volume.endRadius=item.farRadius;}else{volume.origin=pos;volume.radius=(k==0?.5F:1.08F)*scale;}
    if(!all&&count==1)contact=k==3||mode==6?volume.IntersectsSphere(probe,.025F):collisions[k]->WorldDistanceSquared(probe,transform)<=.025F*.025F;
    if(mode==5||mode==6){rlDrawRenderBatchActive();rlDisableDepthTest();Color color=mode==5?GREEN:ORANGE;
     if(k==3){DrawCylinderWiresEx(volume.origin,volume.end,volume.radius,volume.endRadius,24,color);}else if(mode==6){DrawSphereWires(pos,volume.radius,12,20,color);}else{for(auto&p:collisions[k]->Parts())for(auto&t:p.triangles){auto a=Vector3Transform(t.a,transform),b=Vector3Transform(t.b,transform),c=Vector3Transform(t.c,transform);DrawLine3D(a,b,color);DrawLine3D(b,c,color);DrawLine3D(c,a,color);}}
     rlDrawRenderBatchActive();rlEnableDepthTest();
    }
   }
  }
  if(!inside&&!all&&!item.combination)DrawSphere(probe,.025F,contact?RED:GREEN);EndMode3D();Color ink=light?BLACK:RAYWHITE;text(inside&&item.kind==3?"Master Spark | behind emitter -> along +Y -> forward target":names[item.kind],24,18,28,ink);text("TAB kind | C color | V overview | T composition | drag orbit | wheel zoom",24,55,18,ink);text("1 clay | 2 body | 3 final | 4 model collision | 5 game volume | 6 shell | 7 effect",24,82,18,ink);text("G shell | H effect | B background | I along beam | Space play | ,/. step | N phase | R reset",24,109,18,ink);text("Q/E beam length | Z/X end radius | A/D start radius | arrows + PgUp/Dn probe",24,136,18,ink);text(TextFormat("Frame %.1f | %s | %s",item.seconds*60,mode==6?"ORANGE: GAME VOLUME":"GREEN: MODEL COLLISION",contact?"CONTACT":"CLEAR"),24,163,18,ink);
  auto ref=references[item.kind];DrawTextureEx(ref,{24,780},0,std::min(700.F/ref.width,100.F/ref.height),WHITE);EndTextureMode();BeginDrawing();ClearBackground(BLACK);DrawTextureRec(target.texture,{0,0,1440,-900},{0,0},WHITE);EndDrawing();
  if(capture||IsKeyPressed(KEY_F12)){auto out=root/"output/projectile-review/bombs";std::filesystem::create_directories(out);Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);auto path=out/(capture?std::string(names[item.kind])+"-"+shots[frame%16]+".png":"manual.png");ExportImage(image,path.string().c_str());if(inside&&item.kind==3){ExportImage(image,(out/"master-spark-first-person.png").string().c_str());ExportImage(image,(out/"master-spark-along-beam.png").string().c_str());}UnloadImage(image);}if(capture&&++frame==48)break;
 }
 for(auto&group:models)for(auto&m:group)if(m.meshCount)UnloadModel(m);for(auto t:references)if(t.id)UnloadTexture(t);UnloadShader(shader);UnloadFont(font);UnloadRenderTexture(target);CloseWindow();return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
