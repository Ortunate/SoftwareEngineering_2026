#include "game/boss.hpp"

#include "game/bullet_patterns.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace redmist {
namespace {
constexpr Color kRed{245, 76, 96, 255};
constexpr Color kWhite{235, 228, 239, 255};
constexpr Color kBlue{88, 108, 255, 255};
constexpr Color kCyan{68, 230, 239, 255};
constexpr Color kGreen{120, 235, 153, 255};
constexpr std::array<float, 5> kMidbossX{3.0F, 0.0F, -3.0F, 0.0F, 3.0F};
constexpr float kMidbossStep = 3.8F;
constexpr float kCutInDuration = 2.6F;

float PhaseDuration(int phase) {
    switch (phase) {
        case 1: return 14.0F;
        case 2: return 18.0F;
        case 3: return 15.0F;
        default: return 22.0F;
    }
}
}

void RumiaBoss::StartMidboss() {
    encounter_ = Encounter::Midboss;
    active = true;
    phase = 1;
    position = {kMidbossX.front(), 2.0F, 7.2F};
    health = maxHealth = 145.0F;
    phaseTimer = moveClock = starRotation = 0.0F;
    fireTimer = 0.9F;
}

void RumiaBoss::StartFinalBoss() {
    encounter_ = Encounter::Final;
    active = true;
    phase = 1;
    position = {0.0F, 2.5F, 7.2F};
    health = maxHealth = 230.0F;
    phaseTimer = moveClock = starRotation = 0.0F;
    fireTimer = 0.8F;
}

bool RumiaBoss::Update(float deltaTime, Vector3 playerPosition,
                       BulletSystem& bullets) {
    if (!active) return false;
    phaseTimer += deltaTime;
    moveClock += deltaTime;
    fireTimer -= deltaTime;

    if (encounter_ == Encounter::Midboss) {
        // 道中战按五个水平停留点往返，smoothstep 减少转向时的突变。
        const float segment = std::min(moveClock / kMidbossStep, 4.0F);
        const int index = std::min(static_cast<int>(segment), 3);
        const float fraction = std::clamp(segment - static_cast<float>(index),
                                          0.0F, 1.0F);
        const float smooth = fraction * fraction * (3.0F - 2.0F * fraction);
        position = {kMidbossX[index] +
                        (kMidbossX[index + 1] - kMidbossX[index]) * smooth,
                    2.0F + std::sin(moveClock * 1.1F) * 0.35F, 7.2F};
        if (fireTimer <= 0.0F) {
            FireAimedVolley(bullets, playerPosition);
            fireTimer += 0.9F;
        }
        return phaseTimer >= 22.0F;
    }

    position = {std::sin(moveClock * 0.55F) * 2.15F,
                2.5F + std::sin(moveClock * 0.75F) * 0.5F,
                7.2F + std::sin(moveClock * 0.3F) * 0.35F};
    if (fireTimer <= 0.0F) {
        switch (phase) {
            case 1:
                FireAimedStream(bullets, playerPosition);
                fireTimer += 0.22F;
                break;
            case 2:
                CastNightBird(bullets, playerPosition);
                fireTimer += 0.6F;
                break;
            case 3:
                FireMoonlightBeam(bullets, playerPosition);
                fireTimer += 0.75F;
                break;
            case 4:
                CastDemarcation(bullets, playerPosition);
                fireTimer += 0.65F;
                break;
            default: break;
        }
    }
    return phaseTimer >= PhaseDuration(phase);
}

bool RumiaBoss::AdvancePhase() {
    if (!active) return false;
    if (encounter_ == Encounter::Midboss || phase >= 4) {
        active = false;
        phase = 0;
        return false;
    }
    ++phase;
    phaseTimer = 0.0F;
    starRotation = 0.0F;
    switch (phase) {
        case 2: health = maxHealth = 280.0F; break;
        case 3: health = maxHealth = 230.0F; break;
        case 4: health = maxHealth = 330.0F; break;
        default: break;
    }
    // 符卡阶段先留出立绘与名称展示时间，再开始发弹。
    fireTimer = SpellName() != nullptr ? kCutInDuration : 0.8F;
    return true;
}

