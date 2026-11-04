// 本文件用隐藏图形窗口检查正式模型资源、共享批次、孔洞碰撞和卸载；不进行性能测试。
#include "resources/projectile_assets.hpp"
#include "resources/asset_manager.hpp"
#include "gameplay/collision/projectile_collision.hpp"
#include "render/combat_renderer.hpp"
#include "render/projectiles/model_draw.hpp"
#include "render/projectiles/marisa_asset.hpp"
#include "render/projectiles/marisa_beam_geometry.hpp"
#include "render/projectiles/bomb_renderer.hpp"
#include "gameplay/actors/player.hpp"
#include "stages/stage1/content.hpp"
#include "external/glad.h"
#include "gameplay/items/drop_system.hpp"
#include "rlgl.h"
#include <source_location>
#include <iostream>
#include <fstream>
#include <cstring>
#include "resources/projectile_paths.hpp"
using namespace koumakyou;
void Require(bool v,std::source_location at=std::source_location::current()){if(!v)throw std::runtime_error("Projectile runtime integration failed at line "+std::to_string(at.line()));}
/// @brief 将统一队列与独立逐弹绘制比较；混合弹种必须保持远到近的主体/装饰顺序，并且绘制不改战斗 Alpha。
void CheckNearFadeOrdering(ProjectileAssets& assets){
 auto bullets=std::make_unique<BulletSystem>();std::array<VisualResource,2> visuals{};
 auto* far=bullets->Spawn(BulletOwner::Enemy,{0,0,-.2F},{0,0,-1},.2F,WHITE,0);far->nativeType=3;far->nativeColor=2;
 auto* near=bullets->Spawn(BulletOwner::Enemy,{0,0,.2F},{0,0,-1},.2F,WHITE,1);near->nativeType=0;near->nativeColor=6;
 Camera3D camera{{0,0,3},{0,0,0},{0,1,0},42,CAMERA_PERSPECTIVE};const Vector3 player{0,0,.3F};
 auto target=LoadRenderTexture(192,192);SpriteRenderer sprites;ActorEffectSystem effects;
 for(bool spawning:{false,true}){
  far->spawnVisualSeconds=spawning?1.F:0.F;far->age=.5F;
  const auto render=[&](bool unified){
   CombatRenderer models;models.Prepare(&assets,*bullets,visuals,camera,player,1);
   Require(models.GroupTranslucent(0)&&models.GroupTranslucent(1));
   BeginTextureMode(target);ClearBackground({15,21,33,255});BeginMode3D(camera);
   if(unified)sprites.Draw(camera,player,*bullets,{},nullptr,{},visuals,{},effects,{},nullptr,&models);
   else{models.DrawOpaque();models.DrawGroup(0);models.DrawGroup(1);}
   EndMode3D();EndTextureMode();return LoadImageFromTexture(target.texture);
  };
  auto expected=render(false),actual=render(true);auto a=LoadImageColors(expected),b=LoadImageColors(actual);int visible=0;
  for(int i=0;i<192*192;++i){visible+=a[i].r!=15||a[i].g!=21||a[i].b!=33;
   Require(std::abs(int(a[i].r)-b[i].r)<=1&&std::abs(int(a[i].g)-b[i].g)<=1&&std::abs(int(a[i].b)-b[i].b)<=1);}
  Require(visible>100&&far->color.a==255&&near->color.a==255);
  UnloadImageColors(a);UnloadImageColors(b);UnloadImage(expected);UnloadImage(actual);
 }
 UnloadRenderTexture(target);
}
/// @brief 正式加载路径必须保留针晶的平面法线，防止通用平滑把六棱实体变成圆润外观。
void CheckNeedleCrystal(const ProjectileAssets& assets){
 const auto& model=assets.Get(ProjectileAssetId::NeedleClassic).layers[0];Require(model.meshCount==1);
 const auto& mesh=model.meshes[0];Require(mesh.triangleCount==24);
 for(int t=0;t<mesh.triangleCount;++t){
  std::array<int,3> ids{};std::array<Vector3,3> p{};
  for(int j=0;j<3;++j){ids[j]=mesh.indices?mesh.indices[3*t+j]:3*t+j;const int i=ids[j];p[j]={mesh.vertices[3*i],mesh.vertices[3*i+1],mesh.vertices[3*i+2]};}
  const auto face=Vector3Normalize(Vector3CrossProduct(Vector3Subtract(p[1],p[0]),Vector3Subtract(p[2],p[0])));
  for(int i:ids)Require(Vector3DotProduct(face,{mesh.normals[3*i],mesh.normals[3*i+1],mesh.normals[3*i+2]})>.9999F);
 }
}
/// @brief 验证压薄刀柄仍有实体和贯通孔，且正式加载保留竖直主刀面的法线。
void CheckKnifeShape(const ProjectileAssets& assets){
 const auto& resource=assets.Get(ProjectileAssetId::KnifeClassic);Require(resource.collision!=nullptr);
 const auto& mesh=resource.layers[0].meshes[0];int front=0,back=0;
 for(int i=0;i<mesh.vertexCount;++i){const float z=mesh.vertices[3*i+2];
  if(std::abs(std::abs(z)-.028F)<1e-6F&&std::abs(mesh.normals[3*i+2])>.9999F){if(z>0)++front;else ++back;}
 }
 Require(front>=7&&back>=7);
 projectiles::CollisionScratch scratch;scratch.Prepare(resource.collision->MaxPartTriangles());
 Require(resource.collision->DistanceSquared({.065F,-.305F,0},scratch)<1e-8F);
 Require(resource.collision->DistanceSquared({0,-.305F,0},scratch)>.0001F);
 Require(resource.collision->DistanceSquared({.065F,-.305F,.06F},scratch)>.0001F);
}
/// @brief 正式封印四个素材层都必须绘制且保持原作色相，不能丢层或沿用相同混色。
void CheckSealColors(ProjectileAssets& assets,const std::filesystem::path& output){
 constexpr int size=192;auto target=LoadRenderTexture(size,size);Image sheet=GenImageColor(size*4,size,BLACK);
 Camera3D camera{{0,0,3},{0,0,0},{0,1,0},42,CAMERA_PERSPECTIVE};
 std::array<std::array<double,3>,4> totals{};
 for(int layer=0;layer<4;++layer){
  PlayerBombSprite p;p.layer=layer;p.volume.radius=.6F;p.frame.color=WHITE;p.ageSeconds=1;
  BeginTextureMode(target);ClearBackground(BLACK);BeginMode3D(camera);
  Require(BombRenderer::Draw(&assets,camera,{&p,kInvalidVisualId},{},0,1));
  EndMode3D();rlDrawRenderBatchActive();rlEnableDepthMask();EndTextureMode();
  auto picture=LoadImageFromTexture(target.texture);ImageFlipVertical(&picture);auto pixels=LoadImageColors(picture);
  for(int i=0;i<size*size;++i){totals[layer][0]+=pixels[i].r;totals[layer][1]+=pixels[i].g;totals[layer][2]+=pixels[i].b;}
  Require(totals[layer][0]+totals[layer][1]+totals[layer][2]>10000);
  ImageDraw(&sheet,picture,{0,0,size,size},{float(layer*size),0,size,size},WHITE);UnloadImageColors(pixels);UnloadImage(picture);
 }
 // 独立按蓝、青、绿、红检查主通道，避免只验证四张画面均非空却遗漏颜色绑定。
 const auto& blue=totals[0];const auto& cyan=totals[1];const auto& green=totals[2];const auto& red=totals[3];
 Require(blue[2]>blue[0]*1.05&&blue[2]>blue[1]*1.05);
 Require(cyan[1]>cyan[0]*1.05&&cyan[2]>cyan[0]*1.05);
 Require(green[1]>green[0]*1.05&&green[1]>green[2]*1.05);
 Require(red[0]>red[1]*1.05&&red[0]>red[2]*1.05);
 if(!output.empty()){std::filesystem::create_directories(output);Require(ExportImage(sheet,(output/"seal-four-colors.png").string().c_str()));}
 UnloadImage(sheet);UnloadRenderTexture(target);
}
/// @brief 比较立即提交和帧内批次的像素，覆盖类型重排、半透明主体、空批次和片段索引。
void CheckFrameBatches(ProjectileAssets& assets){
 for(auto shader:{assets.Marisa(),assets.Bomb(),assets.Needle()})for(const char* name:{"kind","eye","opacity","not_a_uniform"})Require(assets.UniformLocation(shader,name)==GetShaderLocation(shader,name));
 Camera3D camera{{0,0,6},{0,0,0},{0,1,0},42,CAMERA_PERSPECTIVE};
 std::array<projectiles::CollectionInstance,6> items{};
 for(size_t i=0;i<items.size();++i){auto& item=items[i];item.type=std::array{9,7,4,1,3,0}[i];item.transform.position={float(i%3-1.F)*.5F,float(i/3)*.6F-.3F,float(i)*.1F};item.transform.scale=.8F;item.transform.appearance=projectiles::AppearanceFor(item.type,int(i));if(i%2)item.transform.appearance.bodyColor.a=128;}
 auto target=LoadRenderTexture(256,256);auto& renderer=assets.Collection();
 const auto render=[&](bool prepared){
  BeginTextureMode(target);ClearBackground({15,21,33,255});BeginMode3D(camera);
  if(prepared){
   renderer.BeginFrame(camera,1);auto empty=renderer.Append({});auto first=renderer.Append(std::span(items).first(3));auto last=renderer.Append(std::span(items).last(3));
   bool rejected=false;try{renderer.Select(first);}catch(const std::logic_error&){rejected=true;}Require(rejected);
   renderer.Upload();renderer.Select(empty);renderer.Draw(1);
   for(auto id:{first,last}){renderer.Select(id);renderer.Draw(1);}
  }else for(int i=0;i<2;++i){renderer.Prepare(std::span(items).subspan(i*3,3),camera,1);renderer.Draw(1);}
  EndMode3D();EndTextureMode();return LoadImageFromTexture(target.texture);
 };
 auto immediate=render(false),prepared=render(true);auto a=LoadImageColors(immediate),b=LoadImageColors(prepared);Require(std::memcmp(a,b,256*256*sizeof(Color))==0);UnloadImageColors(a);UnloadImageColors(b);UnloadImage(immediate);UnloadImage(prepared);
 // 两个 variant 以相反输入次序出现，片段句柄必须仍指向原输入激光，不能误用分组后的索引。
 std::array<projectiles::LaserInstance,2> beams{};beams[0].variant=1;beams[0].origin={-.3F,-1,0};beams[1].origin={.3F,-1,0};
 auto& lasers=assets.Lasers();
 const auto renderLasers=[&](bool cached){
  BeginTextureMode(target);ClearBackground(BLACK);BeginMode3D(camera);
  if(cached){lasers.Prepare(beams,camera,1);for(size_t i=0;i<2;++i)lasers.DrawPrepared(i,.2F,.8F);}
  else for(size_t i=0;i<2;++i)lasers.Draw(std::span(beams).subspan(i,1),camera,1,0,true,.2F,.8F);
  EndMode3D();EndTextureMode();return LoadImageFromTexture(target.texture);
 };
 immediate=renderLasers(false);prepared=renderLasers(true);a=LoadImageColors(immediate);b=LoadImageColors(prepared);Require(std::memcmp(a,b,256*256*sizeof(Color))==0);UnloadImageColors(a);UnloadImageColors(b);UnloadImage(immediate);UnloadImage(prepared);
 // 手工敌激光没有有效图集编号时也必须沿用默认模型，不能因帧内预收集漏掉而消失。
 BulletSystem manual;auto* beam=manual.SpawnBeam({0,-1,0},{0,1,0},2,.1F,0,10,WHITE);beam->visual=kInvalidVisualId;beam->age=1;
 CombatRenderer combat;combat.Prepare(&assets,manual,{},camera,camera.position,1);BeginTextureMode(target);BeginMode3D(camera);Require(combat.DrawBeam(*beam,{},0,1));EndMode3D();EndTextureMode();Require(assets.Lasers().Stats().drawCalls==3);
 // 正式批次必须容纳整个弹池，而不是到 8193 才抛出运行时异常。
 std::vector<projectiles::CollectionInstance> full(BulletSystem::kCapacity);for(auto& item:full)item.transform.scale=.01F;
 renderer.Prepare(full,camera,1);Require(renderer.Stats().submitted==BulletSystem::kCapacity);UnloadRenderTexture(target);
}
/// @brief 校验真实玩家各火力、俯仰和近远瞄准下，整个激光外壳都在镜头前方。
/// 目标从远处移近时复用持续光束，必须跟随当前副机缩放；远端中心和判定数据保持不变。
void CheckFirstPersonMarisaGeometry(){
 stage1::Stage1Content content;auto bullets=std::make_unique<BulletSystem>();
 for(int power:{8,48,96,128})for(bool focus:{false,true})for(auto view:std::array<Vector2,3>{{{0,0},{1.2F,1.2F},{-2,-1.2F}}}){
  Player player;player.ConfigureRun(3,3,false,ShotType::IllusionLaser);player.AdjustPower(power-player.Power());player.SetView(view.x,view.y);
  const auto visuals=content.PlayerVisuals(player.Shot());
  for(float targetDepth:{20.F,.05F,1.F}){
   Camera3D camera{};player.SyncCamera(camera);const auto forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
   const PlayerAimTarget target{Vector3Add(player.Position(),Vector3Scale(forward,targetDepth)),.01F};
   for(int i=0;i<8;++i){player.Update(1.F/60,{.focus=focus,.fire=true},*bullets,visuals,true,std::span(&target,1));bullets->Update(1.F/60);}
   int count=0;
   bullets->Pool().ForEachActive([&](const Bullet& beam){if(beam.playerShotKind!=PlayerShotKind::Laser)return;
    ++count;Require(std::abs(beam.visualScale-player.OptionScale())<1e-6F);
    const int tier=power<48?0:power<96?1:2;const auto geometry=projectiles::SampleMarisaBeamGeometry(beam,tier);
    const float near=Vector3DotProduct(Vector3Subtract(geometry.origin,camera.position),forward)-geometry.haloRadius;
    Require(near>.05F&&geometry.radius<.2F*player.OptionScale()&&geometry.length>0);
    Require(Vector3Distance(geometry.end,beam.BeamEnd())<1e-5F);
    Require(Vector3Distance(Vector3Add(geometry.origin,Vector3Scale(beam.Direction(),geometry.length)),beam.BeamEnd())<1e-5F);
    Require(std::abs(beam.beamLength-480*(20.F/448))<1e-5F&&beam.damagePerSecond>0);
   });Require(count==2);
  }
  bullets->Reset();
 }
}
/// @brief 将正式魔理沙分段绘制与仅片元丢弃的参考路径比较，涵盖端帽、薄片、轴向及斜视。
/// 参考侧复用同一模型与光效参数，但关闭硬件裁剪；不能用缩短胶囊替代原束的切片。
void CheckMarisaBeamClipping(ProjectileAssets& assets,const std::filesystem::path& output){
 using namespace projectile_detail;
 constexpr int size=256;
 auto target=LoadRenderTexture(size,size);
 BulletSystem bullets;VisualResource visual{};visual.animation={battle::Atlas::Marisa,71};visual.bulletDiameterScale=2;
 auto* beam=bullets.Spawn(BulletOwner::Player,{0,-1,0},{},.15F,{255,255,255,51},0);
 beam->playerShotKind=PlayerShotKind::Laser;beam->SetBeamDirection({0,1,0});beam->beamLength=2;beam->age=1;beam->maxAge=4;beam->laserFadeAt=3;
 const auto binding=ProjectileFrameBuilder::Resolve(*beam,visual);
 auto pose=ProjectileFrameBuilder::Pose(*beam,binding,assets);pose.scale=1;
 projectiles::MarisaInstance native;native.kind=2;native.tier=binding.tier;native.age=1;native.fadeAt=3;
 const auto state=projectiles::SampleMarisa(native);const auto shader=assets.Marisa();const auto& model=assets.Get(binding.id);
 for(const float radius:{.15F,.45F})for(const auto position:std::array<Vector3,4>{{{0,0,4},{2,1,3},{.2F,3,.1F},{.2F,-1.2F,.1F}}})for(const int projection:{CAMERA_PERSPECTIVE,CAMERA_ORTHOGRAPHIC}){
  beam->radius=radius;const auto geometry=projectiles::SampleMarisaBeamGeometry(*beam,binding.tier);pose.position=geometry.origin;
  Camera3D camera{position,{0,0,0},{0,1,0},projection==CAMERA_PERSPECTIVE?42.F:3.F,projection};
  CombatRenderer combat;combat.Prepare(&assets,bullets,std::span(&visual,1),camera,camera.position,1);
  for(const auto interval:std::array<Vector2,5>{{{0,1},{0,.4F},{.4F,1},{.2F,.8F},{.49F,.51F}}}){
   const auto render=[&](bool clipped){
    BeginTextureMode(target);ClearBackground({15,21,33,255});BeginMode3D(camera);rlDisableBackfaceCulling();
    if(clipped){Require(combat.DrawBeam(*beam,visual,interval.x,interval.y));Require(!glIsEnabled(GL_CLIP_DISTANCE0)&&!glIsEnabled(GL_CLIP_DISTANCE1));}
    else{
     Int(&assets,shader,"kind",2);Int(&assets,shader,"clipEnabled",1);Int(&assets,shader,"clipGeometry",0);Real(&assets,shader,"clipFrom",interval.x);Real(&assets,shader,"clipTo",interval.y);
     Int(&assets,shader,"clay",0);Real(&assets,shader,"beamLength",geometry.length);Real(&assets,shader,"opacity",beam->color.a/255.F*state.nativeOpacity);Real(&assets,shader,"strength",.8F);
     Vec(&assets,shader,"eye",camera.position);Vec(&assets,shader,"tintMultiplier",{1,1,1});
     BeginBlendMode(BLEND_ADDITIVE);rlDrawRenderBatchActive();rlDisableDepthMask();
     Int(&assets,shader,"effect",0);Real(&assets,shader,"radius",geometry.radius);DrawLayer(model.layers[0],shader,pose);
     Int(&assets,shader,"effect",1);Real(&assets,shader,"radius",geometry.haloRadius/1.5F);DrawLayer(model.layers[1],shader,pose);EndBlendMode();
    }
    EndMode3D();rlEnableDepthMask();rlEnableBackfaceCulling();EndTextureMode();return LoadImageFromTexture(target.texture);
   };
   auto reference=render(false),actual=render(true);auto first=LoadImageColors(reference),second=LoadImageColors(actual);
   int covered=0;double error=0;
   for(int i=0;i<size*size;++i){if(first[i].r!=15||first[i].g!=21||first[i].b!=33||second[i].r!=15||second[i].g!=21||second[i].b!=33)++covered;error+=std::abs(int(first[i].r)-int(second[i].r))+std::abs(int(first[i].g)-int(second[i].g))+std::abs(int(first[i].b)-int(second[i].b));}
   // 裁剪交点只能产生亚像素舍入差；空画面不允许作为相等通过。
   if(!(covered>0&&error/(3*covered)<.5)){std::cerr<<"clip view="<<position.x<<","<<position.y<<","<<position.z<<" projection="<<projection<<" interval="<<interval.x<<","<<interval.y<<" covered="<<covered<<" error="<<error<<"\n";}
   Require(covered>0&&error/(3*covered)<.5);
   UnloadImageColors(first);UnloadImageColors(second);UnloadImage(reference);UnloadImage(actual);
  }
 }
 // RenderTexture 不带 MSAA，另在真实 4×MSAA 窗口比较整束与 20 段相加。
 // 没有其他透明对象时两种画法必须连续，防止硬件裁剪后再 discard 造成切口缺样。
 Camera3D nearCamera{{.2F,-1.2F,.1F},{0,0,0},{0,1,0},65,CAMERA_PERSPECTIVE};
 CombatRenderer windowCombat;windowCombat.Prepare(&assets,bullets,std::span(&visual,1),nearCamera,nearCamera.position,float(GetRenderWidth())/GetRenderHeight());
 const auto renderWindow=[&](int segments){
  BeginDrawing();ClearBackground({15,21,33,255});BeginMode3D(nearCamera);rlDisableBackfaceCulling();
  for(int i=0;i<segments;++i)Require(windowCombat.DrawBeam(*beam,visual,float(i)/segments,float(i+1)/segments));
  EndMode3D();rlEnableDepthMask();rlEnableBackfaceCulling();rlDrawRenderBatchActive();
  // 在交换前读取本次 back buffer，避免隐藏窗口首次交换后读到上一帧或未初始化缓冲。
  auto result=LoadImageFromScreen();EndDrawing();return result;
 };
 auto whole=renderWindow(1),split=renderWindow(20);auto a=LoadImageColors(whole),b=LoadImageColors(split);
 int maximum=0,visible=0,large=0;double total=0;
 for(int i=0;i<whole.width*whole.height;++i){
  visible+=int(a[i].r)+a[i].g+a[i].b>15+21+33;
  const int delta=std::max({std::abs(int(a[i].r)-int(b[i].r)),std::abs(int(a[i].g)-int(b[i].g)),std::abs(int(a[i].b)-int(b[i].b))});
  maximum=std::max(maximum,delta);large+=delta>2;total+=delta;
 }
 // 裁剪生成顶点恰好落在采样点上时可有孤立舍入差，但不能容许沿切口延伸的缺样。
 const bool continuous=visible>1000&&large<=8&&maximum<=16&&total/(whole.width*whole.height)<.001;
 if(!continuous){
  std::cerr<<"MSAA beam split maximum delta="<<maximum<<" above2="<<large<<" mean="<<total/(whole.width*whole.height)<<'\n';
  if(!output.empty()){std::filesystem::create_directories(output);ExportImage(whole,(output/"msaa-whole.png").string().c_str());ExportImage(split,(output/"msaa-split.png").string().c_str());}
 }
 Require(continuous);UnloadImageColors(a);UnloadImageColors(b);UnloadImage(whole);UnloadImage(split);
 UnloadRenderTexture(target);
}
/// @brief 相机与弹体一起转动时相对视角不变，十类弹体的颜色不得依赖固定世界光向。
/// 另检查针形弹的对称正背面，防止旧顶点亮面在转向后重新形成白面/暗面分区。
void CheckProjectileLighting(ProjectileAssets& assets,const std::filesystem::path& root,const std::filesystem::path& output){
 constexpr int size=192;
 auto target=LoadRenderTexture(size,size);
 Image sheet=GenImageColor(size*10,size*3,{15,21,33,255});
 auto& renderer=assets.Collection();
 projectiles::ClassicRenderer classic(projectiles::ProjectileAssetDirectory(root)/"kunai_classic/kunai_classic.gltf",projectiles::ProjectileShaderDirectory(root),1);
 const auto render=[&](int type,Quaternion rotation,bool rotateCamera,bool useClassic=false){
  Camera3D camera{{0,0,2},{0,0,0},{0,1,0},42,CAMERA_PERSPECTIVE};
  if(rotateCamera){camera.position=Vector3RotateByQuaternion(camera.position,rotation);camera.up=Vector3RotateByQuaternion(camera.up,rotation);}
  projectiles::CollectionInstance item;item.type=type;item.transform.rotation=rotation;item.transform.appearance=projectiles::AppearanceFor(type,2);
  if(useClassic)classic.Prepare(std::span(&item.transform,1),camera,1,false);
  else renderer.Prepare(std::span(&item,1),camera,1,false);
  BeginTextureMode(target);ClearBackground({15,21,33,255});BeginMode3D(camera);
  if(useClassic)classic.Draw();else renderer.Draw(0);
  EndMode3D();EndTextureMode();
  auto image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);return image;
 };
 const auto compare=[&](const Image& a,const Image& b){
  auto first=LoadImageColors(a),second=LoadImageColors(b);double error=0;int covered=0;
  for(int i=0;i<size*size;++i){
   if(first[i].r==15&&first[i].g==21&&first[i].b==33&&second[i].r==15&&second[i].g==21&&second[i].b==33)continue;
   error+=std::abs(int(first[i].r)-int(second[i].r))+std::abs(int(first[i].g)-int(second[i].g))+std::abs(int(first[i].b)-int(second[i].b));++covered;
  }
  UnloadImageColors(first);UnloadImageColors(second);
  // 仅容许栅格化边缘的舍入差，不能掩盖整面亮度变化；空画面也必须失败。
  Require(covered>100&&error/(3*covered)<.5);
 };
 for(int type=0;type<10;++type){
  auto front=render(type,QuaternionIdentity(),false);
  ImageDraw(&sheet,front,{0,0,size,size},{float(type*size),0,size,size},WHITE);
  for(int view=1;view<=2;++view){
   auto q=QuaternionFromAxisAngle({0,1,0},view*PI/2);
   auto orbit=render(type,q,true);compare(front,orbit);UnloadImage(orbit);
   auto turned=render(type,q,false);
   ImageDraw(&sheet,turned,{0,0,size,size},{float(type*size),float(view*size),size,size},WHITE);
   if(type==5&&view==2)compare(front,turned);
   UnloadImage(turned);
  }
  UnloadImage(front);
 }
 // 兼容苦无查看器使用独立 shader，同样必须跟随本次 Prepare 的相机。
 auto classicFront=render(4,QuaternionIdentity(),false,true);
 auto classicOrbit=render(4,QuaternionFromAxisAngle({0,1,0},PI/2),true,true);
 compare(classicFront,classicOrbit);UnloadImage(classicFront);UnloadImage(classicOrbit);
 if(!output.empty()){std::filesystem::create_directories(output);Require(ExportImage(sheet,(output/"projectile-lighting-views.png").string().c_str()));}
 UnloadImage(sheet);UnloadRenderTexture(target);
}
int main(int argc,char**argv){try{
 if(argc<2)throw std::runtime_error("Asset root required");CheckFirstPersonMarisaGeometry();SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_MSAA_4X_HINT);InitWindow(1280,800,"Projectile runtime integration");
 for(int cycle=0;cycle<2;cycle++){
  ProjectileAssets assets(argv[1]);CheckNearFadeOrdering(assets);CheckNeedleCrystal(assets);CheckKnifeShape(assets);CheckFrameBatches(assets);CheckMarisaBeamClipping(assets,argc>2?std::filesystem::path(argv[2]):std::filesystem::path{});if(cycle==0){CheckSealColors(assets,argc>2?std::filesystem::path(argv[2]):std::filesystem::path{});CheckProjectileLighting(assets,argv[1],argc>2?std::filesystem::path(argv[2]):std::filesystem::path{});}for(const auto&d:ProjectileModels)Require(assets.Get(d.name).layers[0].meshCount>0);
  // DropSystem 模拟年龄决定旋转，相同模拟时间不依赖帧率；零步长冻结，池复用恢复零角。
  DropSystem drops(7);drops.SpawnFixedPower({0,0,0},1);drops.Update(1,{0,-9,-8});
  drops.ForEachActive([](const Pickup&p){Require(std::abs(p.RotationRadians()-PI/2)<1e-5F);Require(p.NativeItemType()==0);});
  drops.Update(0,{0,-9,-8});drops.ForEachActive([](const Pickup&p){Require(std::abs(p.RotationRadians()-PI/2)<1e-5F);});
  drops.Clear();drops.SpawnFixedPower({0,0,0},8);drops.ForEachActive([](const Pickup&p){Require(p.RotationRadians()==0&&p.NativeItemType()==2);});
  Camera3D itemCamera{{0,0,8},{0,0,0},{0,1,0},4,CAMERA_ORTHOGRAPHIC};
  CombatRenderer itemRenderer;BulletSystem empty;itemRenderer.Prepare(&assets,empty,{},itemCamera,itemCamera.position,1,0);
  auto itemTarget=LoadRenderTexture(1024,640);BeginTextureMode(itemTarget);ClearBackground(BLACK);BeginMode3D(itemCamera);
  for(int item=0;item<7;item++){Require(assets.Item(item).meshCount>0);VisualResource visual{};visual.animation=battle::Item(item);visual.worldHeight=.8F;PickupSprite sprite;sprite.position={(item%4-1.5F)*1.3F,(.5F-item/4)*1.5F,0};sprite.itemRotation=.5F;Require(itemRenderer.DrawItem(sprite,visual));}
  EndMode3D();rlEnableDepthMask();rlEnableBackfaceCulling();EndTextureMode();auto itemImage=LoadImageFromTexture(itemTarget.texture);ImageFlipVertical(&itemImage);
  auto pixels=LoadImageColors(itemImage);int itemVisible=0;for(int i=0;i<itemImage.width*itemImage.height;i++)if(pixels[i].r+pixels[i].g+pixels[i].b>30)itemVisible++;Require(itemVisible>1000);UnloadImageColors(pixels);
  if(argc>2&&cycle==0){std::filesystem::create_directories(argv[2]);ExportImage(itemImage,(std::filesystem::path(argv[2])/"items-runtime.png").string().c_str());}UnloadImage(itemImage);UnloadRenderTexture(itemTarget);
  BulletSystem bullets;std::array<VisualResource,10> visuals{};ProjectileCollision collision;
  for(int type=0;type<10;type++){
   visuals[type].animation=battle::EnemyBullet(type,2);auto b=bullets.Spawn(BulletOwner::Enemy,{(type%5-2)*1.2F,(.5F-type/5)*1.4F,0},{0,1,0},battle::EnemyBulletHitRadius(type),WHITE,type);Require(b);b->nativeType=type;b->nativeColor=2;
   if(type==0)b->spawnVisualSeconds=.5F,b->age=.2F;
   auto binding=ProjectileFrameBuilder::Resolve(*b,visuals[type]);Require(!binding.asset.empty());auto pose=ProjectileFrameBuilder::Pose(*b,binding,assets);
   const auto& resource=assets.Get(binding.id);
   Require(std::abs(pose.scale*resource.forwardExtent-battle::EnemyBulletLogicalSize(type,2).y)<1e-6F);
   auto collisionPose=ProjectileFrameBuilder::CollisionPose(*b,binding,assets);
   Require(std::abs(collisionPose.scale*resource.referenceRadius-b->radius)<1e-6F);
   Require(Vector3Distance(Vector3RotateByQuaternion(DescribeProjectileModel(binding.asset).direction.forward,pose.rotation),b->Direction())<1e-4F);
   // 远目标粗筛必须拒绝，而不是误认为未知模型退回球形命中。
   Require(std::isinf(collision.DistanceSquared(&assets,*b,visuals[type],{100,100,100},.2F)));
  }
  // 大泡最后一色沿用 16 像素判定半径，但模型显示必须单独缩到原生 30 像素。
  Bullet smallBubble;smallBubble.active=true;smallBubble.owner=BulletOwner::Enemy;smallBubble.nativeType=9;smallBubble.nativeColor=4;smallBubble.radius=battle::EnemyBulletHitRadius(9);smallBubble.SetVelocity({0,1,0});
  auto smallBubbleBinding=ProjectileFrameBuilder::Resolve(smallBubble,visuals[9]);auto smallBubblePose=ProjectileFrameBuilder::Pose(smallBubble,smallBubbleBinding,assets);
  Require(std::abs(smallBubblePose.scale*assets.Get(smallBubbleBinding.id).forwardExtent-battle::EnemyBulletLogicalSize(9,4).y)<1e-6F);
  // 苦无尾环中心真实为空，刃部是危险实体。换姿态后仍查询同一孔洞。
  Bullet knife;knife.active=true;knife.owner=BulletOwner::Enemy;knife.nativeType=4;knife.nativeColor=2;knife.radius=.4F;knife.SetVelocity({1,0,0});auto binding=ProjectileFrameBuilder::Resolve(knife,visuals[4]);auto pose=ProjectileFrameBuilder::CollisionPose(knife,binding,assets);
  auto world=[&](Vector3 p){return Vector3Add(pose.position,Vector3RotateByQuaternion(Vector3Scale(p,pose.scale),pose.rotation));};
  Require(collision.DistanceSquared(&assets,knife,visuals[4],world({0,-.335F,0}),1)>.00001F);
  Require(collision.DistanceSquared(&assets,knife,visuals[4],world({0,.2F,0}),1)<.00001F);
  Camera3D camera{{0,0,6},{0,0,0},{0,1,0},42,CAMERA_PERSPECTIVE};CombatRenderer models;models.Prepare(&assets,bullets,visuals,camera,camera.position,1.6F);
  auto target=LoadRenderTexture(1280,800);BeginTextureMode(target);ClearBackground({15,21,33,255});BeginMode3D(camera);models.DrawOpaque();for(int group=models.TransparentGroups()-1;group>=0;group--)models.DrawGroup(group);EndMode3D();EndTextureMode();
  Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);int visible=0;for(int y=0;y<800;y+=2)for(int x=0;x<1280;x+=2){auto pixel=GetImageColor(image,x,y);if(pixel.r>35||pixel.g>40||pixel.b>55)visible++;}Require(visible>200);
  if(argc>2&&cycle==0){std::filesystem::create_directories(argv[2]);Require(ExportImage(image,(std::filesystem::path(argv[2])/"enemy-models-runtime.png").string().c_str()));}
  UnloadImage(image);UnloadRenderTexture(target);
 }
 if(argc>2){
  auto bad=std::filesystem::path(argv[2])/"bad-assets";std::filesystem::create_directories(bad/"shaders/projectiles");std::filesystem::create_directories(bad/"projectiles/bomb_seal");
  for(const auto&entry:std::filesystem::directory_iterator(projectiles::ProjectileShaderDirectory(argv[1])))std::filesystem::copy_file(entry.path(),bad/"shaders/projectiles"/entry.path().filename(),std::filesystem::copy_options::overwrite_existing);
  {std::ofstream file(bad/"projectiles/bomb_seal/bomb_seal.gltf");file<<"{invalid";}
  ProjectileAssets failed(bad,false);Require(!failed.Ready());Require(!failed.AdvanceLoad());bool rejected=false;try{failed.AdvanceLoad();}catch(const std::runtime_error&){rejected=true;}Require(rejected&&!failed.Ready());
 }
 {
  AssetManager manager(argv[1]);manager.BeginStageLoad({});for(int i=0;i<5;i++)Require(!manager.AdvanceStageLoad());Require(!manager.Projectiles());manager.UnloadStage();Require(!manager.Projectiles()&&!manager.HasStage());
  manager.BeginStageLoad({});int steps=0;while(!manager.AdvanceStageLoad())Require(++steps<40);Require(manager.Projectiles()&&manager.HasStage());manager.UnloadStage();Require(!manager.Projectiles());
 }
 CloseWindow();std::cout<<"24 shared assets, native mapping, direction, hole collision, spawn batch and unload PASS\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
