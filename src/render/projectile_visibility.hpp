// 本文件定义弹幕近自机淡化的纯计算规则，不修改战斗状态或素材。
#pragma once

#include "gameplay/bullets/types.hpp"

#include <algorithm>
#include <cmath>

namespace koumakyou {

/// @brief 返回敌弹因接近自机而乘到原始 Alpha 上的显示系数。
/// @param bullet 只读弹幕状态；自机弹始终返回 1。
/// @param playerPosition 当前自机的世界坐标，回放观察相机不得代替该位置。
/// @param visualRadius 弹体显示外形的保守世界半径；用于按表面而非中心距离淡化大弹。
/// @return [0.35,1] 内的有限系数。近距离且正在逼近的危险弹下限为 0.65，不会完全隐形。
inline float NearPlayerBulletAlpha(const Bullet& bullet, Vector3 playerPosition,
                                   float visualRadius) {
    if (bullet.owner != BulletOwner::Enemy) return 1.0F;

    constexpr float kNearSurfaceDistance = 0.15F;
    constexpr float kFullOpacitySurfaceDistance = 1.25F;
    constexpr float kMinimumAlpha = 0.35F;
    constexpr float kThreatMinimumAlpha = 0.65F;

    const float distanceSquared = bullet.DistanceSquaredTo(playerPosition);
    if (!std::isfinite(distanceSquared) || !std::isfinite(visualRadius)) return 1.0F;
    const float surfaceDistance = std::sqrt(std::max(0.0F, distanceSquared)) -
                                  std::max(0.0F, visualRadius);
    float progress = std::clamp(
        (surfaceDistance - kNearSurfaceDistance) /
            (kFullOpacitySurfaceDistance - kNearSurfaceDistance),
        0.0F, 1.0F);
    progress = progress * progress * (3.0F - 2.0F * progress);

    const Vector3 velocity = bullet.Velocity();
    const Vector3 relative{bullet.position.x - playerPosition.x,
                           bullet.position.y - playerPosition.y,
                           bullet.position.z - playerPosition.z};
    const bool approaching = relative.x * velocity.x + relative.y * velocity.y +
                                 relative.z * velocity.z <
                             0.0F;
    // 光束的危险体是整段胶囊，不能用起点速度判断是否逼近。
    const bool threatening = bullet.Dangerous() && (bullet.IsBeam() || approaching);
    const float minimum = threatening ? kThreatMinimumAlpha : kMinimumAlpha;
    return minimum + (1.0F - minimum) * progress;
}

/// @brief 将淡化系数乘入 8 位 Alpha，四舍五入并饱和到有效范围。
inline unsigned char ScaleProjectileAlpha(unsigned char alpha, float factor) {
    return static_cast<unsigned char>(std::lround(
        std::clamp(static_cast<float>(alpha) * factor, 0.0F, 255.0F)));
}

}  // namespace koumakyou
