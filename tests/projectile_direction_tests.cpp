// 本文件验证速度方向不变量、静止束轴及模型姿态对齐，不执行性能测试。
#include "gameplay/bullets/system.hpp"
#include "render/projectile_pose.hpp"
#include <limits>
#include <iostream>
using namespace koumakyou;
void Check(bool v){if(!v)throw std::runtime_error("Projectile direction check failed");}
int main(){
 Bullet b;b.SetVelocity({3,4,0});Check(Vector3Distance(b.Direction(),{.6F,.8F,0})<1e-5F);
 b.SetVelocity({});Check(Vector3Distance(b.Direction(),{.6F,.8F,0})<1e-5F);
 b.SetInitialDirection({0,0,-2});Check(b.Direction().z==-1);b.SetVelocity({-3,0,0});Check(b.Direction().x==-1);
 bool rejected=false;try{b.SetInitialDirection({0,1,0});}catch(const std::invalid_argument&){rejected=true;}Check(rejected);
 rejected=false;try{b.SetVelocity({std::numeric_limits<float>::infinity(),0,0});}catch(const std::invalid_argument&){rejected=true;}Check(rejected&&b.Direction().x==-1);
 b.SetVelocity({});b.SetBeamDirection({0,2,0});b.position={1,2,3};b.beamLength=4;Check(Vector3Distance(b.BeamEnd(),{1,6,3})<1e-5F);Check(b.DistanceSquaredTo({1,5,3})==0);
 b=Bullet{};Check(b.Direction().z==1&&Vector3Length(b.Velocity())==0);
 FixedPool<Bullet,1> pool;auto slot=pool.Acquire();slot->SetVelocity({1,0,0});slot->active=false;slot=pool.Acquire();Check(slot->Direction().z==1&&Vector3Length(slot->Velocity())==0);
 BulletSystem system;
 auto moving=system.Spawn(BulletOwner::Enemy,{},Vector3{2,0,0},.1F,WHITE,kInvalidVisualId,{0,2,0});
 Check(moving);moving->angularVelocity=.8F;moving->homingTurnRate=.4F;
 Vector3 target{0,3,3};
 for(int i=0;i<10;i++){system.Update(.01F,std::span(&target,1));Check(Vector3Distance(moving->Direction(),Vector3Normalize(moving->Velocity()))<1e-5F);}
 moving->baseVelocity=moving->Velocity();moving->turnDelaySeconds=.11F;moving->turnSpeed=2;moving->turnAngle=PI;system.Update(.02F);Check(moving->turnApplied&&Vector3Distance(moving->Direction(),Vector3Normalize(moving->Velocity()))<1e-5F);
 auto missile=system.Spawn(BulletOwner::Player,{},Vector3{1,0,0},.1F,WHITE,kInvalidVisualId,{0,3,0});missile->playerShotKind=PlayerShotKind::Missile;system.Update(.02F);Check(Vector3Distance(missile->Direction(),Vector3Normalize(missile->Velocity()))<1e-5F);
 auto beam=system.SpawnBeam({},Vector3{1,2,3},2,.1F,0,1,WHITE);Check(beam);auto axis=beam->Direction();system.Update(.02F);Check(Vector3Distance(beam->Direction(),axis)<1e-5F);
 for(const auto& asset:ProjectileModels){
  Quaternion previous{};bool have=false;
  for(Vector3 d: {Vector3{1,0,0},Vector3{-1,0,0},Vector3{0,1,0},Vector3{0,-1,0},Vector3{0,0,1},Vector3{0,0,-1},Vector3Normalize(Vector3{1,2,3})}){
   auto pose=ProjectilePoseBuilder::BuildPose(asset.direction,d,{},1,have?&previous:nullptr,.2F);
   Check(Vector3Distance(Vector3RotateByQuaternion(asset.direction.forward,pose.rotation),d)<1e-4F);previous=pose.rotation;have=true;
  }
  for(int i=0;i<720;i++){Vector3 d=Vector3Normalize({std::cos(i*.01F),.1F,std::sin(i*.01F)});auto pose=ProjectilePoseBuilder::BuildPose(asset.direction,d,{},1,&previous);Check(Vector3Distance(Vector3RotateByQuaternion(asset.direction.forward,pose.rotation),d)<1e-4F);previous=pose.rotation;}
  Vector3 nearReverse=Vector3Normalize(Vector3Add(Vector3Negate(Vector3RotateByQuaternion(asset.direction.forward,previous)),{.0002F,.0001F,0}));
  auto turn=ProjectilePoseBuilder::BuildPose(asset.direction,nearReverse,{},1,&previous);Check(Vector3Distance(Vector3RotateByQuaternion(asset.direction.forward,turn.rotation),nearReverse)<1e-4F);
 }
 std::cout<<"Bullet direction, beam axis and all model poses PASS\n";
}
