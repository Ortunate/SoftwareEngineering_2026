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
#include "resources/projectile_paths.hpp"
using namespace koumakyou;
void Require(bool v,std::source_location at=std::source_location::current()){if(!v)throw std::runtime_error("Projectile runtime integration failed at line "+std::to_string(at.line()));}
int main(int argc,char**argv){try{
 if(argc<2)throw std::runtime_error("Asset root required");SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(1280,800,"Projectile runtime integration");
 for(int cycle=0;cycle<2;cycle++){
  ProjectileAssets assets(argv[1]);for(const auto&d:ProjectileModels)Require(assets.Get(d.name).layers[0].meshCount>0);
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
