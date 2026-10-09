// 本文件负责实现第一面露米娅道中与关底战的移动、阶段和弹幕技能。

#include "stages/stage1/boss.hpp"
#include "stages/stage1/ids.hpp"

#include "gameplay/bullets/patterns.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace redmist::stage1 {
namespace {
constexpr Color kRed{245, 76, 96, 255};
constexpr Color kWhite{235, 228, 239, 255};
constexpr Color kBlue{88, 108, 255, 255};
constexpr Color kCyan{68, 230, 239, 255};
constexpr Color kGreen{120, 235, 153, 255};
constexpr std::array<float, 5> kMidbossX{3.0F, 0.0F, -3.0F, 0.0F, 3.0F};
constexpr float kMidbossStep = 3.8F;
constexpr float kCutInDuration = 2.6F;

/// @brief 查询关底 Boss 指定阶段的超时时长。
/// @param phase 从 1 开始的阶段序号。
/// @return 阶段时长，单位为秒。
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
    presentation.Reset(ActorKind::Boss);
    visual = kRumia;
    encounter_ = Encounter::Midboss;
    active = true;
    phase = 1;
    position = {kMidbossX.front(), 2.0F, 7.2F};
    health = maxHealth = 145.0F;
    phaseTimer = moveClock = starRotation = 0.0F;
    fireTimer = 0.9F;
}

void RumiaBoss::StartFinalBoss() {
    presentation.Reset(ActorKind::Boss);
    visual = kRumia;
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
    presentation.Update(deltaTime);
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
                fireTimer += 2.8F;
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
    if (phase == 4) return "DARK SIGN  -  DEMARCATION";
    return nullptr;
}

void RumiaBoss::FireAimedVolley(BulletSystem& bullets,
                                Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 axis = BulletSystem::DirectionTo(muzzle, playerPosition);
    pattern::ForEachConeDirection(axis, 2, 10, 0.42F, starRotation,
        [muzzle, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, muzzle,
                          pattern::Scale(direction, 3.25F), 0.15F, kRed,
                          kRedBullet);
        });
    bullets.Spawn(BulletOwner::Enemy, muzzle, pattern::Scale(axis, 3.6F),
                  0.14F, kWhite, kBlueBullet);
    starRotation += 0.27F;
}

void RumiaBoss::FireAimedStream(BulletSystem& bullets,
                                Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 axis = BulletSystem::DirectionTo(muzzle, playerPosition);
    bullets.Spawn(BulletOwner::Enemy, muzzle, pattern::Scale(axis, 5.2F),
                  0.13F, kRed, kRedBullet);
    if (static_cast<int>(phaseTimer * 4.5F) % 6 == 0) {
        pattern::ForEachConeDirection(axis, 1, 8, 0.48F, starRotation,
            [muzzle, &bullets](Vector3 direction) {
                bullets.Spawn(BulletOwner::Enemy, muzzle,
                              pattern::Scale(direction, 2.75F), 0.14F, kBlue,
                              kBlueBullet);
            });
    }
    starRotation += 0.23F;
}

void RumiaBoss::CastNightBird(BulletSystem& bullets,
                              Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 axis = BulletSystem::DirectionTo(muzzle, playerPosition);
    // 内外两层绕各自发射时的瞄准轴反向转弯，离开 Boss 后也保持三维曲线。
    pattern::ForEachConeDirection(axis, 2, 12, 0.75F, starRotation,
        [muzzle, axis, &bullets](Vector3 direction) {
            if (Bullet* bullet = bullets.Spawn(BulletOwner::Enemy, muzzle,
                    pattern::Scale(direction, 2.4F), 0.15F, kBlue, kBlueBullet)) {
                bullet->rotationAxis = axis;
                bullet->angularVelocity = 0.16F;
            }
        });
    pattern::ForEachConeDirection(axis, 1, 10, 0.38F, -starRotation,
        [muzzle, axis, &bullets](Vector3 direction) {
            if (Bullet* bullet = bullets.Spawn(BulletOwner::Enemy, muzzle,
                    pattern::Scale(direction, 3.0F), 0.14F, kCyan, kBlueBullet)) {
                bullet->rotationAxis = axis;
                bullet->angularVelocity = -0.13F;
            }
        });
    starRotation += 0.37F;
}

void RumiaBoss::FireMoonlightBeam(BulletSystem& bullets,
                                  Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 axis = BulletSystem::DirectionTo(muzzle, playerPosition);
    // 方向在预警开始时锁定；之后 Boss 横移也不能使光束追着玩家扫过。
    // 1.05 秒预警 + 0.65 秒生效，再留 1.1 秒间隔，给三个轴向的闪避留足窗口。
    bullets.SpawnBeam(muzzle, axis, 24.0F, 0.22F, 1.05F, 0.65F, kCyan);
    pattern::ForEachConeDirection(axis, 1, 8, 0.55F, starRotation,
        [muzzle, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, muzzle,
                          pattern::Scale(direction, 2.6F), 0.13F, kGreen,
                          kGreenSphere);
        });
    starRotation += 0.22F;
}

void RumiaBoss::CastDemarcation(BulletSystem& bullets,
                                Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 axis = BulletSystem::DirectionTo(muzzle, playerPosition);
    // 三个深度层共享同一开口；每轮缓慢转动开口，不能给每层随机角度封死通道。
    for (int layer = 0; layer < 3; ++layer) {
        const float angle = 0.32F + layer * 0.27F;
        pattern::ForEachOpenRingDirection(axis, 24, angle, starRotation, 0.42F,
            [muzzle, layer, &bullets](Vector3 direction) {
                bullets.Spawn(BulletOwner::Enemy, muzzle,
                    pattern::Scale(direction, 2.8F - layer * 0.3F),
                    layer == 0 ? 0.12F : 0.14F, layer == 0 ? kRed : layer == 1 ? kGreen : kBlue,
                    layer == 0 ? kRedBullet : layer == 1 ? kGreenSphere : kBlueBullet);
            });
    }
    // 稀疏的中央瞄准弹使玩家不能永远停在轴线；与宽环错速，保留闪避空间。
    bullets.Spawn(BulletOwner::Enemy, muzzle, pattern::Scale(axis, 2.1F), 0.12F, kGreen, kGreenSphere);
    starRotation += 0.15F;
}

}  // namespace redmist::stage1
