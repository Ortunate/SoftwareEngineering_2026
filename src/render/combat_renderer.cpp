// 本文件实现敌弹共享批次、自机弹和参数光束绘制；透明模型由公告板统一队列调度。
#include "combat_renderer.hpp"
#include "render/projectiles/effects_asset.hpp"
#include "render/projectiles/marisa_asset.hpp"
#include "render/projectiles/marisa_beam_geometry.hpp"
#include "render/projectile_visibility.hpp"
#include "external/glad.h"
#include "rlgl.h"
#include "render/projectiles/model_draw.hpp"
#include "render/projectiles/option_renderer.hpp"
#include "render/projectiles/bomb_renderer.hpp"
#include <cmath>
#include <functional>
namespace koumakyou {
using namespace projectile_detail;
CombatRenderer::CombatRenderer(){opaque_.reserve(BulletSystem::kCapacity);transparent_.reserve(BulletSystem::kCapacity);grouped_.reserve(BulletSystem::kCapacity);spawning_.reserve(BulletSystem::kCapacity);depths_.reserve(BulletSystem::kCapacity);preparedGroups_.reserve(BulletSystem::kCapacity);lasers_.reserve(BulletSystem::kCapacity);laserIndices_.reserve(BulletSystem::kCapacity);}
void CombatRenderer::Prepare(const ProjectileAssets*assets,const BulletSystem&bullets,std::span<const VisualResource>visuals,const Camera3D&camera,Vector3 playerPosition,float aspect,float simulationSeconds){
 assets_=assets;camera_=camera;aspect_=aspect;opaque_.clear();transparent_.clear();depths_.clear();time_=std::max(0.F,simulationSeconds);lasers_.clear();laserIndices_.clear();if(!assets_)return;
 Vector3 forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
 const VisualResource fallbackVisual{};
 bullets.Pool().ForEachActive([&](const Bullet&b){
  if(b.visual>=visuals.size()&&!b.IsBeam())return;
  // 手工敌激光可以没有图集槽，沿用 Resolve 的默认模型和实例色；普通弹仍按既有规则跳过未知外观。
  const auto& visual=b.visual<visuals.size()?visuals[b.visual]:fallbackVisual;
  auto binding=ProjectileFrameBuilder::Resolve(b,visual);
  if(binding.beam&&b.owner==BulletOwner::Enemy){
   projectiles::LaserInstance item;item.origin=b.position;item.direction=b.Direction();item.length=b.beamLength;item.radius=b.radius;item.visualScaleOverride=b.BeamVisualScale();item.dangerOverride=b.Dangerous()?1:0;item.color=std::clamp(binding.color,0,15);item.variant=binding.id==ProjectileAssetId::LaserRound?1:0;
   // 近自机淡化需要通过实例 Alpha 进入激光批次；原生色号先解析为同一调色，不能因强制 override 丢失色相。
   item.useColorOverride=true;item.tintOverride=visual.animation.atlas!=battle::Atlas::BeamBullets?b.color:projectiles::ClassicPalette(item.color).bodyColor;
   item.tintOverride.a=ScaleProjectileAlpha(b.color.a,NearPlayerBulletAlpha(b,playerPosition,b.radius*b.BeamVisualScale()));item.warning=0;item.growEnd=0;item.dangerEnd=10;item.end=11;item.age=1;item.fadeSeconds=0;
   if(item.radius>0&&item.length>0){laserIndices_.push_back({&b,lasers_.size()});lasers_.push_back(item);}return;
  }
  if(binding.type<0||binding.beam)return;
  auto pose=ProjectileFrameBuilder::Pose(b,binding,*assets_);projectiles::CollectionInstance item;item.type=binding.type;item.transform.position=pose.position;item.transform.rotation=pose.rotation;item.transform.scale=pose.scale;item.transform.appearance=projectiles::AppearanceFor(binding.type,binding.color);
  const float visualRadius=assets_->Get(binding.id).visualRadius*pose.scale;
  item.transform.appearance.bodyColor.a=ScaleProjectileAlpha(b.color.a,NearPlayerBulletAlpha(b,playerPosition,visualRadius));
  bool spawning=b.spawnVisualSeconds>0&&b.age<b.spawnVisualSeconds;
  if(spawning){projectiles::SpawnParams p;p.age=b.age;p.duration=b.spawnVisualSeconds;auto sample=projectiles::SampleEffect(p);item.transform.scale*=sample.scale;item.transform.appearance.bodyColor.a=static_cast<unsigned char>(std::min(254.F,sample.alpha*item.transform.appearance.bodyColor.a));}
  float depth=Vector3DotProduct(Vector3Subtract(b.position,camera.position),forward);transparent_.push_back(item);depths_.push_back(depth);if(!spawning&&item.transform.appearance.bodyColor.a==255)opaque_.push_back(item);
 });
 // 指针只在本次 Draw 借用；排序只影响查找，不改变透明队列、弹池或发弹顺序。
 std::sort(laserIndices_.begin(),laserIndices_.end(),[](const auto& a,const auto& b){return std::less<const Bullet*>{}(a.first,b.first);});
 assets_->Lasers().Prepare(lasers_,camera_,aspect_);
}
bool CombatRenderer::Handles(const Bullet&b,const VisualResource&v)const{return assets_&&!ProjectileFrameBuilder::Resolve(b,v).asset.empty();}
void CombatRenderer::DrawOpaque(){if(!assets_||opaque_.empty())return;auto&r=assets_->Collection();r.Prepare(opaque_,camera_,aspect_);r.Draw(time_,false,0,0,0);}
void CombatRenderer::DrawGroup(int group){std::size_t index=group;DrawGroups(std::span(&index,1));}
void CombatRenderer::DrawGroups(std::span<const std::size_t> groups){
 if(!assets_||groups.empty())return;grouped_.clear();spawning_.clear();
 // Alpha 主体的混合不可交换；逐实例入口也必须遵守传入的远到近顺序。
 if(groups.size()>1&&std::any_of(groups.begin(),groups.end(),[&](auto index){return GroupTranslucent(index);})){for(auto index:groups)DrawGroup(static_cast<int>(index));return;}
 for(auto index:groups){const auto& item=transparent_.at(index);grouped_.push_back(item);if(item.transform.appearance.bodyColor.a<255)spawning_.push_back(item);}
 auto&r=assets_->Collection();
 if(!spawning_.empty()){r.Prepare(spawning_,camera_,aspect_);r.Draw(time_,false,0,0,0);}
 r.Prepare(grouped_,camera_,aspect_);r.Draw(time_,true,0,1,2);
}
void CombatRenderer::BeginGroups(){preparedGroups_.clear();if(assets_)assets_->Collection().BeginFrame(camera_,aspect_);}
std::size_t CombatRenderer::PrepareGroup(std::span<const std::size_t> groups){
 grouped_.clear();spawning_.clear();for(auto index:groups){const auto& item=transparent_.at(index);grouped_.push_back(item);if(item.transform.appearance.bodyColor.a<255)spawning_.push_back(item);}
 auto& renderer=assets_->Collection();auto body=renderer.Append(spawning_);auto decoration=renderer.Append(grouped_);
 preparedGroups_.push_back({body,decoration});return preparedGroups_.size()-1;
}
void CombatRenderer::UploadGroups(){if(assets_)assets_->Collection().Upload();}
void CombatRenderer::DrawPreparedGroup(std::size_t batch){
 auto& renderer=assets_->Collection();const auto& group=preparedGroups_.at(batch);
 renderer.Select(group.body);renderer.Draw(time_,false,0,0,0);
 renderer.Select(group.decoration);renderer.Draw(time_,true,0,1,2);
}
bool CombatRenderer::DrawBullet(const Bullet&b,const VisualResource&v){
 if(!assets_)return false;auto binding=ProjectileFrameBuilder::Resolve(b,v);if(binding.asset.empty())return false;if(binding.type>=0)return true;
 auto pose=ProjectileFrameBuilder::Pose(b,binding,*assets_);auto&r=assets_->Get(binding.id);float alpha=b.color.a/255.F;
 if(v.animation.atlas==battle::Atlas::Marisa){int kind=binding.asset=="marisa_primary"?0:binding.asset=="marisa_missile"?1:3;auto shader=assets_->Marisa();projectiles::MarisaInstance instance;instance.kind=kind;instance.tier=binding.tier;instance.age=b.age;auto state=projectiles::SampleMarisa(instance);
  Int(assets_,shader,"kind",kind);Int(assets_,shader,"clay",0);Real(assets_,shader,"phase",b.age);Real(assets_,shader,"growth",state.growth);Real(assets_,shader,"trailLengthScale",1);Real(assets_,shader,"trailWidthScale",1);Real(assets_,shader,"strength",.8F);Real(assets_,shader,"opacity",alpha*state.nativeOpacity);Vec(assets_,shader,"eye",camera_.position);Vec(assets_,shader,"tintMultiplier",{b.color.r/255.F,b.color.g/255.F,b.color.b/255.F});
  BeginBlendMode(BLEND_ALPHA);rlDrawRenderBatchActive();rlDisableDepthMask();Int(assets_,shader,"effect",0);DrawLayer(r.layers[0],shader,pose);EndBlendMode();
  BeginBlendMode(BLEND_ADDITIVE);Int(assets_,shader,"effect",1);DrawLayer(r.layers[1],shader,pose);EndBlendMode();rlDrawRenderBatchActive();rlDisableDepthMask();return true;
 }
 // 灵梦实体复用抛光材质，底色由 DrawMesh 的 colDiffuse 提供；尾迹继续使用原光效通道。
 auto surface=assets_->Marisa();Int(assets_,surface,"kind",4);Int(assets_,surface,"effect",0);Int(assets_,surface,"clay",0);Real(assets_,surface,"opacity",alpha);Vec(assets_,surface,"eye",camera_.position);Vec(assets_,surface,"tintMultiplier",{b.color.r/255.F,b.color.g/255.F,b.color.b/255.F});
 BeginBlendMode(BLEND_ALPHA);rlDrawRenderBatchActive();rlDisableDepthMask();DrawLayer(r.layers[0],surface,pose);EndBlendMode();
 BeginBlendMode(BLEND_ADDITIVE);if(binding.asset=="reimu_needle")DrawLayer(r.layers[1],assets_->Needle(),pose,Fade(VIOLET,alpha*.75F));else{pose.scale*=1.055F;DrawLayer(r.layers[0],{},pose,Fade(binding.asset=="reimu_gold"?GOLD:SKYBLUE,alpha*.2F));}EndBlendMode();return true;
}
bool CombatRenderer::DrawBeam(const Bullet&b,const VisualResource&v,float from,float to){
 if(!assets_)return false;auto binding=ProjectileFrameBuilder::Resolve(b,v);if(binding.asset.empty()||!binding.beam)return false;
 if(b.owner==BulletOwner::Enemy){
  const auto found=std::lower_bound(laserIndices_.begin(),laserIndices_.end(),&b,[](const auto& entry,const Bullet* key){return std::less<const Bullet*>{}(entry.first,key);});
  if(found!=laserIndices_.end()&&found->first==&b)assets_->Lasers().DrawPrepared(found->second,from,to);
  return true;
 }
 const auto geometry=projectiles::SampleMarisaBeamGeometry(b,binding.tier);if(geometry.length<=0)return true;
 auto&r=assets_->Get(binding.id);auto pose=ProjectileFrameBuilder::Pose(b,binding,*assets_);pose.scale=1;pose.position=geometry.origin;
 Shader s=assets_->Marisa();projectiles::MarisaInstance native;native.kind=2;native.tier=binding.tier;native.age=b.age;native.fadeAt=b.laserFadeAt>0?b.laserFadeAt:std::max(0.F,b.maxAge-.5F);auto state=projectiles::SampleMarisa(native);
 Int(assets_,s,"kind",2);Int(assets_,s,"clipEnabled",1);Int(assets_,s,"clipGeometry",1);Real(assets_,s,"clipFrom",from);Real(assets_,s,"clipTo",to);Int(assets_,s,"clay",0);
 Real(assets_,s,"beamLength",geometry.length);Real(assets_,s,"radius",geometry.radius);Real(assets_,s,"phase",b.age);Real(assets_,s,"growth",1);Real(assets_,s,"trailLengthScale",1);Real(assets_,s,"trailWidthScale",1);Real(assets_,s,"opacity",b.color.a/255.F*state.nativeOpacity);Real(assets_,s,"strength",.8F);Vec(assets_,s,"eye",camera_.position);Vec(assets_,s,"tintMultiplier",{b.color.r/255.F,b.color.g/255.F,b.color.b/255.F});BeginBlendMode(BLEND_ADDITIVE);rlDrawRenderBatchActive();rlDisableDepthMask();
 // 裁剪距离仅在两层激光的立即绘制期间启用；其他 raylib shader 没有对应输出，必须在返回前关闭。
 glEnable(GL_CLIP_DISTANCE0);glEnable(GL_CLIP_DISTANCE1);
 Int(assets_,s,"effect",0);DrawLayer(r.layers[0],s,pose);Int(assets_,s,"effect",1);Real(assets_,s,"radius",geometry.haloRadius/1.5F);DrawLayer(r.layers[1],s,pose);
 glDisable(GL_CLIP_DISTANCE0);glDisable(GL_CLIP_DISTANCE1);EndBlendMode();return true;
}
bool CombatRenderer::DrawItem(const PickupSprite&p,const VisualResource&v){
 if(!assets_||v.animation.atlas!=battle::Atlas::Bullets||v.animation.script<21||v.animation.script>27)return false;
 if(p.animation&&!p.animation->visible)return true;
 ProjectilePose pose;pose.position=p.position;pose.scale=v.worldHeight*p.heightScale;pose.rotation=QuaternionFromAxisAngle({0,1,0},p.itemRotation);
 // 正背面与侧壁沿世界固定长轴转动，不作公告板朝向；只读共享材质，不改变拾取体积。
 Color tint=p.animation?p.animation->color:WHITE;tint.a=static_cast<unsigned char>(int(tint.a)*p.alpha/255);
 BeginBlendMode(BLEND_ALPHA);rlDrawRenderBatchActive();rlDisableDepthMask();rlEnableBackfaceCulling();DrawLayer(assets_->Item(v.animation.script-21),{},pose,tint);rlDisableBackfaceCulling();EndBlendMode();return true;
}
bool CombatRenderer::DrawOption(const PickupSprite&p,const VisualResource&v){return OptionRenderer::Draw(assets_,camera_,p,v);}
bool CombatRenderer::DrawBomb(const BombVolumeSprite&item,const VisualResource&v,float from,float to){return BombRenderer::Draw(assets_,camera_,item,v,from,to);}
}
