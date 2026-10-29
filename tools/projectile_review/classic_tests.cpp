// 本文件验证低面数实体的真实孔洞、世界变换、显示剔除、光效独立性及 GL 状态恢复。
#include "resources/projectile_paths.hpp"
#include "classic_renderer.hpp"
#include "gl_state.hpp"
#include "raymath.h"
#include <iostream>
#include <vector>
#include <limits>
#include <cmath>
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
template<class F>void rejects(F f){bool threw=false;try{f();}catch(const std::exception&){threw=true;}require(threw,"Expected invalid input rejection");}
int main(int argc,char** argv){try{
    std::filesystem::path root=argv[1];review::CollisionAsset asset(root/"assets/projectiles/kunai_classic/collision.bin");
    require(asset.TriangleCount()==146&&asset.Parts().size()==3,"Geometry budget");
    review::CollisionScratch scratch;scratch.Prepare(asset.MaxPartTriangles());auto capacity=scratch.crossings.capacity();
    review::ProjectileInstance instance;
    require(!review::TouchesInstance(asset,scratch,instance,{0,-.335F,0},.02F),"Ring hole must be empty");
    require(review::TouchesInstance(asset,scratch,instance,{.115F,-.335F,0},.005F),"Ring material must collide");
    require(review::TouchesInstance(asset,scratch,instance,{0,-.16F,0},.001F),"Connector is solid");
    require(!review::TouchesInstance(asset,scratch,instance,{.4F,.2F,0},.01F),"Blade exterior must be empty");
    // 所有三角形的内部点与其微小球接触；覆盖尖端、薄刃和环侧面。
    for(auto& part:asset.Parts())for(auto t:part.triangles){auto point=Vector3Scale(Vector3Add(Vector3Add(t.a,t.b),t.c),1.F/3);
        require(review::TouchesInstance(asset,scratch,instance,point,.0001F),"Surface contact missing");}
    instance.position={3,-2,4};instance.rotation=QuaternionFromEuler(.3F,.7F,1.2F);instance.scale=2.5F;
    auto world=[&](Vector3 p){return Vector3Add(instance.position,Vector3RotateByQuaternion(Vector3Scale(p,instance.scale),instance.rotation));};
    require(!review::TouchesInstance(asset,scratch,instance,world({0,-.335F,0}),.02F*instance.scale),"Transformed hole");
    require(review::TouchesInstance(asset,scratch,instance,world({.115F,-.335F,0}),.005F*instance.scale),"Transformed ring");
    instance.appearance.glowWidth=.025F;instance.appearance.glowStrength=2;
    require(!review::TouchesInstance(asset,scratch,instance,world({0,-.335F,0}),.02F*instance.scale),"Glow changed collision");
    require(scratch.crossings.capacity()==capacity,"Collision allocated during query");
    rejects([&]{review::CollisionScratch empty;asset.DistanceSquared({0,0,0},empty);});
    rejects([&]{review::ClassicPalette(16);});
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(128,128,"Classic renderer tests");
    {
        review::ClassicRenderer renderer(root/"assets/projectiles/kunai_classic/kunai_classic.gltf",review::ProjectileShaderDirectory(root),2);
        Camera3D camera{{0,0,3},{0,0,0},{0,1,0},45,CAMERA_PERSPECTIVE};
        std::vector<review::ProjectileInstance> items(2);items[1].position={100,0,0};
        renderer.Prepare(items,camera,1);require(renderer.Stats().visible==1,"Frustum culling");
        BeginDrawing();BeginMode3D(camera);review::GLState before;renderer.Draw(true);review::GLState after;
        require(renderer.Stats().drawCalls==2&&renderer.Stats().triangles==292,"Glow batch budget");
        require(before.program==after.program&&before.vao==after.vao&&before.arrayBuffer==after.arrayBuffer&&before.depthWrite==after.depthWrite&&before.depth==after.depth&&before.cull==after.cull&&before.blend==after.blend&&before.srcRGB==after.srcRGB&&before.dstRGB==after.dstRGB&&before.srcAlpha==after.srcAlpha&&before.dstAlpha==after.dstAlpha&&before.eqRGB==after.eqRGB&&before.eqAlpha==after.eqAlpha&&before.depthFunc==after.depthFunc&&before.cullFace==after.cullFace&&before.frontFace==after.frontFace,"GL state leaked");
        renderer.Draw(false);require(renderer.Stats().drawCalls==1,"Glow off budget");EndMode3D();EndDrawing();
        renderer.Prepare(items,camera,1,false);require(renderer.Stats().visible==2,"Disabled culling");
        items.push_back({});rejects([&]{renderer.Prepare(items,camera,1);});items.pop_back();
        items[0].scale=0;rejects([&]{renderer.Prepare(items,camera,1);});items[0].scale=1;
        items[0].rotation={0,0,0,2};rejects([&]{renderer.Prepare(items,camera,1);});items[0].rotation={0,0,0,1};
        items[0].appearance.glowWidth=.03F;rejects([&]{renderer.Prepare(items,camera,1);});
        require(glGetError()==GL_NO_ERROR,"OpenGL error");
    }CloseWindow();std::cout<<"Classic collision and renderer checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';if(IsWindowReady())CloseWindow();return 1;}}
