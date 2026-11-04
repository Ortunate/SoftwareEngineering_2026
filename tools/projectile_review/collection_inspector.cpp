// 本文件提供十类普通弹体的独立对照审阅、透明层拆解、冻结动画和真实碰撞探针。
#include "collection_renderer.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
namespace fs=std::filesystem;
namespace {
Font font{};
Rectangle viewRect(){float scale=std::min(GetScreenWidth()/1520.F,GetScreenHeight()/1000.F);return {(GetScreenWidth()-1520*scale)*.5F,(GetScreenHeight()-1000*scale)*.5F,1520*scale,1000*scale};}
Vector2 uiMouse(){auto r=viewRect();auto p=GetMousePosition();return {(p.x-r.x)*1520/r.width,(p.y-r.y)*1000/r.height};}
bool approvedAt(const fs::path& path){
    // 仅读取人工状态字段；缺失或无记录时保持待验收，不以自动测试结果代替。
    std::ifstream file(path);std::string record((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    auto key=record.find("\"humanAcceptance\"");if(key==std::string::npos)return false;
    auto colon=record.find(':',key);if(colon==std::string::npos)return false;auto begin=record.find('"',colon);
    if(begin==std::string::npos)return false;auto end=record.find('"',begin+1);
    return end!=std::string::npos&&record.substr(begin+1,end-begin-1)=="approved";
}
void text(const char* s,int x,int y,int size,Color c){DrawTextEx(font,s,{(float)x,(float)y},(float)size,.2F,c);}
void collisionMesh(const review::CollisionAsset& a,bool wire){
    rlDrawRenderBatchActive();rlDisableBackfaceCulling();rlBegin(wire?RL_LINES:RL_TRIANGLES);rlColor4ub(56,218,176,255);
    for(auto& p:a.Parts())for(auto t:p.triangles){Vector3 v[]{t.a,t.b,t.c};for(int j=0;j<3;++j){rlVertex3f(v[j].x,v[j].y,v[j].z);if(wire){auto q=v[(j+1)%3];rlVertex3f(q.x,q.y,q.z);}}}rlEnd();rlDrawRenderBatchActive();rlEnableBackfaceCulling();
}
void capture(RenderTexture2D canvas,const fs::path& path){Image i=LoadImageFromTexture(canvas.texture);ImageFlipVertical(&i);bool ok=ExportImage(i,path.string().c_str());UnloadImage(i);if(!ok)throw std::runtime_error("Capture failed");}
}
int main(int argc,char** argv){try{
    fs::path root=fs::absolute(argc>1?argv[1]:".");bool overview=argc>2&&std::string(argv[2])=="--capture-overview";
    bool revision=argc>2&&std::string(argv[2])=="--capture-revision";
    bool automated=overview||revision||(argc>2&&std::string(argv[2])=="--capture");
    auto out=root/"output/projectile-review/collection";fs::create_directories(out);
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_MSAA_4X_HINT|(automated?FLAG_WINDOW_HIDDEN:FLAG_WINDOW_RESIZABLE));InitWindow(1280,842,"Projectile collection | Independent review");SetWindowMinSize(1064,700);SetTargetFPS(automated?0:60);
    {
    font=LoadFontEx((root/".deps/raylib-5.5/examples/text/resources/anonymous_pro_bold.ttf").string().c_str(),32,nullptr,0);if(!font.texture.id)font=GetFontDefault();SetTextureFilter(font.texture,TEXTURE_FILTER_BILINEAR);
    review::CollectionRenderer renderer(root,32);std::array<Texture2D,10> references{};std::array<bool,10> approved{};
    for(int k=0;k<10;++k){references[k]=LoadTexture((root/"assets/projectiles/references"/TextFormat("enemy-%02i.png",k)).string().c_str());
        approved[k]=approvedAt(root/"assets/projectiles"/review::ProjectileKinds()[k].directory/"asset.json");}
    Texture2D frames=LoadTexture((root/"assets/projectiles/references/fireball-frames.png").string().c_str());
    RenderTexture2D stage=LoadRenderTexture(1150,830),canvas=LoadRenderTexture(1520,1000),small=LoadRenderTexture(128,128);
    int kind=5,color=2,mode=0,background=0,frame=overview?200:revision?180:0,comparison=0,probePreset=0;bool gallery=false,glow=true,probeOn=false,paused=false,overlap=false,all=false;
    float yaw=-.3F,pitch=.12F,distance=1.9F,time=0,strength=.55F,width=.015F,opacity=.65F,radius=.02F;
    Vector3 probe{};review::CollisionScratch scratch;scratch.Prepare(512);
    const char* shots[]{"material","front","side","back","clay","wire","collision","overlay","probe-center","probe-surface","palette","glow-off","light","layers","overlap","phase-00","phase-05","phase-10","phase-15","near"};
    while(!WindowShouldClose()){
        if(automated){kind=std::min(frame/20,9);all=frame==200;int s=frame%20;color=kind==6||kind==8||kind==9?1:kind==7?0:2;mode=0;gallery=probeOn=overlap=false;glow=true;background=0;time=0;yaw=-.3F;pitch=.12F;distance=1.9F;
            if(s==1)yaw=pitch=0;if(s==2)yaw=PI/2,pitch=0;if(s==3)yaw=PI,pitch=0;
            if(s>=4&&s<=7)mode=s-3;
            if(s==8||s==9){probeOn=true;probe=s==8?Vector3{0,kind==7?-.22F:kind==8?-.305F:kind==4?-.335F:0,0}:Vector3{kind==8?.085F:kind==4?.115F:.49F,kind==8?-.305F:kind==4?-.335F:0,0};if(kind==5||kind==2){if(s==9)probe={0,.499F,0};}mode=4;}
            if(s==10)gallery=true;if(s==11)glow=false;if(s==12)background=1;if(s==13)mode=6;if(s==14)overlap=true;
            if(s>=15&&s<=18)time=(s-15)*5.F/60;if(s==19)distance=.65F;
            if(frame>200){kind=9;color=0;mode=0;probeOn=false;all=false;yaw=pitch=0;distance=1.9F;comparison=0;
                if(frame==201||frame==202)comparison=frame-200;
                if(frame>=203&&frame<=205)mode=frame==203?5:frame==204?7:8;
                if(frame==206||frame==207){mode=4;probeOn=true;probe={frame==206?.32F:.52F,0,0};}
                if(frame==208){kind=8;color=1;yaw=PI/2;}}
        }else{
            if(IsKeyPressed(KEY_TAB)){kind=(kind+(IsKeyDown(KEY_LEFT_SHIFT)?9:1))%10;color=0;comparison=0;}
            for(int k=0;k<10;++k){Rectangle r{18.F,143.F+k*24,300,23};if(CheckCollisionPointRec(uiMouse(),r)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){kind=k;color=0;comparison=0;}}
            if(IsKeyPressed(KEY_C))color=(color+1)%review::ProjectileKinds()[kind].colors;
            if(IsKeyPressed(KEY_A)){all=!all;comparison=0;}
            if(IsKeyPressed(KEY_V)){comparison=(comparison+1)%3;all=gallery=overlap=false;}
            if(IsKeyPressed(KEY_EIGHT))mode=7;if(IsKeyPressed(KEY_NINE))mode=8;
            if(IsKeyPressed(KEY_J)){kind=9;color%=5;comparison=0;all=gallery=overlap=false;probePreset=(probePreset+1)%3;probe={probePreset==0?.32F:probePreset==1?.45F:.52F,0,0};probeOn=true;}
            for(int n=0;n<7;++n)if(IsKeyPressed(KEY_ONE+n))mode=n,gallery=false;
            if(IsKeyPressed(KEY_T)){gallery=!gallery;comparison=0;}if(IsKeyPressed(KEY_G))glow=!glow;if(IsKeyPressed(KEY_B))background=(background+1)%3;
            if(IsKeyPressed(KEY_O))overlap=!overlap;if(IsKeyPressed(KEY_P))probeOn=!probeOn;
            if(IsKeyPressed(KEY_H)){probe={0,kind==8?-.305F:kind==4?-.335F:kind==7?-.22F:0,0};probeOn=true;}
            if(IsKeyPressed(KEY_SPACE))paused=!paused;
            if(!paused)time+=GetFrameTime();if(IsKeyPressed(KEY_COMMA))time=std::max(0.F,time-1.F/60),paused=true;if(IsKeyPressed(KEY_PERIOD))time+=1.F/60,paused=true;
            if(IsKeyDown(KEY_Q))strength=Clamp(strength-GetFrameTime(),0,2);if(IsKeyDown(KEY_E))strength=Clamp(strength+GetFrameTime(),0,2);
            if(IsKeyDown(KEY_LEFT_BRACKET))opacity=Clamp(opacity-GetFrameTime()*.4F,0,1);if(IsKeyDown(KEY_RIGHT_BRACKET))opacity=Clamp(opacity+GetFrameTime()*.4F,0,1);
            if(uiMouse().x>340){if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)){float scale=1520/viewRect().width;yaw-=GetMouseDelta().x*scale*.008F;pitch=Clamp(pitch+GetMouseDelta().y*scale*.006F,-1.4F,1.4F);}distance=Clamp(distance*std::pow(.9F,GetMouseWheelMove()),.15F,6.F);}
            if(IsKeyPressed(KEY_R))yaw=-.3F,pitch=.12F,distance=1.9F;
            if(probeOn){float d=GetFrameTime()*.12F;probe.x+=(IsKeyDown(KEY_RIGHT)-IsKeyDown(KEY_LEFT))*d;probe.y+=(IsKeyDown(KEY_UP)-IsKeyDown(KEY_DOWN))*d;probe.z+=(IsKeyDown(KEY_PAGE_UP)-IsKeyDown(KEY_PAGE_DOWN))*d;radius=Clamp(radius+(IsKeyDown(KEY_EQUAL)-IsKeyDown(KEY_MINUS))*d*.2F,.001F,.15F);}
        }
        Camera3D camera{{distance*std::cos(pitch)*std::sin(yaw),distance*std::sin(pitch),distance*std::cos(pitch)*std::cos(yaw)},{0,0,0},{0,1,0},40,CAMERA_PERSPECTIVE};
        if(gallery)camera.position={0,0,4.2F};if(all)camera.position={0,0,4.4F};std::array<review::CollectionInstance,32> items{};int count=all?10:gallery?review::ProjectileKinds()[kind].colors:overlap?3:1;
        for(int i=0;i<count;++i){auto& p=items[i];p.type=kind;p.transform.appearance=review::AppearanceFor(kind,gallery?i:overlap?(color+i)%review::ProjectileKinds()[kind].colors:color);p.transform.appearance.glowStrength=strength;p.transform.appearance.glowWidth=width;p.shellOpacity=opacity;
            if(gallery){int cols=count>8?8:count;p.transform.scale=.60F;p.transform.position={(i%cols-(cols-1)*.5F)*.44F,(count>8?.43F:0)-i/cols*.86F,0};}
            if(overlap)p.transform.position={(i-1)*.32F,0,(i-1)*.28F};}
        if(all)for(int i=0;i<10;++i){items[i].type=i;items[i].transform.appearance=review::AppearanceFor(i,i==6||i==8?1:i==9?0:2);items[i].transform.scale=.8F;items[i].transform.position={(i%5-2)*.88F,(.5F-i/5)*1.15F,0};}
        if(comparison){camera.position={0,0,4.4F};count=3;
            for(int i=0;i<3;++i){int type=i==0?1:i==1?3:6;auto& item=items[i];item={};item.type=type;
                item.transform.appearance=review::AppearanceFor(type,type==6?1:2);item.shellOpacity=opacity;
                item.transform.scale=comparison==1?.9F:(type==6?1.1F:.55F);item.transform.position={(i-1)*1.15F,0,0};}}
        // 固定相机的小画布帮助判断游戏尺寸下的辨识度；与主视图任意放大独立。
        review::CollectionInstance icon;icon.type=kind;icon.transform.appearance=review::AppearanceFor(kind,color);icon.transform.appearance.glowStrength=strength;icon.shellOpacity=opacity;
        Camera3D iconCamera{{0,0,1.7F},{0,0,0},{0,1,0},40,CAMERA_PERSPECTIVE};renderer.Prepare(std::span(&icon,1),iconCamera,1);
        BeginTextureMode(small);ClearBackground({21,29,43,255});BeginMode3D(iconCamera);renderer.Draw(time,glow);EndMode3D();EndTextureMode();
        renderer.Prepare(std::span(items.data(),count),camera,1150.F/830);
        auto& shape=renderer.Collision(kind);bool hit=review::TouchesInstance(shape,scratch,review::ProjectileInstance{},probe,radius);
        BeginTextureMode(stage);Color bg=background==0?Color{21,29,43,255}:background==1?Color{224,229,235,255}:Color{53,64,75,255};ClearBackground(bg);
        // 透过球壳能看到背景网格；装饰层不伪造不透明实体深度。
        for(int j=0;j<24;++j)DrawLine(j*50,0,j*50,830,Fade(background==1?GRAY:LIGHTGRAY,.08F));
        for(int j=0;j<17;++j)DrawLine(0,j*50,1150,j*50,Fade(background==1?GRAY:LIGHTGRAY,.08F));
        BeginMode3D(camera);
        if(mode!=3)renderer.Draw(time,glow,mode==1?1:mode==2?4:mode==5?2:mode==6?3:mode==7?5:mode==8?6:0);
        if(mode==3||mode==4){if(mode==4)rlDisableDepthTest();
            for(int i=0;i<count;++i){auto& t=items[i].transform;rlPushMatrix();rlTranslatef(t.position.x,t.position.y,t.position.z);auto rotation=MatrixToFloatV(QuaternionToMatrix(t.rotation));rlMultMatrixf(rotation.v);rlScalef(t.scale,t.scale,t.scale);collisionMesh(renderer.Collision(items[i].type),mode==4);rlPopMatrix();}rlEnableDepthTest();}
        if(probeOn&&!gallery&&!overlap&&!all&&!comparison)DrawSphereWires(probe,radius,12,20,hit?ORANGE:GREEN);
        EndMode3D();EndTextureMode();BeginTextureMode(canvas);ClearBackground({13,19,29,255});
        text("PROJECTILE COLLECTION",18,24,24,RAYWHITE);text("Independent asset review",18,61,16,LIGHTGRAY);
        text(approved[kind]?"HUMAN REVIEW APPROVED":"PENDING HUMAN REVIEW",18,98,17,approved[kind]?GREEN:GOLD);
        for(int k=0;k<10;++k){if(kind==k)DrawRectangle(14,143+k*24,310,23,{37,57,76,255});text(TextFormat("%i  %s",k,review::ProjectileKinds()[k].title),22,146+k*24,15,kind==k?WHITE:LIGHTGRAY);}
        const char* controls[]{"Tab / click: choose type","C color / T palette / A overview","1 material / 2 clay / 3 wire","4 collision / 5 overlay","6 body / 7 all decorations","8 shell / 9 filaments or flames","V orbs: equal / relative / off","G glow / B background / O overlap","P probe / H hole / J bubble zones","Arrows XY / PgUp PgDn Z / +/- size","Q/E glow   [ / ] opacity","Space pause / , . frame step","Mouse orbit / wheel / R reset","F12 capture"};
        for(int j=0;j<14;++j)text(controls[j],18,401+j*23,14,LIGHTGRAY);
        for(int c=0;c<review::ProjectileKinds()[kind].colors;++c){Rectangle r{18.F+(c%8)*37,741.F+(c/8)*32,30,26};DrawRectangleRec(r,review::AppearanceFor(kind,c).bodyColor);if(color==c)DrawRectangleLinesEx(r,2,WHITE);if(!automated&&CheckCollisionPointRec(uiMouse(),r)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))color=c;}
        text(TextFormat("C%02i  glow %.2f  alpha %.2f",color,strength,opacity),18,818,15,WHITE);
        text(kind==7?"ORIGINAL 4 ANIMATION PHASES":"ORIGINAL SPRITE REFERENCE",18,859,13,GOLD);Texture2D ref=kind==7?frames:references[kind];
        float h=std::min(88.F,310.F*ref.height/ref.width);DrawTexturePro(ref,{0,0,(float)ref.width,(float)ref.height},{18,884,310,h},{0,0},0,WHITE);
        if(comparison){DrawRectangle(14,852,315,145,{13,19,29,255});int types[]{1,3,6};
            for(int i=0;i<3;++i){auto t=references[types[i]];text(TextFormat("ORIGINAL TYPE %i",types[i]),72,870+i*43,12,GOLD);
                DrawTexturePro(t,{types[i]==6?128.F:256.F,0,128,152},{18,854.F+i*43,34,40.375F},{0,0},0,WHITE);}}
        text(comparison?"THREE ORBS / ORIGINAL FEATURES":all?"TEN TYPES / NINE NEW ASSETS":TextFormat("TYPE %i / %s",kind,review::ProjectileKinds()[kind].title),355,24,26,WHITE);
        int pending=0;for(bool accepted:approved)if(!accepted)++pending;
        text(comparison?"Type 1: white core + dark gap + ring / Type 3: compact / Type 6: broad soft edge":all?TextFormat("Shared geometry by type / 118 effective colors / %i types pending review",pending):TextFormat("%i triangles / %i effective colors / one shared mesh",renderer.Triangles(kind),review::ProjectileKinds()[kind].colors),355,58,17,LIGHTGRAY);
        DrawTextureRec(stage.texture,{0,0,1150,-830},{350,90},WHITE);
        if(comparison){text(comparison==1?"ORB COMPARISON / SAME COLOR + EQUAL SIZE":"ORB COMPARISON / SPRITE SIZE RATIO 1 : 1 : 2",390,110,20,GOLD);
            text("RING ORB",540,690,18,WHITE);text("SOLID ORB",840,690,18,WHITE);text("LARGE ORB",1130,690,18,WHITE);}
        if(!comparison){DrawRectangle(1115,797,372,117,{13,19,29,245});text("FIXED VIEW / CANVAS PIXELS",1128,806,13,GOLD);
        const int pixels[]{8,16,32,64};for(int i=0;i<4;++i){int size=pixels[i];DrawTexturePro(small.texture,{0,0,128,-128},{1135.F+i*84,895.F-size,(float)size,(float)size},{0,0},0,WHITE);text(TextFormat("%ipx",size),1130+i*84,897,12,LIGHTGRAY);}}
        text(TextFormat("%i submitted / %i visible | %i draws | %i triangles | time %.3fs",count,(int)renderer.Stats().visible,renderer.Stats().drawCalls,renderer.Stats().triangles,time),355,937,16,LIGHTGRAY);
        bool showProbe=probeOn&&!gallery&&!overlap&&!all&&!comparison;
        text(showProbe?TextFormat("%s  radius %.3f | XYZ %.3f %.3f %.3f",hit?"CONTACT":"CLEAR",radius,probe.x,probe.y,probe.z):"Collision: stable solid volume. Glow and decorative flames never change it.",355,969,15,showProbe?(hit?ORANGE:GREEN):LIGHTGRAY);
        EndTextureMode();BeginDrawing();ClearBackground(BLACK);DrawTexturePro(canvas.texture,{0,0,1520,-1000},viewRect(),{0,0},0,WHITE);EndDrawing();
        if(automated||IsKeyPressed(KEY_F12)){const char* extra[]{"orbs-equal","orbs-relative","bubble-inner","bubble-shell","bubble-filaments","bubble-inside-halo-contact","bubble-outside-clear","knife-cylinder-side"};
            std::string name=automated?(frame>200?extra[frame-201]:all?"collection-overview":TextFormat("type-%02i-%02i-%s",kind,frame%20,shots[frame%20])):TextFormat("manual-%02i-%i",kind,(int)(GetTime()*1000));capture(canvas,out/(name+".png"));}
        if(automated&&(overview||++frame==209))break;
    }
    for(auto t:references)UnloadTexture(t);UnloadTexture(frames);UnloadRenderTexture(stage);UnloadRenderTexture(canvas);UnloadRenderTexture(small);if(font.texture.id!=GetFontDefault().texture.id)UnloadFont(font);
    }CloseWindow();return 0;
}catch(const std::exception& e){std::cerr<<"Collection review: "<<e.what()<<'\n';if(IsWindowReady())CloseWindow();return 1;}}
