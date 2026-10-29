// 本文件实现敌弹共享批次、自机弹和参数光束绘制；透明模型由公告板统一队列调度。
#include "combat_renderer.hpp"
#include "render/projectiles/effects_asset.hpp"
#include "render/projectiles/marisa_asset.hpp"
#include "rlgl.h"
#include "render/projectiles/model_draw.hpp"
#include "render/projectiles/option_renderer.hpp"
#include "render/projectiles/bomb_renderer.hpp"
#include <cmath>
namespace koumakyou {
using namespace projectile_detail;
CombatRenderer::CombatRenderer(){opaque_.reserve(BulletSystem::kCapacity);transparent_.reserve(BulletSystem::kCapacity);grouped_.reserve(BulletSystem::kCapacity);spawning_.reserve(BulletSystem::kCapacity);depths_.reserve(BulletSystem::kCapacity);}
void CombatRenderer::Prepare(const ProjectileAssets*assets,const BulletSystem&bullets,std::span<const VisualResource>visuals,const Camera3D&camera,float aspect,float simulationSeconds){
 assets_=assets;camera_=camera;aspect_=aspect;opaque_.clear();transparent_.clear();depths_.clear();time_=std::max(0.F,simulationSeconds);if(!assets_)return;
 Vector3 forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
 bullets.Pool().ForEachActive([&](const Bullet&b){if(b.visual>=visuals.size())return;auto binding=ProjectileFrameBuilder::Resolve(b,visuals[b.visual]);if(binding.type<0||binding.beam)return;
  auto pose=ProjectileFrameBuilder::Pose(b,binding,*assets_);projectiles::CollectionInstance item;item.type=binding.type;item.transform.position=pose.position;item.transform.rotation=pose.rotation;item.transform.scale=pose.scale;item.transform.appearance=projectiles::AppearanceFor(binding.type,binding.color);item.transform.appearance.bodyColor.a=b.color.a;
  bool spawning=b.spawnVisualSeconds>0&&b.age<b.spawnVisualSeconds;
  if(spawning){projectiles::SpawnParams p;p.age=b.age;p.duration=b.spawnVisualSeconds;auto sample=projectiles::SampleEffect(p);item.transform.scale*=sample.scale;item.transform.appearance.bodyColor.a=static_cast<unsigned char>(std::min(254.F,sample.alpha*b.color.a));}
  float depth=Vector3DotProduct(Vector3Subtract(b.position,camera.position),forward);transparent_.push_back(item);depths_.push_back(depth);if(!spawning&&b.color.a==255)opaque_.push_back(item);
 });
}
bool CombatRenderer::Handles(const Bullet&b,const VisualResource&v)const{return assets_&&!ProjectileFrameBuilder::Resolve(b,v).asset.empty();}
void CombatRenderer::DrawOpaque(){if(!assets_||opaque_.empty())return;auto&r=assets_->Collection();r.Prepare(opaque_,camera_,aspect_);r.Draw(time_,false,0,0,0);}
void CombatRenderer::DrawGroup(int group){std::size_t index=group;DrawGroups(std::span(&index,1));}
void CombatRenderer::DrawGroups(std::span<const std::size_t> groups){
 if(!assets_||groups.empty())return;grouped_.clear();spawning_.clear();
 for(auto index:groups){const auto& item=transparent_.at(index);grouped_.push_back(item);if(item.transform.appearance.bodyColor.a<255)spawning_.push_back(item);}
 auto&r=assets_->Collection();
 if(!spawning_.empty()){r.Prepare(spawning_,camera_,aspect_);r.Draw(time_,false,0,0,0);}
 r.Prepare(grouped_,camera_,aspect_);r.Draw(time_,true,0,1,2);
}
bool CombatRenderer::DrawBullet(const Bullet&b,const VisualResource&v){
 if(!assets_)return false;auto binding=ProjectileFrameBuilder::Resolve(b,v);if(binding.asset.empty())return false;if(binding.type>=0)return true;
 auto pose=ProjectileFrameBuilder::Pose(b,binding,*assets_);auto&r=assets_->Get(binding.id);float alpha=b.color.a/255.F;
 if(v.animation.atlas==battle::Atlas::Marisa){int kind=binding.asset=="marisa_primary"?0:binding.asset=="marisa_missile"?1:3;auto shader=assets_->Marisa();projectiles::MarisaInstance instance;instance.kind=kind;instance.tier=binding.tier;instance.age=b.age;auto state=projectiles::SampleMarisa(instance);
  Int(shader,"kind",kind);Int(shader,"clay",0);Real(shader,"phase",b.age);Real(shader,"growth",state.growth);Real(shader,"trailLengthScale",1);Real(shader,"trailWidthScale",1);Real(shader,"strength",.8F);Real(shader,"opacity",alpha*state.nativeOpacity);Vec(shader,"eye",camera_.position);Vec(shader,"tintMultiplier",{b.color.r/255.F,b.color.g/255.F,b.color.b/255.F});
  BeginBlendMode(BLEND_ALPHA);rlDrawRenderBatchActive();rlDisableDepthMask();Int(shader,"effect",0);DrawLayer(r.layers[0],shader,pose);EndBlendMode();
  BeginBlendMode(BLEND_ADDITIVE);Int(shader,"effect",1);DrawLayer(r.layers[1],shader,pose);EndBlendMode();rlDrawRenderBatchActive();rlDisableDepthMask();return true;
 }
 BeginBlendMode(BLEND_ALPHA);rlDrawRenderBatchActive();rlDisableDepthMask();DrawLayer(r.layers[0],{},pose,Fade(Color{b.color.r,b.color.g,b.color.b,255},alpha));EndBlendMode();
 BeginBlendMode(BLEND_ADDITIVE);if(binding.asset=="reimu_needle")DrawLayer(r.layers[1],assets_->Needle(),pose,Fade(VIOLET,alpha*.75F));else{pose.scale*=1.055F;DrawLayer(r.layers[0],{},pose,Fade(binding.asset=="reimu_gold"?GOLD:SKYBLUE,alpha*.2F));}EndBlendMode();return true;
}
bool CombatRenderer::DrawBeam(const Bullet&b,const VisualResource&v,float from,float to){
 if(!assets_)return false;auto binding=ProjectileFrameBuilder::Resolve(b,v);if(binding.asset.empty()||!binding.beam)return false;
 if(b.owner==BulletOwner::Enemy){projectiles::LaserInstance item;item.origin=b.position;item.direction=b.Direction();item.length=b.beamLength;item.radius=b.radius;item.visualScaleOverride=b.BeamVisualScale();item.dangerOverride=b.Dangerous()?1:0;item.color=std::clamp(binding.color,0,15);item.variant=binding.id==ProjectileAssetId::LaserRound?1:0;item.useColorOverride=v.animation.atlas!=battle::Atlas::BeamBullets;item.tintOverride=b.color;item.warning=0;item.growEnd=0;item.dangerEnd=10;item.end=11;item.age=1;item.fadeSeconds=0;if(item.radius>0&&item.length>0)assets_->Lasers().Draw(std::span(&item,1),camera_,aspect_,0,true,from,to);return true;}
 auto&r=assets_->Get(binding.id);auto pose=ProjectileFrameBuilder::Pose(b,binding,*assets_);pose.scale=1;pose.position=b.position;Shader s=assets_->Marisa();projectiles::MarisaInstance native;native.kind=2;native.tier=binding.tier;native.age=b.age;native.fadeAt=b.laserFadeAt>0?b.laserFadeAt:std::max(0.F,b.maxAge-.5F);auto state=projectiles::SampleMarisa(native);Int(s,"kind",2);Int(s,"clipEnabled",1);Real(s,"clipFrom",from);Real(s,"clipTo",to);Int(s,"clay",0);Real(s,"beamLength",b.beamLength);Real(s,"radius",b.radius*b.BeamVisualScale());Real(s,"phase",b.age);Real(s,"growth",1);Real(s,"trailLengthScale",1);Real(s,"trailWidthScale",1);Real(s,"opacity",b.color.a/255.F*state.nativeOpacity);Real(s,"strength",.8F);Vec(s,"eye",camera_.position);Vec(s,"tintMultiplier",{b.color.r/255.F,b.color.g/255.F,b.color.b/255.F});BeginBlendMode(BLEND_ADDITIVE);rlDrawRenderBatchActive();rlDisableDepthMask();Int(s,"effect",0);DrawLayer(r.layers[0],s,pose);Int(s,"effect",1);Real(s,"radius",b.radius*b.BeamVisualScale()*state.haloScale);DrawLayer(r.layers[1],s,pose);EndBlendMode();return true;
}
bool CombatRenderer::DrawOption(const PickupSprite&p,const VisualResource&v){return OptionRenderer::Draw(assets_,camera_,p,v);}
bool CombatRenderer::DrawBomb(const BombVolumeSprite&item,const VisualResource&v,float from,float to){return BombRenderer::Draw(assets_,camera_,item,v,from,to);}
}
