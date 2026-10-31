// 本文件将弹幕状态映射为模型标识及渲染/碰撞姿态；显示尺寸与危险体积保持独立。
#pragma once
#include "gameplay/bullets/types.hpp"
#include "resources/battle_catalog.hpp"
#include "resources/loaded_assets.hpp"
#include "resources/projectile_assets.hpp"
#include "core/projectile_pose.hpp"
namespace koumakyou {
/// @brief 无所有权的模型绑定；type=-1 表示自机弹/光束，color/tier 属于对应素材的变体作用域。
/// nominalLength 是模型正向的目标世界显示长度，零时按危险半径缩放；不包含显现动画放大。
struct ProjectileBinding {std::string_view asset;int type=-1,color=0,tier=0;bool beam=false;float nominalLength=0;ProjectileAssetId id=ProjectileAssetId::Invalid;};
class ProjectileFrameBuilder {
public:
 /// @brief 无资源分配；空 asset 表示沿用原公告板，未知手工弹不猜测模型。
 static ProjectileBinding Resolve(const Bullet&b,const VisualResource&v){
  if(!std::isfinite(b.radius)||b.radius<=0)return {};
  if(b.owner==BulletOwner::Enemy){if(b.IsBeam()){bool round=v.animation.atlas==battle::Atlas::BeamBullets&&v.animation.script==1;return {round?"laser_round":"laser_default",-1,v.animation.atlas==battle::Atlas::BeamBullets?v.animation.spriteOffset:b.nativeColor,0,true,0,round?ProjectileAssetId::LaserRound:ProjectileAssetId::LaserDefault};}int type=b.nativeType;if(type<0&&v.animation.atlas==battle::Atlas::Bullets&&v.animation.script<=9)type=v.animation.script;if(type>=0&&type<10){auto name=projectiles::ProjectileKinds()[type].directory;const int color=b.nativeType>=0?b.nativeColor:v.animation.spriteOffset;return {name,type,color,0,false,battle::EnemyBulletLogicalSize(type,color).y,DescribeProjectileModel(name).id};}return {};}
  int script=v.animation.script;
  const auto player=[&](std::string_view name,int tier=0,bool beam=false){return ProjectileBinding{name,-1,0,tier,beam,(v.worldUnitsPerPixel>0?v.worldHeight*b.visualScale:b.radius*v.bulletDiameterScale),DescribeProjectileModel(name).id};};
  if(v.animation.atlas==battle::Atlas::Reimu){if(script==64)return player("reimu_gold");if(script==65)return player("reimu_cyan");if(script==66)return player("reimu_needle");}
  if(v.animation.atlas==battle::Atlas::Marisa){if(script==64)return player("marisa_primary");if(script>=65&&script<=68)return player("marisa_missile",script-65);if(script>=69&&script<=71)return player("marisa_laser",script-69,true);if(script>=97&&script<=100)return player("marisa_explosion",script-97);}return {};
 }
 /// @brief 敌弹和自机弹均按原生显示高度缩放；光效和 spawn 不改变此渲染姿态。
 static ProjectilePose Pose(const Bullet&b,const ProjectileBinding&binding,const ProjectileAssets&assets){float tierScale=1;if(binding.asset=="marisa_missile"){constexpr float tiers[]{.65F,.75F,.9F,1};if(binding.tier<0||binding.tier>=4)throw std::invalid_argument("Invalid projectile tier");tierScale=tiers[binding.tier];}const auto&r=assets.Get(binding.id);auto d=DescribeProjectileModel(binding.id).direction;Quaternion canonical{0,0,0,1};if(d.forward.y!=1||d.up.z!=1)canonical=ProjectilePoseBuilder::BuildPose(d,{0,1,0},{},1).rotation;float scale=binding.nominalLength>0?binding.nominalLength/r.forwardExtent:b.radius/r.referenceRadius;if(!std::isfinite(scale)||scale<=0)throw std::invalid_argument("Invalid projectile scale");return {b.position,QuaternionNormalize(QuaternionMultiply(b.Orientation(),canonical)),scale*tierScale};}
 /// @brief 返回危险实体姿态；敌弹继续按判定半径缩放，视觉尺寸修正不得扩大或缩小既有碰撞体。
 static ProjectilePose CollisionPose(const Bullet&b,ProjectileBinding binding,const ProjectileAssets&assets){
  if(b.owner==BulletOwner::Enemy&&!binding.beam)binding.nominalLength=0;
  return Pose(b,binding,assets);
 }
};
}
