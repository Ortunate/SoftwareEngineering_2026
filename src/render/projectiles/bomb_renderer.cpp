// 本文件将现有世界演出状态转换为已验收模型显示；显示层不产生攻击体积。
#include "bomb_renderer.hpp"
#include "render/projectiles/model_draw.hpp"
#include "rlgl.h"
namespace koumakyou {
using namespace projectile_detail;
bool BombRenderer::Draw(const ProjectileAssets* assets_,const Camera3D& camera_,const BombVolumeSprite&item,const VisualResource&v,float from,float to){
 if(!assets_||!item.sprite)return false;const auto&p=*item.sprite;if(p.shape==PlayerBombSprite::Shape::Slab)return false;if(p.trail)return true;
 int kind=p.shape==PlayerBombSprite::Shape::Beam?3:p.shape==PlayerBombSprite::Shape::Star?2:0;if(kind!=2&&p.layer!=0)return true;auto name=kind==3?"bomb_spark":kind==2?"bomb_star":"bomb_seal";const auto&r=assets_->Get(name);ProjectilePose pose;pose.position=p.volume.origin;pose.scale=kind==2?p.height/(2*r.referenceRadius):p.volume.radius/r.referenceRadius;Shader s=assets_->Bomb();
 Int(assets_,s,"kind",kind);Int(assets_,s,"palette",kind==2?p.layer%3:-1);Int(assets_,s,"clipEnabled",kind==3);Real(assets_,s,"clipFrom",from);Real(assets_,s,"clipTo",to);Int(assets_,s,"clay",0);Real(assets_,s,"seconds",p.ageSeconds);float opacity=p.frame.color.a/255.F*(kind==2?.5F:1.F);
 // 攻击几何保持不变，近眼曝光沿用原演出规则，避免球体/星群覆盖整个第一人称画面。
 if(kind!=3){float distance=Vector3Distance(camera_.position,p.volume.origin);float near=std::clamp((distance/std::max(.01F,p.volume.radius)-.35F)/1.3F,0.F,1.F);opacity*=std::max(kind==2?.005F:.075F,near*near*near*near);}
 // 原四层光炮合并为一份几何，补偿总亮度，避免重复描画四组螺旋。
 if(kind==3)opacity=std::min(1.F,opacity*4);
 Real(assets_,s,"opacity",opacity);Real(assets_,s,"strength",.8F);Vec(assets_,s,"eye",camera_.position);Vec(assets_,s,"tintMultiplier",{1,1,1});
 if(kind==3){Vector3 delta=Vector3Subtract(p.end,p.position);float length=Vector3Length(delta);if(length<=0)return true;Vector3 direction=Vector3Scale(delta,1/length);pose=ProjectilePoseBuilder::BuildPose(DescribeProjectileModel(name).direction,direction,p.position,1);Real(assets_,s,"beamLength",length);Real(assets_,s,"nearRadius",.3F);Real(assets_,s,"farRadius",1.05F);}
 else if(kind==2)pose.rotation=QuaternionFromAxisAngle({0,0,1},p.frame.rotation.z);
 BeginBlendMode(BLEND_ADDITIVE);rlDrawRenderBatchActive();rlDisableDepthMask();for(int layer=0;layer<3;layer++){Int(assets_,s,"layer",layer);DrawLayer(r.layers[layer],s,pose);}EndBlendMode();return true;
}
}
