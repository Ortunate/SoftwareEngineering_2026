// 本文件在全组制作完成后统一验证实体碰撞、孔洞、壳层透明度、动画和共享实例容量。
#include "collection_renderer.hpp"
#include "gl_state.hpp"
#include "raymath.h"
#include <cmath>
#include <iostream>
#include <vector>
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F fn){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid input accepted");}
int main(int argc,char** argv){try{
    std::filesystem::path root=argv[1];SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(160,160,"Collection tests");
    {
    review::CollectionRenderer renderer(root,32);review::CollisionScratch scratch;scratch.Prepare(512);auto reserved=scratch.crossings.capacity();
    for(int kind=0;kind<10;++kind){auto& c=renderer.Collision(kind);require(renderer.Triangles(kind)<=review::ProjectileKinds()[kind].hardLimit,"Budget");
        for(auto& part:c.Parts())for(auto t:part.triangles){Vector3 p=Vector3Scale(Vector3Add(Vector3Add(t.a,t.b),t.c),1.F/3);require(review::TouchesInstance(c,scratch,{},p,.0001F),"Surface mismatch");}
        Vector3 p{0,kind==7?-.22F:0,0};if(kind!=4&&kind!=8)require(review::TouchesInstance(c,scratch,{},p,.001F),"Volume center empty");
        if(kind==8||kind==4){p.y=kind==8?-.305F:-.335F;require(!review::TouchesInstance(c,scratch,{},p,.015F),"Physical handle hole filled");}
        review::ProjectileInstance transformed;transformed.position={3,2,-4};transformed.scale=1.7F;transformed.rotation=QuaternionFromEuler(.7F,.4F,.9F);
        auto t=c.Parts()[0].triangles[0];auto surface=Vector3Scale(Vector3Add(Vector3Add(t.a,t.b),t.c),1.F/3);auto world=Vector3Add(transformed.position,Vector3RotateByQuaternion(Vector3Scale(surface,transformed.scale),transformed.rotation));
        require(review::TouchesInstance(c,scratch,transformed,world,.0002F),"Transformed surface mismatch");
        transformed.appearance.glowWidth=.025F;transformed.appearance.glowStrength=2;require(review::TouchesInstance(c,scratch,transformed,world,.0002F),"Glow changed collision");
        for(int color=0;color<16;++color){auto a=review::AppearanceFor(kind,color),b=review::AppearanceFor(kind,color%review::ProjectileKinds()[kind].colors);require(ColorToInt(a.bodyColor)==ColorToInt(b.bodyColor),"Native color modulo");}
    }
    require(scratch.crossings.capacity()==reserved,"Collision query allocation");
    auto& fire=renderer.Collision(7);require(!review::TouchesInstance(fire,scratch,{}, {0,.45F,0},.01F),"Decorative flame is collidable");
    auto& bubble=renderer.Collision(9);
    require(review::TouchesInstance(bubble,scratch,{}, {0,0,.17F},.001F),"Bubble inner sphere empty");
    for(Vector3 point:std::vector<Vector3>{{.30F,0,0},{0,.38F,0},{0,0,.21F}})
        require(review::TouchesInstance(bubble,scratch,{},point,.001F),"Volume inside halo is empty");
    for(Vector3 point:std::vector<Vector3>{{.43F,0,0},{0,.46F,0},{0,0,.49F}})
        require(!review::TouchesInstance(bubble,scratch,{},point,.005F),"White halo collides");
    auto& knife=renderer.Collision(8);
    for(float z:{-.12F,0.F,.12F})require(!review::TouchesInstance(knife,scratch,{}, {0,-.305F,z},.015F),"Knife through bore blocked");
    require(review::TouchesInstance(knife,scratch,{}, {.065F,-.305F,0},.005F),"Cylindrical handle wall missing");
    Camera3D camera{{0,0,2},{0,0,0},{0,1,0},40,CAMERA_PERSPECTIVE};std::vector<review::CollectionInstance> items(1);items[0].type=9;items[0].transform.appearance=review::AppearanceFor(9,0);
    RenderTexture2D target=LoadRenderTexture(160,160);
    auto render=[&](float time,int mode=0){renderer.Prepare(items,camera,1);BeginTextureMode(target);ClearBackground({13,21,34,255});BeginMode3D(camera);renderer.Draw(time,false,mode);EndMode3D();EndTextureMode();return LoadImageFromTexture(target.texture);};
    items[0].shellOpacity=0;Image hidden=render(0);items[0].shellOpacity=1;Image visible=render(0);int changed=0;
    for(int y=0;y<160;++y)for(int x=0;x<160;++x)changed+=ColorToInt(GetImageColor(hidden,x,y))!=ColorToInt(GetImageColor(visible,x,y));
    require(changed>300,"Transparent shell invisible");UnloadImage(hidden);UnloadImage(visible);
    Image stillA=render(0),stillB=render(.17F);changed=0;
    for(int y=0;y<160;++y)for(int x=0;x<160;++x)changed+=ColorToInt(GetImageColor(stillA,x,y))!=ColorToInt(GetImageColor(stillB,x,y));
    require(changed==0,"Static bubble filaments animated");UnloadImage(stillA);UnloadImage(stillB);
    Image shell=render(0,5),filaments=render(0,6);int shellOnly=0,filamentOnly=0;
    for(int y=0;y<160;++y)for(int x=0;x<160;++x){bool s=ColorToInt(GetImageColor(shell,x,y))!=ColorToInt(Color{13,21,34,255});
        bool f=ColorToInt(GetImageColor(filaments,x,y))!=ColorToInt(Color{13,21,34,255});shellOnly+=s&&!f;filamentOnly+=f&&!s;}
    require(shellOnly>100&&filamentOnly>100,"Shell/filament inspection layers not independent");UnloadImage(shell);UnloadImage(filaments);
    // 相机进入泡内部后仍能看到壳层，不应因背面剔除整颗消失。
    camera.position={0,0,.1F};camera.target={0,0,-1};visible=render(0);changed=0;
    for(int y=0;y<160;++y)for(int x=0;x<160;++x)changed+=ColorToInt(GetImageColor(visible,x,y))!=ColorToInt(Color{13,21,34,255});
    require(changed>300,"Bubble interior invisible");UnloadImage(visible);camera.position={0,0,2};camera.target={0,0,0};
    items[0].type=7;items[0].transform.appearance=review::AppearanceFor(7,0);Image a=render(0),b=render(1.F/12);changed=0;
    for(int y=0;y<160;++y)for(int x=0;x<160;++x)changed+=ColorToInt(GetImageColor(a,x,y))!=ColorToInt(GetImageColor(b,x,y));
    require(changed>100,"Fire animation static");UnloadImage(a);UnloadImage(b);
    items[0].type=0;items.push_back(items[0]);items[1].transform.position={100,0,0};renderer.Prepare(items,camera,1);require(renderer.Stats().visible==1,"Frustum culling");
    BeginDrawing();BeginMode3D(camera);review::GLState before;renderer.Draw(0);review::GLState after;
    require(before.program==after.program&&before.vao==after.vao&&before.depthWrite==after.depthWrite&&before.blend==after.blend&&before.srcRGB==after.srcRGB&&before.dstRGB==after.dstRGB&&before.cullFace==after.cullFace,"GL state leak");EndMode3D();EndDrawing();
    // 仅比较渲染器调用边界；窗口提交和 raylib 的模式切换不属于被测函数。
    GLint polygonBefore[2]{},polygonAfter[2]{};BeginDrawing();BeginMode3D(camera);
    glGetIntegerv(GL_POLYGON_MODE,polygonBefore);renderer.Draw(0,false,4);glGetIntegerv(GL_POLYGON_MODE,polygonAfter);
    require(polygonBefore[0]==polygonAfter[0]&&polygonBefore[1]==polygonAfter[1],"Wireframe polygon state leak");EndMode3D();EndDrawing();
    items.resize(33);rejects([&]{renderer.Prepare(items,camera,1);});items.resize(1);items[0].type=10;rejects([&]{renderer.Prepare(items,camera,1);});items[0].type=0;items[0].shellOpacity=-1;rejects([&]{renderer.Prepare(items,camera,1);});
    require(glGetError()==GL_NO_ERROR,"OpenGL error");UnloadRenderTexture(target);
    }CloseWindow();std::cout<<"All ten types: collision, palette, shell, animation and renderer checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';if(IsWindowReady())CloseWindow();return 1;}}
