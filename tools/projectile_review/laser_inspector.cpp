// 本文件提供激光独立审阅、分层/碰撞探针、时序冻结与离屏截图。
#include "laser_renderer.hpp"
#include "raymath.h"
#include <iostream>
#include <array>
#include <cmath>
namespace {
Font reviewFont{};
void label(const char* value,int x,int y,int size,Color color){DrawTextEx(reviewFont,value,{float(x),float(y)},float(size),.2F,color);}
}
int main(int argc,char** argv){try{
    std::filesystem::path root=argc>1?argv[1]:".";bool capture=argc>2&&std::string(argv[2])=="--capture";
    SetConfigFlags(capture?FLAG_WINDOW_HIDDEN:FLAG_MSAA_4X_HINT);InitWindow(1440,900,"Enemy lasers | Independent review");SetTargetFPS(capture?0:60);
    {reviewFont=LoadFontEx((root/".deps/raylib-5.5/examples/text/resources/anonymous_pro_bold.ttf").string().c_str(),32,nullptr,0);SetTextureFilter(reviewFont.texture,TEXTURE_FILTER_BILINEAR);
    review::LaserRenderer renderer(root,32);std::array<Texture2D,2> refs{};for(int i=0;i<2;++i)refs[i]=LoadTexture((root/"assets/projectiles/references"/("enemy-laser-"+std::to_string(i)+".png")).string().c_str());
    RenderTexture2D target=LoadRenderTexture(1440,900);Camera3D camera{{6,4,9},{0,0,0},{0,1,0},40,CAMERA_PERSPECTIVE};
    review::LaserInstance beam;beam.origin={0,-2,0};int mode=0,background=0,frame=0,probePreset=0;bool paused=true,glow=true,collision=false,collisionOnly=false,palette=false,overlap=false,pair=false;float yaw=.6F,pitch=.3F,distance=11;Vector3 probe{.15F,0,0};
    const char* shots[]{"overview","front","side","end","clay","body","halo","collision","palette","warning","grow","sustain","fade","light","crossing","inside"};
    while(!WindowShouldClose()){
        if(capture){beam={};beam.origin={0,-2,0};beam.variant=frame/16;beam.age=3;int s=frame%16;mode=0;collision=palette=overlap=false;background=0;yaw=.6F;pitch=.3F;distance=11;
            pair=s==0;if(s==1)yaw=pitch=0;if(s==2)yaw=PI/2,pitch=0;if(s==3)pitch=1.55F;
            if(s==4)mode=1;if(s==5)mode=2;if(s==6)mode=3;if(s==7)collision=true;if(s==8)palette=true;
            if(s==9)beam.age=.5F;if(s==10)beam.age=1.8F;if(s==11)beam.age=3;if(s==12)beam.age=5.5F;if(s==13)background=1;if(s==14)overlap=true;if(s==15)distance=.3F;
        }else{
            if(IsKeyPressed(KEY_TAB))beam.variant=1-beam.variant;if(IsKeyPressed(KEY_C))beam.color=(beam.color+1)%16;
            if(IsKeyPressed(KEY_T))palette=!palette;if(IsKeyPressed(KEY_O))overlap=!overlap;
            if(IsKeyPressed(KEY_V)){pair=!pair;palette=overlap=false;}
            if(IsKeyPressed(KEY_SIX))collisionOnly=!collisionOnly;
            if(IsKeyPressed(KEY_H)){probePreset=(probePreset+1)%4;probe=beam.origin;
                probe.y+=probePreset==0?beam.length*.5F:probePreset==1?-beam.radius*.8F:probePreset==2?beam.length+beam.radius*.8F:beam.length*.5F;
                if(probePreset==3)probe.x+=beam.radius*1.4F;}
            if(IsKeyPressed(KEY_SPACE))paused=!paused;if(!paused)beam.age=std::fmod(beam.age+GetFrameTime(),6.F);
            if(IsKeyPressed(KEY_COMMA))beam.age=std::max(0.F,beam.age-1.F/60),paused=true;if(IsKeyPressed(KEY_PERIOD))beam.age=std::min(6.F,beam.age+1.F/60),paused=true;
            if(IsKeyPressed(KEY_R))beam.age=3;if(IsKeyPressed(KEY_G))glow=!glow;if(IsKeyPressed(KEY_B))background=1-background;if(IsKeyPressed(KEY_FOUR))collision=!collision;
            for(int i=0;i<4;++i)if(IsKeyPressed(KEY_ONE+i)&&i!=3)mode=i;
            if(IsKeyPressed(KEY_FIVE))mode=3;
            float dt=GetFrameTime();beam.length=Clamp(beam.length+(IsKeyDown(KEY_E)-IsKeyDown(KEY_Q))*dt*2,.05F,20);beam.radius=Clamp(beam.radius+(IsKeyDown(KEY_X)-IsKeyDown(KEY_Z))*dt*.1F,.02F,1);
            probe.x+=(IsKeyDown(KEY_RIGHT)-IsKeyDown(KEY_LEFT))*dt;probe.y+=(IsKeyDown(KEY_UP)-IsKeyDown(KEY_DOWN))*dt;probe.z+=(IsKeyDown(KEY_PAGE_UP)-IsKeyDown(KEY_PAGE_DOWN))*dt;
            if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)){yaw-=GetMouseDelta().x*.008F;pitch=Clamp(pitch+GetMouseDelta().y*.006F,-1.55F,1.55F);}distance=Clamp(distance*std::pow(.9F,GetMouseWheelMove()),.1F,40);
        }
        camera.position={distance*std::cos(pitch)*std::sin(yaw),distance*std::sin(pitch),distance*std::cos(pitch)*std::cos(yaw)};
        std::array<review::LaserInstance,32> items;int count=palette?16:overlap?3:1;for(int i=0;i<count;++i){items[i]=beam;if(palette){items[i].color=i;items[i].origin={(i%8-3.5F)*.75F,-1.2F, i/8*1.2F};items[i].length=2;items[i].radius=.09F;}if(overlap){items[i].color=(beam.color+i*3)%16;items[i].direction=Vector3Normalize(Vector3{(i-1)*.6F,1,(i-1)*.3F});}}
        if(pair){count=2;for(int i=0;i<2;++i){items[i]=beam;items[i].variant=i;items[i].origin.x=i==0?-1:1;}}
        BeginTextureMode(target);ClearBackground(background?Color{218,225,234,255}:Color{15,22,34,255});BeginMode3D(camera);DrawGrid(20,.5F);if(!collisionOnly)renderer.Draw(std::span(items.data(),count),camera,1.6F,mode,glow);
        if(collision||collisionOnly)for(int i=0;i<count;++i){auto& b=items[i];DrawCapsuleWires(b.origin,Vector3Add(b.origin,Vector3Scale(b.direction,b.length)),b.radius,12,4,review::LaserDangerous(b)?GREEN:GRAY);}
        bool hit=review::LaserTouches(beam,probe,.025F);if(!palette&&!overlap)DrawSphereWires(probe,.025F,8,12,hit?ORANGE:GREEN);EndMode3D();
        DrawRectangle(0,0,1440,126,{9,14,24,235});label(TextFormat("ENEMY LASER %i / PENDING HUMAN REVIEW / %s",beam.variant,review::LaserDangerous(beam)?"DANGEROUS":"SAFE"),24,16,24,GOLD);
        label("Tab type | C color | T palette | V compare | O crossing | 1 material 2 clay 3 body 5 halo | 4 collision",24,52,18,RAYWHITE);
        label("Q/E length | Z/X radius | Space pause | , . step | R sustain | G glow | B background | mouse orbit/zoom",24,78,17,RAYWHITE);
        label("6 collision only | H probe presets | Arrows / PgUp PgDn: probe | F12 capture | glow excluded",24,101,16,LIGHTGRAY);
        label(TextFormat("Length %.2f  radius %.3f  age %.3f  visual scale %.3f  probe %s  draws %i",beam.length,beam.radius,beam.age,review::LaserVisualScale(beam),hit?"CONTACT":"CLEAR",renderer.Stats().drawCalls),24,700,20,hit?ORANGE:GREEN);
        auto tex=refs[beam.variant];DrawTexturePro(tex,{0,0,(float)tex.width,(float)tex.height},{20,750,450,450.F*tex.height/tex.width},{0,0},0,WHITE);label("Original reference / 16 colors",500,770,20,LIGHTGRAY);
        EndTextureMode();BeginDrawing();DrawTextureRec(target.texture,{0,0,1440,-900},{0,0},WHITE);EndDrawing();
        if(capture||IsKeyPressed(KEY_F12)){auto out=root/"output/projectile-review/lasers";std::filesystem::create_directories(out);Image img=LoadImageFromTexture(target.texture);ImageFlipVertical(&img);ExportImage(img,(out/(capture?TextFormat("laser-%i-%02i-%s.png",frame/16,frame%16,shots[frame%16]):"manual.png")).string().c_str());UnloadImage(img);}
        if(capture&&++frame==32)break;
    }for(auto t:refs)UnloadTexture(t);UnloadRenderTexture(target);UnloadFont(reviewFont);
    }CloseWindow();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
