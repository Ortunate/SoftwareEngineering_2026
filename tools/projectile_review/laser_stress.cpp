// 本文件测量两套激光、多束交叉、近镜头及普通弹混合的 CPU/GPU 帧耗时。
#include "laser_renderer.hpp"
#include "collection_renderer.hpp"
#include "performance_metrics.hpp"
#include "raymath.h"
#include <fstream>
#include <iostream>
#include <chrono>
int main(int argc,char** argv){try{
 std::filesystem::path root=argv[1];bool quick=argc>2&&std::string(argv[2])=="--quick";auto out=root/"output/projectile-review/lasers";std::filesystem::create_directories(out);
 SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);InitWindow(1920,1080,"Laser performance | do not minimize");SetTargetFPS(0);
 {review::LaserRenderer lasers(root,8192);review::CollectionRenderer ordinary(root,8192);std::vector<review::LaserInstance> beams(8192);std::vector<review::CollectionInstance> bullets(7680);
 std::ofstream hardware(out/"hardware.txt");hardware<<glGetString(GL_RENDERER)<<'\n'<<glGetString(GL_VERSION)<<"\n1920x1080, no vsync/cap/MSAA. Full: warm 3s, measure 10s, 3 repeats.\n";
 std::ofstream csv(out/(quick?"smoke.csv":"performance.csv"));csv<<"case,repeat,beams,ordinary,draws,gpu_skipped";for(auto s:{"frame","cpu","gpu"})for(auto q:{"mean","p50","p95","p99"})csv<<','<<s<<'_'<<q;csv<<'\n';
 for(int scenario=0;scenario<11;++scenario)for(int repeat=0;repeat<(quick?1:3);++repeat){size_t count=scenario<6?std::array<size_t,3>{32,128,512}[scenario%3]:scenario==10?8192:512;bool mixed=scenario==9;Camera3D camera{{0,0,scenario==6||scenario==7?1.5F:16.F},{0,0,0},{0,1,0},50,CAMERA_PERSPECTIVE};
  for(size_t i=0;i<count;++i){auto& b=beams[i];b={};b.variant=scenario<6?scenario/3:scenario==6?0:scenario==7?1:i%2;b.color=i%16;b.radius=scenario==6||scenario==7?.22F:.04F;b.origin={(int(i%32)-15.5F)*.25F,-2,float(i/32)*.12F};b.direction=scenario==8?Vector3Normalize(Vector3{std::sin(float(i)),1,std::cos(float(i))}):Vector3{0,1,0};}
  for(size_t i=0;i<bullets.size();++i){auto& b=bullets[i];b.type=i%10;b.transform.scale=.06F;b.transform.position={(int(i%80)-40)*.12F,(int(i/80%48)-24)*.12F,-2};b.transform.appearance=review::AppearanceFor(b.type,i%16);}
  review::PerformanceSamples frame,cpu,gpu;review::AsyncGpuTimer timer;double start=GetTime(),previous=start,warm=quick?.1:3,duration=quick?.3:10;volatile size_t hits=0;int calls=0;
  while(GetTime()-start<warm+duration){double now=GetTime();bool measured=now-start>=warm;if(measured)frame.Add((now-previous)*1000);previous=now;auto begin=std::chrono::steady_clock::now();
   for(size_t i=0;i<count;++i){beams[i].origin.x+=float(std::sin(now+i)*.0001);if(review::LaserTouches(beams[i],{0,0,0},.02F))hits=hits+1;}
   if(measured)cpu.Add(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());timer.Poll(gpu);
   BeginDrawing();ClearBackground(BLACK);BeginMode3D(camera);timer.Begin(measured);lasers.Draw(std::span(beams.data(),count),camera,1920.F/1080);calls=lasers.Stats().drawCalls;
   if(mixed){ordinary.Prepare(bullets,camera,1920.F/1080);ordinary.Draw(0);calls+=ordinary.Stats().drawCalls;}timer.End();EndMode3D();EndDrawing();
  }glFinish();timer.Poll(gpu);csv<<scenario<<','<<repeat+1<<','<<count<<','<<(mixed?bullets.size():0)<<','<<calls<<','<<timer.skipped;frame.Write(csv);cpu.Write(csv);gpu.Write(csv);csv<<'\n';csv.flush();std::cout<<"Laser case "<<scenario<<" repeat "<<repeat+1<<'\n';
 }
 }CloseWindow();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
