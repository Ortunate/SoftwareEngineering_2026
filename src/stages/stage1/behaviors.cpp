// 本文件实现第 1 面普通演员的命名行为与资源绑定；Boss 专属行为由角色模块拥有。
#include "stages/stage1/behaviors.hpp"
#include "stages/stage1/ids.hpp"
#include "gameplay/spatial.hpp"
#include <cmath>
#include <numbers>
namespace koumakyou::stage1 {
void ConfigureBehavior(behavior::Actor& a,bool spatial) {
    a.skills.bulletBase=kNativeBulletBase;a.skills.laserBase=kNativeLaserBase;a.skills.fallback=kBlueBullet;
    a.skills.spatial=spatial;
    if(!spatial)a.liftedMotion.reset();
}
} // namespace koumakyou::stage1
