// 本文件检查胶囊轴身/端帽、时窗边界和游戏 BeamVisualScale 的一致性。
#include "laser_renderer.hpp"
#include "gameplay/bullets/types.hpp"
#include "raymath.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char** argv){try{
 review::LaserInstance b;review::ValidateLaser(b);
 check(review::LaserTouches(b,{.1F,2,0},0),"body");check(review::LaserTouches(b,{0,-.1F,0},0),"start cap");check(review::LaserTouches(b,{0,4.1F,0},0),"end cap");check(!review::LaserTouches(b,{.15F,2,0},.01F),"halo");
 b.origin={3,2,-1};b.direction=Vector3Normalize(Vector3{1,2,3});check(review::LaserTouches(b,Vector3Add(b.origin,Vector3Scale(b.direction,2)),.01F),"rotated axis");
 koumakyou::Bullet game;game.active=true;game.warningSeconds=b.warning;game.beamGrowSeconds=b.growEnd;game.dangerEndSeconds=b.dangerEnd;game.maxAge=b.end;game.beamFadeSeconds=b.end-b.dangerEnd;
 for(float t:{0.F,1.499F,1.5F,1.99F,2.F,3.F,4.999F,5.F,5.5F,6.F}){b.age=t;game.age=t;check(review::LaserDangerous(b)==game.Dangerous(),"danger window");check(std::abs(review::LaserVisualScale(b)-game.BeamVisualScale())<1e-5F,"visual timing");if(!review::LaserDangerous(b))check(!review::LaserTouches(b,b.origin,1),"safe phase contact");}
 b.dangerEnd=4;b.age=4.5F;check(!review::LaserDangerous(b)&&review::LaserVisualScale(b)==1,"Independent fade and danger cutoff");
 b.radius=-1;bool rejected=false;try{review::ValidateLaser(b);}catch(...){rejected=true;}check(rejected,"invalid radius");
 SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(256,256,"Laser tests");{review::LaserRenderer renderer(argv[1],2);b={};Camera3D camera{{3,2,6},{0,2,0},{0,1,0},40,CAMERA_PERSPECTIVE};
 for(int type=0;type<2;++type)for(int color=0;color<16;++color){b.variant=type;b.color=color;BeginDrawing();ClearBackground(BLACK);BeginMode3D(camera);renderer.Draw(std::span(&b,1),camera,1);EndMode3D();EndDrawing();check(renderer.Stats().drawCalls==3&&renderer.Stats().triangles==432,"mesh roles or draws");}
 // 像素检查避免只有调用计数正确、实际着色器却没有可见输出。
 RenderTexture2D target=LoadRenderTexture(256,256);
 auto image=[&](bool inside){camera.position=inside?Vector3{0,2,0}:Vector3{3,2,6};camera.target={0,2,inside?1.F:0.F};
  BeginTextureMode(target);ClearBackground(BLACK);BeginMode3D(camera);renderer.Draw(std::span(&b,1),camera,1);EndMode3D();EndTextureMode();return LoadImageFromTexture(target.texture);};
 for(bool inside:{false,true}){Image rendered=image(inside);int visible=0;for(int y=0;y<256;++y)for(int x=0;x<256;++x){Color c=GetImageColor(rendered,x,y);visible+=c.r+c.g+c.b>20;}UnloadImage(rendered);check(visible>100,"Laser outside/inside invisible");}
 UnloadRenderTexture(target);}

 CloseWindow();std::cout<<"Laser capsule, timing and 32 appearances: PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
