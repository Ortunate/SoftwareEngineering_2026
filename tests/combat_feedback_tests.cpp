// 本文件无窗口验证曲面法线、硬边/显示角色隔离及倒计时提示的跨帧规则。
#include "render/projectiles/smooth_normals.hpp"
#include "gameplay/effects/combat_audio.hpp"
#include <cassert>
#include <iostream>
using namespace koumakyou;
namespace {
void CheckNormals(){
 for(bool indexed:{false,true})for(bool hard:{false,true})for(bool separateRole:{false,true}){
  constexpr float q=.70710678F;
  std::array<float,18> positions{0,0,0,1,0,0,0,1,0,0,0,0,0,1,0,hard?0.F:-q,0,hard?1.F:q};
  std::array<float,18> normals{0,0,1,0,0,1,0,0,1,hard?1.F:q,0,hard?0.F:q,hard?1.F:q,0,hard?0.F:q,hard?1.F:q,0,hard?0.F:q};
  const auto originalPositions=positions,originalNormals=normals;
  std::array<unsigned char,24> colors{};for(int i=3;i<6;++i)colors[4*i+1]=separateRole?128:0;
  std::array<unsigned short,6> indices{0,1,2,3,4,5};Mesh mesh{};mesh.vertexCount=6;mesh.triangleCount=2;
  mesh.vertices=positions.data();mesh.normals=normals.data();mesh.colors=colors.data();mesh.indices=indexed?indices.data():nullptr;
  const auto result=projectiles::SmoothProjectileNormals(mesh);
  assert(result.size()==6&&positions==originalPositions&&normals==originalNormals);
  for(auto n:result)assert(std::isfinite(n.x)&&std::isfinite(n.y)&&std::isfinite(n.z)&&std::abs(Vector3Length(n)-1)<1e-5F);
  if(hard||separateRole){assert(Vector3Distance(result[0],{0,0,1})<1e-5F);assert(Vector3Distance(result[3],{hard?1.F:q,0,hard?0.F:q})<1e-5F);}
  else {assert(Vector3Distance(result[0],result[3])<1e-5F);assert(result[0].x>.1F&&result[0].z>.8F&&result[0].z<.99F);}
 }
}
void CheckCountdown(){
 using namespace combat_audio;
 for(int fps:{30,60,120,144,240}){
  int previous=CountdownSecond(12,0),heard=0;
  for(int frame=1;frame<=12*fps;++frame){
   const int current=CountdownSecond(12,float(frame)/fps);const auto sound=CountdownCue(previous,current);
   if(!sound.empty()){++heard;assert(current==11-heard);assert(sound==(current<=5?"countdown_urgent":"countdown"));}
   assert(CountdownCue(current,current).empty());previous=current;
  }assert(heard==10);
 }
 assert(CountdownSecond(-1,0)==-1&&CountdownSecond(20,21)==0);
 assert(CountdownCue(11,3)=="countdown_urgent"&&CountdownCue(11,9)=="countdown");
 assert(CountdownCue(2,5).empty()&&CountdownCue(1,0).empty()&&CountdownCue(-1,-1).empty());
 assert(CountdownSecond(20,10)==10&&CountdownSecond(20,10.00001F)==10);
}
}
int main(){CheckNormals();CheckCountdown();std::cout<<"Smooth surfaces, preserved hard edges/roles and countdown boundaries PASS\n";}
