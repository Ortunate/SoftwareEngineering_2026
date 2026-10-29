// 本文件声明独立的副机模型绘制，不推进动画或结算伤害。
#pragma once
#include "resources/projectile_assets.hpp"
#include "render/sprite_renderer.hpp"
namespace koumakyou {
class OptionRenderer {public:
 /// @brief 在统一透明队列内调用；资源和动画仅在本次绘制借用。返回是否已处理模型。
 static bool Draw(const ProjectileAssets* assets_,const Camera3D& camera_,const PickupSprite&p,const VisualResource&v);
};
}
