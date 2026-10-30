// 本文件查询模型危险实体到目标球心的距离；纯 CPU，不创建或访问 GPU 对象。
#pragma once
#include "resources/projectile_binding.hpp"
namespace koumakyou {
class ProjectileCollision {
public:
 ProjectileCollision(){scratch_.Prepare(2048);}
 /// @brief 返回接触范围内的实体外距离平方，内部为零；粗筛已排除时返回无穷大。
 /// 缺少模型时返回 -1，调用方沿用旧球/胶囊；queryRadius 为目标的最大接触半径。
 /// 正常姿态与渲染一致，显现缩放与光效不参与危险体积。每个查询线程独占一个实例。
 float DistanceSquared(const ProjectileAssets*assets,const Bullet&b,const VisualResource&visual,Vector3 point,float queryRadius){
  if(!assets)return -1;
  auto binding=ProjectileFrameBuilder::Resolve(b,visual);
  if(binding.asset.empty())return -1;
  if(binding.beam){float distance=std::max(0.F,std::sqrt(b.DistanceSquaredTo(point))-b.radius);return distance*distance;}
  const auto&r=assets->Get(binding.id);if(!r.collision)return -1;
  auto pose=ProjectileFrameBuilder::Pose(b,binding,*assets);Vector3 local=Vector3Scale(Vector3RotateByQuaternion(Vector3Subtract(point,pose.position),QuaternionInvert(pose.rotation)),1/pose.scale);
  // 包围球先排除远目标；只在接近实体时查询已加载的 BVH。远距离返回保守下界。
  float lower=std::max(0.F,Vector3Length(local)-r.collision->BoundingRadius())*pose.scale;
  if(lower>queryRadius)return INFINITY;
  return r.collision->DistanceSquared(local,scratch_)*pose.scale*pose.scale;
 }
private:projectiles::CollisionScratch scratch_;
};
}
