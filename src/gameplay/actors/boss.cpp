// 本文件负责实现 Boss 共用的发弹点计算。

#include "gameplay/actors/boss.hpp"

#include "gameplay/bullets/patterns.hpp"

namespace koumakyou {

Vector3 Boss::MuzzlePosition(Vector3 playerPosition) const {
    return pattern::Add(position,
        pattern::Scale(BulletSystem::DirectionTo(position, playerPosition),
                       hitRadius * 0.55F));
}

}  // namespace koumakyou
