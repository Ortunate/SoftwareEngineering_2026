// 本文件负责实现玩家移动、视角、射击、Bomb 消耗及敌弹接触规则。

#include "gameplay/actors/player.hpp"

#include "app/math.hpp"
#include "gameplay/arena.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace redmist {
namespace {
constexpr float kSpeed = 6.5F;
constexpr float kFocusSpeed = 2.8F;
constexpr float kMouseSensitivity = 0.0024F;
constexpr float kHitRadius = 0.18F;
constexpr float kGrazeRadius = 0.72F;
/// @brief 使用原作式火力成长门槛；弹数按三维战场调整，直射和追踪各有独立通道。
struct ShotLevel {
    int threshold;
    int straightCount;
    int homingCount;
};
constexpr std::array<ShotLevel, 9> kShotLevels{{
    {0, 2, 0}, {8, 2, 2}, {16, 3, 2}, {32, 3, 3}, {48, 4, 3},
    {64, 4, 4}, {80, 5, 4}, {96, 5, 5}, {128, 6, 6},
}};
}  // namespace

void Player::Reset() { *this = Player{}; }

void Player::ResetPose() {
    position_ = {0.0F, 0.0F, -6.5F};
    yaw_ = 0.0F;
    pitch_ = 0.0F;
}

void Player::SetView(float yawRadians, float pitchRadians) {
    yaw_ = yawRadians;
    pitch_ = std::clamp(pitchRadians, -1.45F, 1.45F);
}

void Player::SetInvulnerability(float seconds) {
    invulnerableTimer_ = std::max(0.0F, seconds);
}

bool Player::Update(float deltaTime, const PlayerInput& input,
                    BulletSystem& bullets, VisualId bulletVisual,
                    VisualId homingVisual, bool allowFire) {
    invulnerableTimer_ = std::max(0.0F, invulnerableTimer_ - deltaTime);
    bombCooldown_ = std::max(0.0F, bombCooldown_ - deltaTime);
    fireTimer_ -= deltaTime;

    SetView(yaw_ - input.mouseDelta.x * kMouseSensitivity,
            pitch_ - input.mouseDelta.y * kMouseSensitivity);

    Vector3 movement{};
    const Vector3 forward = Forward();
    const Vector3 right = Right();
    if (input.forward) movement = game_math::Add(movement, forward);
    if (input.backward) movement = game_math::Subtract(movement, forward);
    if (input.moveRight) movement = game_math::Add(movement, right);
    if (input.moveLeft) movement = game_math::Subtract(movement, right);
    if (input.ascend) movement.y += 1.0F;
    if (input.descend) movement.y -= 1.0F;

    // 归一化后斜向移动不会比单轴移动更快。
    movement = game_math::Normalize(movement);
    const float speed = input.focus ? kFocusSpeed : kSpeed;
    position_ = game_math::Add(position_,
                               game_math::Scale(movement, speed * deltaTime));
    const float limit = kArenaHalfExtent - kArenaPadding;
    position_.x = std::clamp(position_.x, -limit, limit);
    position_.y = std::clamp(position_.y, -limit, limit);
    position_.z = std::clamp(position_.z, -limit, limit);

    if (allowFire && input.fire && fireTimer_ <= 0.0F) {
        Fire(bullets, bulletVisual, homingVisual);
        fireTimer_ = input.focus ? 0.075F : 0.09F;
    }
    return input.bomb && TryUseBomb();
}

void Player::AdjustPower(int delta) {
    const long long updated = static_cast<long long>(power_) + delta;
    power_ = static_cast<int>(std::clamp(updated, 0LL,
                                         static_cast<long long>(kMaxPower)));
}

PlayerContact Player::ContactEnemyBullet(Bullet& bullet) const {
    if (!bullet.active || bullet.owner != BulletOwner::Enemy)
        return PlayerContact::None;
    const float distanceSquared = game_math::DistanceSquared(bullet.position,
                                                              position_);
    // 使用距离平方比较；命中优先于擦弹，并使本帧多颗命中弹各自失活。
    const float hitDistance = bullet.radius + kHitRadius;
    if (distanceSquared <= hitDistance * hitDistance) {
        bullet.active = false;
        return invulnerableTimer_ <= 0.0F ? PlayerContact::Hit
                                          : PlayerContact::None;
    }
    const float grazeDistance = bullet.radius + kGrazeRadius;
    if (!bullet.grazed && distanceSquared <= grazeDistance * grazeDistance) {
        bullet.grazed = true;
        return PlayerContact::Graze;
    }
    return PlayerContact::None;
}

bool Player::TakeHit() {
    --lives_;
    AdjustPower(-16);
    bombs_ = std::max(bombs_, 2);
    ResetPose();
    invulnerableTimer_ = 2.8F;
    return lives_ >= 0;
}

void Player::SyncCamera(Camera3D& camera) const {
    camera.position = position_;
    camera.target = game_math::Add(position_, Forward());
    camera.up = {0.0F, 1.0F, 0.0F};
}

int Player::RemainingLives() const { return std::max(0, lives_ + 1); }

Vector3 Player::Forward() const {
    return BulletSystem::DirectionFromYawPitch(yaw_, pitch_);
}

Vector3 Player::Right() const {
    return {-std::cos(yaw_), 0.0F, std::sin(yaw_)};
}

void Player::Fire(BulletSystem& bullets, VisualId bulletVisual,
                  VisualId homingVisual) const {
    const Vector3 forward = Forward();
    const Vector3 right = Right();
    const Vector3 muzzle = game_math::Add(position_,
                                          game_math::Scale(forward, 0.55F));
    const ShotLevel* level = &kShotLevels.front();
    for (const ShotLevel& candidate : kShotLevels) {
        if (power_ < candidate.threshold) break;
        level = &candidate;
    }
    for (int index = 0; index < level->straightCount; ++index) {
        const float offset = (static_cast<float>(index) -
                              static_cast<float>(level->straightCount - 1) * 0.5F) *
                             0.16F;
        const Vector3 origin = game_math::Add(muzzle,
                                              game_math::Scale(right, offset));
        Bullet* bullet = bullets.Spawn(BulletOwner::Player, origin,
            game_math::Scale(forward, 20.0F), 0.09F,
            Color{255, 255, 255, 185}, bulletVisual);
        if (bullet) bullet->damage = 2.0F;
    }
    for (int index = 0; index < level->homingCount; ++index) {
        const float offset = (static_cast<float>(index) -
                              static_cast<float>(level->homingCount - 1) * 0.5F) *
                             0.22F;
        const Vector3 origin = game_math::Add(muzzle,
                                              game_math::Scale(right, offset));
        const Vector3 spread = game_math::Add(forward,
                                              game_math::Scale(right, offset * 0.3F));
        Bullet* bullet = bullets.Spawn(BulletOwner::Player, origin,
            game_math::Scale(BulletSystem::DirectionTo({}, spread), 14.0F),
            0.10F, Color{255, 255, 255, 165}, homingVisual);
        if (bullet) {
            bullet->damage = 1.0F;
            bullet->homingTurnRate = 5.5F;
        }
    }
}

bool Player::TryUseBomb() {
    if (bombs_ <= 0 || bombCooldown_ > 0.0F) return false;
    --bombs_;
    bombCooldown_ = 1.0F;
    invulnerableTimer_ = 2.6F;
    return true;
}

}  // namespace redmist
