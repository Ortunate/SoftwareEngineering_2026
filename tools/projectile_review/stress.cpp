// 本文件执行可复现的可见窗口弹幕压力实验；结果只代表独立通道，不等于整场游戏。
#include "classic_renderer.hpp"
#include "gl_state.hpp"
#include "gameplay/bullets/system.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <cstdlib>
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point a){return std::chrono::duration<double,std::milli>(Clock::now()-a).count();}
struct Samples {
    std::vector<double> v;
    Samples(){v.reserve(250000);}
    void add(double x){if(v.size()==v.capacity())throw std::runtime_error("Sample capacity exceeded");v.push_back(x);}
    void write(std::ostream& o){if(v.empty()){o<<",NA,NA,NA,NA";return;}double mean=std::accumulate(v.begin(),v.end(),0.)/v.size();std::sort(v.begin(),v.end());o<<','<<mean;for(double q:{.5,.95,.99})o<<','<<v[std::min(v.size()-1,(size_t)(q*v.size()))];}
};
// 异步计时槽仅在启动时分配；忙槽跳过采样并计数，不在帧内等待 GPU。
struct Timers {
    std::array<GLuint,32> ids{};std::array<bool,32> busy{},record{};int active=-1;bool supported=false;size_t skipped=0;
    Timers(){supported=glGenQueries&&glGetQueryObjectui64v&&glBeginQuery;if(supported)glGenQueries(32,ids.data());}
    ~Timers(){if(supported)glDeleteQueries(32,ids.data());}
    void poll(Samples& out){if(!supported)return;for(int i=0;i<32;++i)if(busy[i]){GLint ready=0;glGetQueryObjectiv(ids[i],GL_QUERY_RESULT_AVAILABLE,&ready);if(ready){GLuint64 ns;glGetQueryObjectui64v(ids[i],GL_QUERY_RESULT,&ns);if(record[i])out.add(ns/1e6);busy[i]=false;}}}
    void begin(bool measured){if(!supported)return;for(int i=0;i<32;++i)if(!busy[i]){active=i;record[i]=measured;glBeginQuery(GL_TIME_ELAPSED,ids[i]);return;}if(measured)++skipped;}
    void end(){if(active>=0){glEndQuery(GL_TIME_ELAPSED);busy[active]=true;active=-1;}}
};
struct Case {const char* name;size_t count;bool moving,glow,multi;int layout;bool sprite=false;};
int main(int argc,char** argv){try{
    fs::path root=fs::absolute(argc>1?argv[1]:".");bool quick=argc>2&&std::string(argv[2])=="--quick";
    fs::path out=root/"output/projectile-review/performance";fs::create_directories(out);
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);InitWindow(1920,1080,"Kunai stress | 1920x1080 | do not minimize");
    ClearWindowState(FLAG_VSYNC_HINT);SetTargetFPS(0);
    if(GetRenderWidth()!=1920||GetRenderHeight()!=1080)throw std::runtime_error("Required 1920x1080 framebuffer unavailable");
    {
    constexpr size_t capacity=koumakyou::BulletSystem::kCapacity;
    review::ClassicRenderer renderer(root/"assets/projectiles/kunai_classic/kunai_classic.gltf",root/"tools/projectile_review/shaders",capacity);
    review::CollisionAsset collider(root/"assets/projectiles/kunai_classic/collision.bin");review::CollisionScratch scratch;scratch.Prepare(collider.MaxPartTriangles());
    std::vector<review::ProjectileInstance> instances(capacity);std::vector<Vector3> bases(capacity);
    // 简化二维基线：同样的朝向、尺寸、数量，使用一次生成的白色三角刃/环 sprite。
    RenderTexture2D spriteTarget=LoadRenderTexture(64,128);BeginTextureMode(spriteTarget);ClearBackground(BLANK);
    DrawTriangle({32,0},{4,78},{60,78},WHITE);DrawRectangle(28,75,8,18,WHITE);DrawRing({32,108},11,19,0,360,32,WHITE);EndTextureMode();
    Camera3D camera{{0,0,12},{0,0,0},{0,1,0},50,CAMERA_PERSPECTIVE};
    std::ofstream hardware(out/(quick?"smoke-hardware.txt":"hardware.txt"));
    hardware<<"GPU: "<<glGetString(GL_RENDERER)<<"\nVendor: "<<glGetString(GL_VENDOR)<<"\nOpenGL: "<<glGetString(GL_VERSION)<<"\nCPU: "<<(std::getenv("PROCESSOR_IDENTIFIER")?std::getenv("PROCESSOR_IDENTIFIER"):"unavailable")
      <<"\nFramebuffer: "<<GetRenderWidth()<<'x'<<GetRenderHeight()<<"\nInstance capacity: "<<capacity<<"\nGPU instance buffer bytes: "<<renderer.BufferBytes()<<"\nInstance CPU bytes: "<<instances.size()*sizeof(instances[0])<<"\nShared geometry triangles: "<<collider.TriangleCount()<<"\nWarmup seconds: "<<(quick?.2:3)<<"\nMeasure seconds: "<<(quick?.4:10)<<"\nRepeats: "<<(quick?1:3)<<"\nVsync off, no FPS cap, no MSAA, visible window.\n";
    std::array cases{
        Case{"normal-1024-static",1024,false,false,true,0},Case{"normal-4096-moving",4096,true,false,true,0},
        Case{"normal-8192-static",capacity,false,false,true,0},Case{"normal-8192-moving",capacity,true,false,true,0},
        Case{"normal-8192-glow",capacity,true,true,true,0},Case{"normal-8192-one-color",capacity,true,true,false,0},
        Case{"dense-8192-glow",capacity,true,true,true,1},Case{"near-8192-glow",capacity,true,true,true,2},
        Case{"culled-8192",capacity,true,true,true,3},Case{"collision-heavy-8192",capacity,true,false,true,4},
        Case{"sprite-1024",1024,true,false,true,0,true},Case{"sprite-4096",4096,true,false,true,0,true},Case{"sprite-8192",capacity,true,false,true,0,true}};
    std::ofstream csv(out/(quick?"smoke.csv":"results.csv"));
    csv<<"case,repeat,submitted,visible,draw_calls,triangles,narrow_per_frame,hits_per_frame,frames,gpu_samples,gpu_skipped";
    for(auto stage:{"frame","update","collision","upload","draw_cpu","gpu"})for(auto stat:{"mean","p50","p95","p99"})csv<<','<<stage<<'_'<<stat<<"_ms";csv<<'\n';
    for(auto c:cases)for(int repeat=1;repeat<=(quick?1:3);++repeat){
        for(size_t i=0;i<c.count;++i){auto& p=instances[i];p={};p.scale=.18F;p.appearance=review::ClassicPalette(c.multi?i%16:2);
            bases[i]={(float(i%128)-63.5F)*.125F,(float(i/128)-31.5F)*.115F,float(i%7)*.05F};
            if(c.layout==1)bases[i]=Vector3Scale(bases[i],.12F);
            if(c.layout==2)bases[i]=Vector3Scale(bases[i],.1F),bases[i].z=10.4F,p.scale=.5F;
            if(c.layout==3&&i%4)bases[i].x+=40;
            if(c.layout==4)bases[i]={.025F*std::sin((float)i),.025F*std::cos((float)i),0};
            p.position=bases[i];
        }
        Samples frame,update,collision,upload,draw,gpu;Timers timer;
        size_t narrows=0,hits=0,frames=0;int visible=0,calls=0,triangles=0;
        double start=GetTime(),warm=quick?.2:3,duration=quick?.4:10;
        std::cout<<c.name<<" repeat "<<repeat<<std::endl;
        while(GetTime()-start<warm+duration){
            if(WindowShouldClose()||IsWindowMinimized())throw std::runtime_error("Benchmark aborted: closed/minimized window");
            auto begin=Clock::now();double t=GetTime()-start;bool measuring=t>=warm;timer.poll(gpu);
            auto stage=Clock::now();
            if(c.moving)for(size_t i=0;i<c.count;++i){auto& p=instances[i];float a=float(t)*.8F+float(i)*.017F;p.position=Vector3Add(bases[i],{.015F*std::sin(a),.015F*std::cos(a),0});p.rotation=QuaternionFromAxisAngle({0,0,1},a);}
            double updateMs=ms(stage);stage=Clock::now();size_t narrow=0,hit=0;
            for(size_t i=0;i<c.count;++i)hit+=review::TouchesInstance(collider,scratch,instances[i],{0,0,0},.012F,&narrow);
            double collisionMs=ms(stage);stage=Clock::now();
            if(!c.sprite)renderer.Prepare(std::span(instances.data(),c.count),camera,1920.F/1080);
            double uploadMs=ms(stage);
            BeginDrawing();ClearBackground({18,25,38,255});BeginMode3D(camera);rlDrawRenderBatchActive();timer.begin(measuring);stage=Clock::now();
            if(!c.sprite){renderer.Draw(c.glow);visible=(int)renderer.Stats().visible;calls=renderer.Stats().drawCalls;triangles=renderer.Stats().triangles;}
            else{for(size_t i=0;i<c.count;++i)DrawBillboardPro(camera,spriteTarget.texture,{0,0,64,-128},instances[i].position,{0,1,0},{.51F*instances[i].scale,instances[i].scale},{.255F*instances[i].scale,.5F*instances[i].scale},float(t)*45+float(i),instances[i].appearance.bodyColor);
                rlDrawRenderBatchActive();visible=(int)c.count;calls=(int)((c.count+8190)/8191);triangles=(int)c.count*2;}
            double drawMs=ms(stage);timer.end();EndMode3D();DrawText(TextFormat("%s | %i / %i | repeat %i | %s",c.name,visible,(int)c.count,repeat,measuring?"MEASURE":"WARMUP"),20,20,20,WHITE);EndDrawing();
            if(measuring){frame.add(ms(begin));update.add(updateMs);collision.add(collisionMs);upload.add(uploadMs);draw.add(drawMs);narrows+=narrow;hits+=hit;++frames;}
        }
        glFinish();timer.poll(gpu); // 仅测量区间结束后收集尾部结果。
        csv<<c.name<<','<<repeat<<','<<c.count<<','<<visible<<','<<calls<<','<<triangles<<','<<double(narrows)/frames<<','<<double(hits)/frames<<','<<frames<<','<<gpu.v.size()<<','<<timer.skipped;
        for(auto s:{&frame,&update,&collision,&upload,&draw,&gpu})s->write(csv);csv<<'\n';csv.flush();
    }
    UnloadRenderTexture(spriteTarget);
    }CloseWindow();return 0;
}catch(const std::exception& e){std::cerr<<"Stress failed: "<<e.what()<<'\n';if(IsWindowReady())CloseWindow();return 1;}}
