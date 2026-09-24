#include "game/boss.hpp"

#include "game/bullet_patterns.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace redmist {
namespace {
constexpr Color kRed{245, 76, 96, 255};
constexpr Color kWhite{235, 228, 239, 255};
constexpr std::array<float, 5> kMovementX{3.0F, 0.0F, -3.0F, 0.0F, 3.0F};
constexpr float kMovementStep = 3.2F;
constexpr float kMidbossTimeout = 17.0F;
}

void RumiaBoss::Start() {
    active = true;
    position = {kMovementX.front(), 2.0F, 7.2F};
    health = maxHealth = 145.0F;
    phaseTimer = 0.0F;
    fireTimer = 1.0F;
    moveClock = 0.0F;
    starRotation = 0.0F;
    phase = 1;
}

bool RumiaBoss::Update(float deltaTime, Vector3 playerPosition,
                       BulletSystem& bullets) {
    if (!active) return false;
    phaseTimer += deltaTime;
    moveClock += deltaTime;
    fireTimer -= deltaTime;

    const float segment = std::min(moveClock / kMovementStep, 4.0F);
    const int index = std::min(static_cast<int>(segment), 3);
    const float fraction = std::clamp(segment - static_cast<float>(index),
                                      0.0F, 1.0F);
    const float smooth = fraction * fraction * (3.0F - 2.0F * fraction);
    position = {kMovementX[index] +
                    (kMovementX[index + 1] - kMovementX[index]) * smooth,
                2.0F + std::sin(moveClock * 1.1F) * 0.35F,
                7.2F};

    if (fireTimer <= 0.0F) {
        FireAimedVolley(bullets, playerPosition);
        fireTimer += 0.95F;
    }
    return phaseTimer >= kMidbossTimeout;
}

bool RumiaBoss::AdvancePhase() {
    active = false;
    phase = 0;
    return false;
}

const char* RumiaBoss::SpellName() const { return nullptr; }

void RumiaBoss::FireAimedVolley(BulletSystem& bullets,
                                Vector3 playerPosition) {
    const Vector3 axis = BulletSystem::DirectionTo(position, playerPosition);
    // The outer rings spread through depth for the 3D adaptation.
    pattern::ForEachConeDirection(axis, 2, 10, 0.42F, starRotation,
        [this, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, position,
                          pattern::Scale(direction, 3.25F), 0.15F, kRed);
        });
    bullets.Spawn(BulletOwner::Enemy, position, pattern::Scale(axis, 3.6F),
                  0.14F, kWhite);
    starRotation += 0.27F;
}

}  // namespace redmist
