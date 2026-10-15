// 本文件负责实现 Boss 共用的发弹点计算和默认单段血条快照。

#include "gameplay/actors/boss.hpp"

#include "gameplay/bullets/patterns.hpp"
#include <algorithm>

namespace koumakyou {

BossHealthBar Boss::HealthBar() const {
    if (!active || maxHealth <= 0) return {};
    BossHealthBar bar;
    bar.count = 1;
    bar.segments[0] = {1, std::clamp(health / maxHealth, 0.0F, 1.0F),
                       SpellName() ? BossHealthKind::Spell : BossHealthKind::NonSpell};
    return bar;
}

Vector3 Boss::MuzzlePosition(Vector3 playerPosition) const {
    return pattern::Add(position,
        pattern::Scale(BulletSystem::DirectionTo(position, playerPosition),
                       hitRadius * 0.55F));
}

}  // namespace koumakyou
