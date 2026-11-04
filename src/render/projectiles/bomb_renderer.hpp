// 本文件声明独立的Bomb模型绘制，不推进动画或结算伤害。
#pragma once
#include "resources/projectile_assets.hpp"
#include "render/sprite_renderer.hpp"
namespace koumakyou {
class BombRenderer {public:
 /// @brief 在统一透明队列内调用；资源和动画仅在本次绘制借用。返回是否已处理模型。
 /// 封印按 sprite.layer 的 0～3 选择原作蓝/青/绿/红，各层曝光为单份的 1/4；不改变攻击几何。
 static bool Draw(const ProjectileAssets* assets_,const Camera3D& camera_,const BombVolumeSprite&item,const VisualResource&v,float from,float to);
};
}
