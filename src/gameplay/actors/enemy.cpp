// 本文件负责敌人出生及视觉计时、边界预淡出和发弹点计算，并调用子类行为。

#include "gameplay/arena.hpp"
#include "gameplay/actors/enemy.hpp"

#include "gameplay/bullets/patterns.hpp"

#include <algorithm>
#include <cmath>

namespace koumakyou {

Enemy::Enemy(VisualId appearance, Vector3 start,
             std::span<const DropOutcome> drops)
    : visual(appearance), position(start), origin(start),
      dropDistribution_(drops) {}

Vector3 Enemy::MuzzlePosition(Vector3 playerPosition) const {
    return pattern::Add(position,
        pattern::Scale(BulletSystem::DirectionTo(position, playerPosition),
                       hitRadius * 0.55F));
}

void Enemy::Update(float deltaTime, Vector3 playerPosition,
                   BulletSystem& bullets) {
    age += deltaTime;
    presentation.Update(deltaTime);
    const Vector3 previous = position;
    Move(deltaTime);
    UpdateSkills(deltaTime, playerPosition, bullets);
    constexpr float margin = 2.0F;
    // 到达战场表面之前按当前外向速度预淡出；原有场外回收边界保持不变。
    // 首帧轨道偏移不是速度；后续预测只在本帧有效，转向后不能沿用过时淡出。
    // 同帧取最早越界的轴，斜向运动不能漏掉更近的表面。
    if (hasMovementSample_ && deltaTime > 0.0F) {
        const auto prepare = [this, deltaTime](float current, float last, float extent) {
            const float outward = (current - last) * (current >= 0.0F ? 1.0F : -1.0F);
            if (outward > 0.0F)
                presentation.PreviewBoundaryExit(std::max(0.0F, extent - std::abs(current)) *
                                                 deltaTime / outward);
        };
        prepare(position.x, previous.x, kArenaHalfExtents.x);
        prepare(position.y, previous.y, kArenaHalfExtents.y);
        prepare(position.z, previous.z, kArenaHalfExtents.z);
    }
    hasMovementSample_ = true;
    if (std::abs(position.x) > kArenaHalfExtents.x + margin ||
        std::abs(position.y) > kArenaHalfExtents.y + margin ||
        std::abs(position.z) > kArenaHalfExtents.z + margin) active = false;
}

}  // namespace koumakyou
