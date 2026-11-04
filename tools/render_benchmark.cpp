// 本文件通过生产渲染链执行可重复压力实验，记录异步 GPU 时间、CPU 时间及真实 GL 提交计数。
#include "app/game.hpp"
#include "resources/asset_manager.hpp"
#include "render/combat_renderer.hpp"
#include "stages/stage1/content.hpp"
#include "stages/stage1/ids.hpp"
#include "external/glad.h"
#include "rlgl.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/resource.h>

namespace {
using namespace koumakyou;
using Clock=std::chrono::steady_clock;
double Milliseconds(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}

// 只在正式绘制区间计数；函数指针替换不改变驱动调用及参数，基线和优化版共用同一测量入口。
struct Counters {std::uint64_t draws=0,uploads=0,bytes=0,locations=0,stateReads=0;};
Counters counts;
auto originalDrawElementsInstanced=glad_glDrawElementsInstanced;
auto originalDrawElements=glad_glDrawElements;
auto originalDrawArrays=glad_glDrawArrays;
auto originalBufferSubData=glad_glBufferSubData;
auto originalGetUniformLocation=glad_glGetUniformLocation;
auto originalGetIntegerv=glad_glGetIntegerv;
auto originalGetBooleanv=glad_glGetBooleanv;
auto originalIsEnabled=glad_glIsEnabled;
void GLAD_API_PTR CountInstanced(GLenum mode,GLsizei n,GLenum type,const void* indices,GLsizei instances){++counts.draws;originalDrawElementsInstanced(mode,n,type,indices,instances);}
void GLAD_API_PTR CountElements(GLenum mode,GLsizei n,GLenum type,const void* indices){++counts.draws;originalDrawElements(mode,n,type,indices);}
void GLAD_API_PTR CountArrays(GLenum mode,GLint first,GLsizei n){++counts.draws;originalDrawArrays(mode,first,n);}
void GLAD_API_PTR CountUpload(GLenum target,GLintptr offset,GLsizeiptr size,const void* data){++counts.uploads;counts.bytes+=size;originalBufferSubData(target,offset,size,data);}
GLint GLAD_API_PTR CountLocation(GLuint program,const GLchar* name){++counts.locations;return originalGetUniformLocation(program,name);}
void GLAD_API_PTR CountInteger(GLenum name,GLint* value){++counts.stateReads;originalGetIntegerv(name,value);}
void GLAD_API_PTR CountBoolean(GLenum name,GLboolean* value){++counts.stateReads;originalGetBooleanv(name,value);}
GLboolean GLAD_API_PTR CountEnabled(GLenum name){++counts.stateReads;return originalIsEnabled(name);}
void InstallCounters(){
 originalDrawElementsInstanced=glad_glDrawElementsInstanced;glad_glDrawElementsInstanced=CountInstanced;
 originalDrawElements=glad_glDrawElements;glad_glDrawElements=CountElements;
 originalDrawArrays=glad_glDrawArrays;glad_glDrawArrays=CountArrays;
 originalBufferSubData=glad_glBufferSubData;glad_glBufferSubData=CountUpload;
 originalGetUniformLocation=glad_glGetUniformLocation;glad_glGetUniformLocation=CountLocation;
 originalGetIntegerv=glad_glGetIntegerv;glad_glGetIntegerv=CountInteger;
 originalGetBooleanv=glad_glGetBooleanv;glad_glGetBooleanv=CountBoolean;
 originalIsEnabled=glad_glIsEnabled;glad_glIsEnabled=CountEnabled;
}

/// @brief GPU 查询在 64 槽中轮转；仅读取已完成结果，缺少入口或驱动拒绝时报告 NA。
struct GpuTimer {
 struct Slot {GLuint id=0;bool busy=false;int sample=-1;};
 std::array<Slot,64> slots{};int active=-1;bool supported=false;std::size_t skipped=0;
 GpuTimer(){
  supported=glGenQueries&&glBeginQuery&&glEndQuery&&glGetQueryObjectui64v&&glGetQueryObjectiv;
  if(!supported)return;
  while(glGetError()!=GL_NO_ERROR){}
  for(auto& slot:slots)glGenQueries(1,&slot.id);
  glBeginQuery(GL_TIME_ELAPSED,slots[0].id);glEndQuery(GL_TIME_ELAPSED);
  supported=glGetError()==GL_NO_ERROR;
 }
 ~GpuTimer(){for(auto& slot:slots)if(slot.id)glDeleteQueries(1,&slot.id);}
 void Poll(std::vector<double>& samples){if(!supported)return;for(auto& slot:slots)if(slot.busy){GLint ready=0;glGetQueryObjectiv(slot.id,GL_QUERY_RESULT_AVAILABLE,&ready);if(ready){GLuint64 ns=0;glGetQueryObjectui64v(slot.id,GL_QUERY_RESULT,&ns);if(slot.sample>=0)samples.at(slot.sample)=ns/1e6;slot.busy=false;}}}
 void Begin(int sample){if(!supported)return;for(int i=0;i<int(slots.size());++i)if(!slots[i].busy){active=i;slots[i].sample=sample;glBeginQuery(GL_TIME_ELAPSED,slots[i].id);return;}if(sample>=0)++skipped;}
 void End(){if(active>=0){glEndQuery(GL_TIME_ELAPSED);slots[active].busy=true;active=-1;}}
};

/// @brief 静态输入快照；坐标/尺寸为世界单位，固定年龄确保两个版本绘制同一内容。
struct Scene {
 BulletSystem bullets;CombatRenderer models;SpriteRenderer sprites;ActorEffectSystem effects;
 std::vector<PickupSprite> pickups;std::array<behavior::Actor,24> actors;std::vector<const behavior::Actor*> visible;
 PlayerBombSprite bomb{};std::vector<BombVolumeSprite> bombs;
 Camera3D camera{{0,0,-10},{0,0,5},{0,1,0},65,CAMERA_PERSPECTIVE};
 Scene(const std::string& name){
  std::size_t n=name=="mixed1024"?1024:name=="mixed8192"?8192:name=="capacity16384"?16384:name=="interleaved"?4096:name=="lasers"?1024:name=="marisa-lasers"?2048:0;
  for(std::size_t i=0;i<n;++i){const int type=i%10,color=i%16;auto* b=bullets.Spawn(BulletOwner::Enemy,{(int(i%64)-31.5F)*.16F,(int(i/64%64)-31.5F)*.12F,float(i%127)*.09F},{0,0,1},battle::EnemyBulletHitRadius(type),WHITE,stage1::kNativeBulletBase+type*16+color);if(!b)throw std::runtime_error("Bullet pool overflow");b->nativeType=type;b->nativeColor=color;b->age=1;if(name=="interleaved"&&i%7==0){b->age=.12F;b->spawnVisualSeconds=.5F;}}
  if(name=="interleaved"||name=="lasers"||name=="marisa-lasers")for(std::size_t i=0;i<actors.size();++i){
   behavior::Spawn spawn{.position={(int(i%4)-1.5F)*1.2F,(int(i%3)-1)*1.2F,float(i)*.45F},.pattern={.movement={.speed=0},.endFrame=2147483647},.sourceHealth=1,.visual=stage1::kFairy,.hitRadius=.4F,.retireOutside=false};
   actors[i].Activate(spawn);actors[i].presentation.Update(1);visible.push_back(&actors[i]);
  }
  if(name=="lasers")for(int i=0;i<32;++i){auto* b=bullets.SpawnBeam({(i%8-3.5F)*.4F,(i/8-1.5F)*.5F,-2},{.015F,0,1},24,.12F,0,10,WHITE);b->visual=stage1::kNativeLaserBase+i%16;b->age=1;}
  // 从生产 Player 发射满 Power 的两束激光和主弹，保留真实近景半径、外辉光及透明切点。
  // 手工缩小半径或只放远处激光会漏掉局内的大面积重复着色；快照生成不计入性能采样。
  if(name=="marisa-lasers"){
   Player player;player.ConfigureRun(3,3,false,ShotType::IllusionLaser);player.AdjustPower(128);
   stage1::Stage1Content content;const auto visuals=content.PlayerVisuals(ShotType::IllusionLaser);
   for(int i=0;i<20;++i){player.Update(1.F/60,{.fire=true},bullets,visuals,true);bullets.Update(1.F/60);}
   player.SyncCamera(camera);
  }
  if(name=="shots")for(int i=0;i<512;++i){auto* b=bullets.Spawn(BulletOwner::Player,{(i%32-15.5F)*.2F,(i/32-7.5F)*.2F,float(i%16)*.5F},{0,0,1},.08F,WHITE,i%2?stage1::kMarisaPrimary:stage1::kPlayerNeedle);b->age=.3F;}
  if(name=="drops"||name=="interleaved")for(int i=0;i<(name=="drops"?512:48);++i)pickups.push_back({{(i%24-11.5F)*.2F,(i/24-4)*.2F,float(i%37)*.3F},stage1::kPowerPoint,1,nullptr,255,.5F});
  if(name=="bomb"){
   bomb.shape=PlayerBombSprite::Shape::Beam;bomb.position={0,0,-8};bomb.end={0,0,18};bomb.volume.origin=bomb.position;bomb.volume.end=bomb.end;bomb.volume.radius=1;bomb.frame.color=WHITE;bomb.ageSeconds=1;
   bombs.push_back({&bomb,stage1::kMarisaBombSpark});
  }
 }
 void Draw(const AssetManager& assets){const auto& stage=assets.Stage();BeginMode3D(camera);models.Prepare(assets.Projectiles(),bullets,stage.visuals,camera,camera.position,float(GetRenderWidth())/GetRenderHeight(),1);sprites.Draw(camera,camera.position,bullets,visible,nullptr,pickups,stage.visuals,stage.shaders,effects,bombs,nullptr,&models);EndMode3D();}
};

double CpuSeconds(){rusage usage{};getrusage(RUSAGE_SELF,&usage);return usage.ru_utime.tv_sec+usage.ru_utime.tv_usec/1e6+usage.ru_stime.tv_sec+usage.ru_stime.tv_usec/1e6;}
void WriteDistribution(std::ostream& out,std::vector<double> values){
 values.erase(std::remove_if(values.begin(),values.end(),[](double v){return v<0;}),values.end());
 if(values.empty()){out<<",NA,NA,NA,NA";return;}const double mean=std::accumulate(values.begin(),values.end(),0.)/values.size();std::sort(values.begin(),values.end());out<<','<<mean;for(double q:{.5,.95,.99})out<<','<<values[std::min(values.size()-1,std::size_t(q*values.size()))];
}
}