const char* RumiaBoss::SpellName() const {
    if (!active || encounter_ != Encounter::Final) return nullptr;
    if (phase == 2) return "NIGHT SIGN  -  NIGHT BIRD";
    if (phase == 4) return "BOUNDARY SIGN  -  DEMARCATION";
    return nullptr;
}

Vector3 Boss::MuzzlePosition(Vector3 playerPosition) const {
    return pattern::Add(position,
        pattern::Scale(BulletSystem::DirectionTo(position, playerPosition),
                       hitRadius * 0.55F));
}

void RumiaBoss::FireAimedVolley(BulletSystem& bullets,
                                Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 axis = BulletSystem::DirectionTo(muzzle, playerPosition);
    pattern::ForEachConeDirection(axis, 2, 10, 0.42F, starRotation,
        [muzzle, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, muzzle,
                          pattern::Scale(direction, 3.25F), 0.15F, kRed,
                          BulletVisualKind::RedOrb);
        });
    bullets.Spawn(BulletOwner::Enemy, muzzle, pattern::Scale(axis, 3.6F),
                  0.14F, kWhite, BulletVisualKind::BluePearl);
    starRotation += 0.27F;
}

void RumiaBoss::FireAimedStream(BulletSystem& bullets,
                                Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 axis = BulletSystem::DirectionTo(muzzle, playerPosition);
    bullets.Spawn(BulletOwner::Enemy, muzzle, pattern::Scale(axis, 5.2F),
                  0.13F, kRed, BulletVisualKind::RedOrb);
    if (static_cast<int>(phaseTimer * 4.5F) % 6 == 0) {
        pattern::ForEachConeDirection(axis, 1, 8, 0.48F, starRotation,
            [muzzle, &bullets](Vector3 direction) {
                bullets.Spawn(BulletOwner::Enemy, muzzle,
                              pattern::Scale(direction, 2.75F), 0.14F, kBlue,
                              BulletVisualKind::BluePearl);
            });
    }
    starRotation += 0.23F;
}

void RumiaBoss::CastNightBird(BulletSystem& bullets,
                              Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 axis = BulletSystem::DirectionTo(muzzle, playerPosition);
    pattern::ForEachConeDirection(axis, 2, 12, 0.75F, starRotation,
        [muzzle, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, muzzle,
                          pattern::Scale(direction, 2.4F), 0.15F, kBlue,
                          BulletVisualKind::BluePearl);
        });
    pattern::ForEachConeDirection(axis, 1, 10, 0.38F, -starRotation,
        [muzzle, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, muzzle,
                          pattern::Scale(direction, 3.0F), 0.14F, kCyan,
                          BulletVisualKind::BluePearl);
        });
    starRotation += 0.37F;
}

void RumiaBoss::FireMoonlightBeam(BulletSystem& bullets,
                                  Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 axis = BulletSystem::DirectionTo(muzzle, playerPosition);
    for (int index = 0; index < 12; ++index) {
        const Vector3 start = pattern::Add(muzzle,
                                           pattern::Scale(axis, index * 0.32F));
        bullets.Spawn(BulletOwner::Enemy, start, pattern::Scale(axis, 5.0F),
                      0.16F, kBlue, BulletVisualKind::BluePearl);
    }
    pattern::ForEachConeDirection(axis, 1, 8, 0.55F, starRotation,
        [muzzle, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, muzzle,
                          pattern::Scale(direction, 2.6F), 0.13F, kGreen,
                          BulletVisualKind::SolidSphere);
        });
    starRotation += 0.22F;
}

void RumiaBoss::CastDemarcation(BulletSystem& bullets,
                                Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 axis = BulletSystem::DirectionTo(muzzle, playerPosition);
    pattern::ForEachConeDirection(axis, 2, 18, 0.95F, starRotation,
        [muzzle, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, muzzle,
                          pattern::Scale(direction, 2.25F), 0.14F, kBlue,
                          BulletVisualKind::BluePearl);
        });
    pattern::ForEachConeDirection(axis, 1, 12, 0.52F, -starRotation * 0.8F,
        [muzzle, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, muzzle,
                          pattern::Scale(direction, 3.15F), 0.12F, kGreen,
                          BulletVisualKind::SolidSphere);
        });
    starRotation += 0.3F;
}

}  // namespace redmist
