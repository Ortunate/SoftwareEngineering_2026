#include "game/boss.hpp"

#include "game/bullet_patterns.hpp"

namespace redmist {

Vector3 Boss::MuzzlePosition(Vector3 playerPosition) const {
    return pattern::Add(position,
        pattern::Scale(BulletSystem::DirectionTo(position, playerPosition),
                       hitRadius * 0.55F));
}

}  // namespace redmist