/// @brief 参数：素材根、输出目录、场景、预热秒、测量秒、重复数、MSAA(0/4)、FPS(0不限)。
/// 场景 game/menu 运行完整 Game 固定快照；其他场景调用正式模型和透明队列；窗口须保持非最小化。
int main(int argc,char** argv){try{
 if(argc!=9)throw std::runtime_error("Usage: render_benchmark ASSETS OUTPUT CASE WARM_SECONDS MEASURE_SECONDS REPEATS MSAA FPS");
 const auto root=std::filesystem::absolute(argv[1]),out=std::filesystem::absolute(argv[2]);const std::string name=argv[3];
 const std::array<std::string,12> names{"empty","mixed1024","mixed8192","interleaved","lasers","marisa-lasers","shots","drops","bomb","capacity16384","game","menu"};
 if(std::find(names.begin(),names.end(),name)==names.end())throw std::runtime_error("Unknown scene");
 const double warm=std::stod(argv[4]),duration=std::stod(argv[5]);const int repeats=std::stoi(argv[6]),msaa=std::stoi(argv[7]),fps=std::stoi(argv[8]);
 if(warm<0||duration<=0||repeats<1||(msaa!=0&&msaa!=4)||fps<0)throw std::runtime_error("Invalid sampling parameters");
 std::filesystem::create_directories(out);SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN|FLAG_WINDOW_UNFOCUSED|(msaa?FLAG_MSAA_4X_HINT:0));InitWindow(1100,760,"koumakyou production renderer benchmark");ClearWindowState(FLAG_VSYNC_HINT);SetTargetFPS(fps);
 if(!IsWindowReady())throw std::runtime_error("No graphics context");
 GLint samples=0;glGetIntegerv(GL_SAMPLES,&samples);
 std::ofstream hardware(out/(name+"-hardware.txt"));hardware<<"renderer="<<glGetString(GL_RENDERER)<<"\nversion="<<glGetString(GL_VERSION)<<"\nlogical="<<GetScreenWidth()<<'x'<<GetScreenHeight()<<"\nframebuffer="<<GetRenderWidth()<<'x'<<GetRenderHeight()<<"\nsamples="<<samples<<"\nfps="<<fps<<"\nvsync="<<IsWindowState(FLAG_VSYNC_HINT)<<"\nwarm="<<warm<<"\nmeasure="<<duration<<"\nrepeats="<<repeats<<"\n";
 InstallCounters();
 {
 AssetManager assets(root);stage1::Stage1Content content;assets.LoadStage(content.Assets());Scene scene(name);
 std::unique_ptr<Game> game;
 if(name=="game"||name=="menu"){game=std::make_unique<Game>();if(name=="game"){game->StartPreview();while(game->IsLoading()){game->Update(1.F/60);if(game->HasLoadingFailed())throw std::runtime_error("Game loading failed");}for(int i=0;i<360;++i)game->Update(1.F/60);}SetTargetFPS(fps);}
 std::ofstream csv(out/(name+".csv"));csv<<"case,repeat,frames,gpu_samples,gpu_skipped,draws,uploads,upload_bytes,uniform_lookups,state_reads,segments,cpu_core_percent,over_60,over_120";for(auto stage:{"frame","cpu_draw","gpu"})for(auto stat:{"mean","p50","p95","p99"})csv<<','<<stage<<'_'<<stat<<"_ms";csv<<'\n';
 for(int repeat=1;repeat<=repeats;++repeat){
  std::vector<double> frames,draw,gpu;std::vector<Counters> counters;frames.reserve(500000);draw.reserve(500000);gpu.reserve(500000);counters.reserve(500000);GpuTimer timer;
  auto start=Clock::now();bool begun=false;double cpuStart=0;Counters total{};std::size_t over60=0,over120=0;
  while(Milliseconds(start)<(warm+duration)*1000){
   if(WindowShouldClose()||IsWindowMinimized())throw std::runtime_error("Benchmark closed or minimized");
   const bool measure=Milliseconds(start)>=warm*1000;if(measure&&!begun){begun=true;cpuStart=CpuSeconds();}
   timer.Poll(gpu);const auto frameStart=Clock::now();const int sample=measure?int(frames.size()):-1;if(measure)gpu.push_back(-1);
   BeginDrawing();ClearBackground({7,9,20,255});counts={};timer.Begin(sample);const auto drawStart=Clock::now();
   if(game)game->Draw();else scene.Draw(assets);
   rlDrawRenderBatchActive();const double drawMs=Milliseconds(drawStart);timer.End();const auto measuredCounts=counts;EndDrawing();
   if(measure){const double frameMs=Milliseconds(frameStart);frames.push_back(frameMs);draw.push_back(drawMs);counters.push_back(measuredCounts);total.draws+=measuredCounts.draws;total.uploads+=measuredCounts.uploads;total.bytes+=measuredCounts.bytes;total.locations+=measuredCounts.locations;total.stateReads+=measuredCounts.stateReads;over60+=frameMs>1000./60;over120+=frameMs>1000./120;}
  }
  const double cpuUsed=CpuSeconds()-cpuStart;glFinish();timer.Poll(gpu);
  if(frames.empty())throw std::runtime_error("No measured frames");
  std::ofstream raw(out/(name+"-"+std::to_string(repeat)+"-frames.csv"));raw<<"frame_ms,cpu_draw_ms,gpu_ms,draws,uploads,upload_bytes,uniform_lookups,state_reads\n";
  for(std::size_t i=0;i<frames.size();++i)raw<<frames[i]<<','<<draw[i]<<','<<gpu[i]<<','<<counters[i].draws<<','<<counters[i].uploads<<','<<counters[i].bytes<<','<<counters[i].locations<<','<<counters[i].stateReads<<'\n';
  const double n=frames.size();csv<<name<<','<<repeat<<','<<n<<','<<std::count_if(gpu.begin(),gpu.end(),[](double v){return v>=0;})<<','<<timer.skipped<<','<<total.draws/n<<','<<total.uploads/n<<','<<total.bytes/n<<','<<total.locations/n<<','<<total.stateReads/n<<','<<scene.sprites.LastBeamSegmentCount()<<','<<cpuUsed/duration*100<<','<<over60/n<<','<<over120/n;
  WriteDistribution(csv,frames);WriteDistribution(csv,draw);WriteDistribution(csv,gpu);csv<<'\n';csv.flush();std::cout<<name<<" repeat="<<repeat<<" frames="<<frames.size()<<" draws="<<total.draws/n<<" uploads="<<total.uploads/n<<std::endl;
  // 导出不在采样区间内，供 A/B 图像逐像素对照；窗口目标保留实际 MSAA resolve 结果。
  auto image=LoadImageFromScreen();ExportImage(image,(out/(name+".png")).string().c_str());UnloadImage(image);
 }
 }
 CloseWindow();return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
