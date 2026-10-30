// 本文件将现有世界演出状态转换为已验收模型显示；显示层不产生攻击体积。
#include "option_renderer.hpp"
#include "render/projectiles/model_draw.hpp"
#include "rlgl.h"
namespace koumakyou {
using namespace projectile_detail;
bool OptionRenderer::Draw(const ProjectileAssets* assets_,const Camera3D& camera_,const PickupSprite&p,const VisualResource&v){
 if(!assets_||v.animation.script<128||v.animation.script>129||(v.animation.atlas!=battle::Atlas::Reimu&&v.animation.atlas!=battle::Atlas::Marisa))return false;
 auto name=v.animation.atlas==battle::Atlas::Reimu?"reimu_option":"marisa_option";ProjectilePose pose;pose.position=p.position;pose.scale=v.worldHeight*p.heightScale/assets_->Get(name).forwardExtent;
 // 原 ANM 游标仍独立推进；魔理沙右侧反向自转，灵梦左侧在旋转前沿模型本地 X 镜像。
 // 只复制 Model 描述并改变本次变换，不修改共享网格、纹理或原生动画状态。
 Vector3 direction=Vector3Normalize(Vector3Subtract(camera_.position,p.position));pose=ProjectilePoseBuilder::BuildPose(DescribeProjectileModel(name).direction,direction,p.position,pose.scale);float angle=p.animation?p.animation->rotation.z:0;const bool reimu=v.animation.atlas==battle::Atlas::Reimu;
 if(!reimu&&v.animation.script==129)angle=-angle;
 Model model=assets_->Get(name).layers[0];
 if(reimu&&v.animation.script==128)model.transform=MatrixMultiply(model.transform,MatrixScale(-1,1,1));
 pose.rotation=QuaternionMultiply(QuaternionFromAxisAngle(direction,-angle),pose.rotation);BeginBlendMode(BLEND_ALPHA);rlDrawRenderBatchActive();rlDisableDepthMask();DrawLayer(model,{},pose,Fade(p.animation?p.animation->color:WHITE,p.alpha/255.F*(p.animation?p.animation->color.a/255.F:1.F)));EndBlendMode();BeginBlendMode(BLEND_ADDITIVE);DrawBillboard(camera_,assets_->OptionHalo(),p.position,v.worldHeight*p.heightScale*1.37F,Fade(v.animation.atlas==battle::Atlas::Reimu?Color{255,220,220,180}:Color{90,235,240,210},p.alpha/255.F*(p.animation?p.animation->color.a/255.F:1.F)));EndBlendMode();return true;
}
}
