// 本文件用隐藏图形窗口检查正式模型资源、共享批次、孔洞碰撞和卸载；不进行性能测试。
#include "resources/projectile_assets.hpp"
#include "resources/asset_manager.hpp"
#include "gameplay/collision/projectile_collision.hpp"
#include "render/combat_renderer.hpp"
#include "gameplay/items/drop_system.hpp"
#include "rlgl.h"
#include <source_location>
#include <iostream>
#include <fstream>
#include <cstring>
#include "resources/projectile_paths.hpp"
using namespace koumakyou;
void Require(bool v,std::source_location at=std::source_location::current()){if(!v)throw std::runtime_error("Projectile runtime integration failed at line "+std::to_string(at.line()));}
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
 CombatRenderer combat;combat.Prepare(&assets,manual,{},camera,1);BeginTextureMode(target);BeginMode3D(camera);Require(combat.DrawBeam(*beam,{},0,1));EndMode3D();EndTextureMode();Require(assets.Lasers().Stats().drawCalls==3);
 // 正式批次必须容纳整个弹池，而不是到 8193 才抛出运行时异常。
 std::vector<projectiles::CollectionInstance> full(BulletSystem::kCapacity);for(auto& item:full)item.transform.scale=.01F;
 renderer.Prepare(full,camera,1);Require(renderer.Stats().submitted==BulletSystem::kCapacity);UnloadRenderTexture(target);
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
 if(argc<2)throw std::runtime_error("Asset root required");SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(1280,800,"Projectile runtime integration");
 for(int cycle=0;cycle<2;cycle++){
  ProjectileAssets assets(argv[1]);CheckFrameBatches(assets);if(cycle==0)CheckProjectileLighting(assets,argv[1],argc>2?std::filesystem::path(argv[2]):std::filesystem::path{});for(const auto&d:ProjectileModels)Require(assets.Get(d.name).layers[0].meshCount>0);
  // DropSystem 模拟年龄决定旋转，相同模拟时间不依赖帧率；零步长冻结，池复用恢复零角。
  DropSystem drops(7);drops.SpawnFixedPower({0,0,0},1);drops.Update(1,{0,-9,-8});
  drops.ForEachActive([](const Pickup&p){Require(std::abs(p.RotationRadians()-PI/2)<1e-5F);Require(p.NativeItemType()==0);});
  drops.Update(0,{0,-9,-8});drops.ForEachActive([](const Pickup&p){Require(std::abs(p.RotationRadians()-PI/2)<1e-5F);});
  drops.Clear();drops.SpawnFixedPower({0,0,0},8);drops.ForEachActive([](const Pickup&p){Require(p.RotationRadians()==0&&p.NativeItemType()==2);});
  Camera3D itemCamera{{0,0,8},{0,0,0},{0,1,0},4,CAMERA_ORTHOGRAPHIC};
  CombatRenderer itemRenderer;BulletSystem empty;itemRenderer.Prepare(&assets,empty,{},itemCamera,1,0);
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
  Camera3D camera{{0,0,6},{0,0,0},{0,1,0},42,CAMERA_PERSPECTIVE};CombatRenderer models;models.Prepare(&assets,bullets,visuals,camera,1.6F);
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
