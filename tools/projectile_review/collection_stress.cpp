// 本文件在全组建模完成后统一测试九类新弹、混合弹种、极端透明覆盖和二维基线。
#include "collection_renderer.hpp"
#include "performance_metrics.hpp"
#include "gameplay/bullets/system.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <cstdlib>
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
struct Scenario {std::string name;int type;size_t count;bool glow,moving,multi;int layout;bool diagnostic=false,sprite=false;};
int main(int argc,char** argv){try{
    fs::path root=fs::absolute(argc>1?argv[1]:".");bool quick=argc>2&&std::string(argv[2])=="--quick";
    auto out=root/"output/projectile-review/collection/performance";fs::create_directories(out);
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);InitWindow(1920,1080,"All projectiles | 1920x1080 benchmark | do not minimize");ClearWindowState(FLAG_VSYNC_HINT);SetTargetFPS(0);
    if(GetRenderWidth()!=1920||GetRenderHeight()!=1080)throw std::runtime_error("1920x1080 framebuffer required");
    {
    constexpr size_t capacity=koumakyou::BulletSystem::kCapacity;review::CollectionRenderer renderer(root,capacity);
    std::vector<review::CollectionInstance> items(capacity);std::vector<Vector3> bases(capacity);review::CollisionScratch scratch;scratch.Prepare(512);
    Camera3D camera{{0,0,12},{0,0,0},{0,1,0},50,CAMERA_PERSPECTIVE};
    RenderTexture2D sprite=LoadRenderTexture(64,64);BeginTextureMode(sprite);ClearBackground(BLANK);DrawCircle(32,32,29,WHITE);EndTextureMode();
    std::ofstream hardware(out/(quick?"smoke-hardware.txt":"hardware.txt"));hardware<<"GPU: "<<glGetString(GL_RENDERER)<<"\nOpenGL: "<<glGetString(GL_VERSION)<<"\nCPU: "<<(std::getenv("PROCESSOR_IDENTIFIER")?std::getenv("PROCESSOR_IDENTIFIER"):"unavailable")
        <<"\nFramebuffer: 1920x1080\nVsync off, no frame cap, no MSAA, visible/non-minimized.\nShared capacity: "<<capacity<<"\nGPU shared instances: "<<renderer.BufferBytes()<<" bytes\nCPU caller instances: "<<items.size()*sizeof(items[0])<<" bytes\nCPU renderer packed instances: "<<renderer.BufferBytes()<<" bytes\nWarm/measure/repeats: normal 3s/10s/3; diagnostic 0.2s/0.4s/1.\n";
    std::vector<Scenario> scenarios;
    for(int k:{5,8,2,0,3,1,6,9,7}){
        for(size_t n:{1024U,4096U})scenarios.push_back({"diagnostic-type-"+std::to_string(k)+"-"+std::to_string(n),k,n,true,true,true,0,true});
        scenarios.push_back({"type-"+std::to_string(k)+"-8192-static-off",k,capacity,false,false,false,0});
        scenarios.push_back({"type-"+std::to_string(k)+"-8192-moving-glow",k,capacity,true,true,true,0});
    }
    for(int k:{1,3,6,9,7})scenarios.push_back({"extreme-type-"+std::to_string(k)+"-near",k,capacity,true,true,true,2});
    scenarios.push_back({"mixed-solid",-1,capacity,true,true,true,0});scenarios.push_back({"mixed-shell",-2,capacity,true,true,true,0});scenarios.push_back({"mixed-fire-bubble",-3,capacity,true,true,true,1});
    scenarios.push_back({"culled-75-percent",-1,capacity,true,true,true,3});scenarios.push_back({"collision-heavy",8,capacity,false,true,true,4});
    for(size_t n:{1024U,4096U,8192U})scenarios.push_back({"sprite-"+std::to_string(n),0,n,false,true,true,0,false,true});
    std::ofstream csv(out/(quick?"smoke.csv":"results.csv"));csv<<"case,repeat,diagnostic,submitted,visible,draw_calls,triangles,narrow_per_frame,hits_per_frame,frames,gpu_samples,gpu_skipped,type_counts";
    for(auto stage:{"frame","update","collision","upload","draw_cpu","gpu"})for(auto stat:{"mean","p50","p95","p99"})csv<<','<<stage<<'_'<<stat<<"_ms";csv<<'\n';
    for(auto s:scenarios)for(int repeat=1;repeat<=(quick||s.diagnostic?1:3);++repeat){
        std::array<size_t,10> counts{};
        for(size_t i=0;i<s.count;++i){auto& p=items[i];p={};p.type=s.type;
            if(s.type==-1){static int types[]{0,2,4,5,8};p.type=types[i%5];}
            if(s.type==-2){static int types[]{1,3,6,9};p.type=types[i%4];}
            if(s.type==-3)p.type=i%2?7:9;
            ++counts[p.type];p.transform.appearance=review::AppearanceFor(p.type,s.multi?i%16:2);p.phase=float(i%20)*.31F;
            static float sizes[]{.16F,.20F,.20F,.20F,.20F,.20F,.4F,.3F,.20F,.55F};p.transform.scale=sizes[p.type];
            bases[i]={(float(i%128)-63.5F)*.125F,(float(i/128)-31.5F)*.115F,float(i%7)*.05F};
            if(s.layout==1)bases[i]=Vector3Scale(bases[i],.35F);
            if(s.layout==2){bases[i]=Vector3Scale(bases[i],.1F);bases[i].z=10.4F;p.transform.scale=.5F;}
            if(s.layout==3&&i%4)bases[i].x+=40;
            if(s.layout==4)bases[i]={.025F*std::sin(float(i)),.025F*std::cos(float(i)),0};
            p.transform.position=bases[i];
        }
        review::PerformanceSamples frame,update,collision,upload,draw,gpu;review::AsyncGpuTimer timer;
        double start=GetTime(),warm=quick||s.diagnostic?.2:3,duration=quick||s.diagnostic?.4:10;
        size_t frames=0,narrows=0,hits=0;int visible=0,calls=0,triangles=0;std::cout<<s.name<<" repeat "<<repeat<<std::endl;
        while(GetTime()-start<warm+duration){
            if(WindowShouldClose()||IsWindowMinimized())throw std::runtime_error("Experiment aborted: closed/minimized");
            auto begin=Clock::now();double t=GetTime()-start;bool measured=t>=warm;timer.Poll(gpu);auto stage=Clock::now();
            if(s.moving)for(size_t i=0;i<s.count;++i){auto& p=items[i].transform;float a=float(t)*.8F+float(i)*.017F;p.position=Vector3Add(bases[i],{.015F*std::sin(a),.015F*std::cos(a),0});p.rotation=QuaternionFromAxisAngle({0,0,1},a);}
            double u=elapsed(stage);stage=Clock::now();size_t narrow=0,hit=0;
            for(size_t i=0;i<s.count;++i)hit+=review::TouchesInstance(renderer.Collision(items[i].type),scratch,items[i].transform,{0,0,0},.012F,&narrow);
            double c=elapsed(stage);stage=Clock::now();if(!s.sprite)renderer.Prepare(std::span(items.data(),s.count),camera,1920.F/1080);double uploadMs=elapsed(stage);
            BeginDrawing();ClearBackground({18,25,38,255});BeginMode3D(camera);rlDrawRenderBatchActive();timer.Begin(measured);stage=Clock::now();
            if(!s.sprite){renderer.Draw((float)t,s.glow);visible=(int)renderer.Stats().visible;calls=renderer.Stats().drawCalls;triangles=renderer.Stats().triangles;}
            else{for(size_t i=0;i<s.count;++i)DrawBillboard(camera,sprite.texture,items[i].transform.position,items[i].transform.scale,items[i].transform.appearance.bodyColor);rlDrawRenderBatchActive();visible=(int)s.count;calls=(int)((s.count+8190)/8191);triangles=(int)s.count*2;}
            double d=elapsed(stage);timer.End();EndMode3D();DrawText(TextFormat("%s | %i/%i | repeat %i | %s",s.name.c_str(),visible,(int)s.count,repeat,measured?"MEASURE":"WARMUP"),20,20,20,WHITE);EndDrawing();
            if(measured){frame.Add(elapsed(begin));update.Add(u);collision.Add(c);upload.Add(uploadMs);draw.Add(d);narrows+=narrow;hits+=hit;++frames;}
        }
        glFinish();timer.Poll(gpu);
        if(!frames)throw std::runtime_error("No measured frames");
        csv<<s.name<<','<<repeat<<','<<(quick||s.diagnostic)<<','<<s.count<<','<<visible<<','<<calls<<','<<triangles<<','<<double(narrows)/frames<<','<<double(hits)/frames<<','<<frames<<','<<gpu.values.size()<<','<<timer.skipped<<',';
        for(int k=0;k<10;++k)if(counts[k])csv<<k<<':'<<counts[k]<<';';
        for(auto samples:{&frame,&update,&collision,&upload,&draw,&gpu})samples->Write(csv);csv<<'\n';csv.flush();
    }
    UnloadRenderTexture(sprite);
    }CloseWindow();return 0;
}catch(const std::exception& e){std::cerr<<"Collection stress: "<<e.what()<<'\n';if(IsWindowReady())CloseWindow();return 1;}}
