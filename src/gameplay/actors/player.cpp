// 本文件负责实现玩家移动、视角、射击、Bomb 消耗及敌弹接触规则。

#include "gameplay/actors/player.hpp"

#include "app/math.hpp"
#include "gameplay/arena.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace koumakyou {
namespace {
constexpr float kSpeed = 6.5F;
constexpr float kFocusSpeed = 2.8F;
constexpr float kMouseSensitivity = 0.0024F;
constexpr float kHitRadius = 0.18F;
constexpr float kGrazeRadius = 0.72F;
// 发射点使用玩家局部坐标：gold 位于前方偏下，cyan 在其左右各偏移 0.78 世界单位。
// Power 只改变弹数和张角，不能随通道编号移动这三个发射点。
constexpr float kMuzzleForward = 1.2F;
constexpr float kMuzzleDown = 0.6F;
constexpr float kHomingSide = 0.78F;
// gold 与 cyan 统一使用 20% 不透明度（255×0.2）；渲染时还会乘以贴图自身 Alpha。
constexpr unsigned char kPlayerBulletAlpha = 51;
/// @brief 火力档位同时决定弹数和散射宽度；角度为相对视线的单侧最大偏转（度）。
struct ShotLevel {
    int threshold;
    int straightCount;
    int homingCount;
    float straightHalfAngleDegrees;
    float homingHalfAngleDegrees;
};
constexpr std::array<ShotLevel, 9> kShotLevels{{
    {0, 2, 0, 2.0F, 0.0F}, {8, 2, 2, 3.0F, 3.5F},
    {16, 3, 2, 4.0F, 4.5F}, {32, 3, 3, 4.5F, 5.5F},
    {48, 4, 3, 5.0F, 6.5F}, {64, 4, 4, 6.0F, 7.5F},
    {80, 5, 4, 6.5F, 8.0F}, {96, 5, 5, 7.0F, 9.0F},
    {128, 6, 6, 8.0F, 10.0F},
}};

/// @brief 返回成对散射通道的方向符号（-1 或 +1）；奇数发多出的外侧弹随轮次换边。
float ShotSideSign(int index, bool alternateSides) {
    return (index + static_cast<int>(alternateSides)) % 2 == 0 ? -1.0F : 1.0F;
}
/// @brief 将视线沿横向偏转 angleDegrees（度），返回单位方向；forward/right 须正交且归一化。
/// angleDegrees 的正负分别指向玩家局部右侧和左侧。
Vector3 ShotSpreadDirection(Vector3 forward, Vector3 right, float angleDegrees) {
    const float angle = angleDegrees * std::numbers::pi_v<float> / 180.0F;
    // 在局部正交基上偏转，保证高仰俯角下仍保持张角和弹速；不能直接叠加世界 yaw。
    return game_math::Add(game_math::Scale(forward, std::cos(angle)),
                          game_math::Scale(right, std::sin(angle)));
}
}  // namespace

void Player::Reset() { *this = Player{}; }

void Player::ConfigureRun(int extraLives, int bombs, bool challenge, ShotType shot) {
    lives_ = std::clamp(extraLives, 1, 5);
    bombs_ = std::clamp(bombs, 0, 3);
    challenge_ = challenge;
    shot_ = shot;
}

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

    SetView(yaw_ - input.mouseDelta.x * kMouseSensitivity * input.mouseSensitivityScale,
            pitch_ - input.mouseDelta.y * kMouseSensitivity * input.mouseSensitivityScale);

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
    constexpr Vector3 limit{kArenaHalfExtents.x - kArenaPadding,
                            kArenaHalfExtents.y - kArenaPadding,
                            kArenaHalfExtents.z - kArenaPadding};
    position_.x = std::clamp(position_.x, -limit.x, limit.x);
    position_.y = std::clamp(position_.y, -limit.y, limit.y);
    position_.z = std::clamp(position_.z, -limit.z, limit.z);

    if (allowFire && input.fire && fireTimer_ <= 0.0F) {
        Fire(bullets, bulletVisual, homingVisual);
        fireTimer_ = input.focus ? 0.075F : 0.09F;
    }
    return input.bomb && TryUseBomb();
}

bool Player::AdjustPower(int delta) {
    const int previousPower = power_;
    const long long updated = static_cast<long long>(power_) + delta;
    power_ = static_cast<int>(std::clamp(updated, 0LL,
                                         static_cast<long long>(kMaxPower)));
    return previousPower < kMaxPower && power_ == kMaxPower;
}

PlayerContact Player::ContactEnemyBullet(Bullet& bullet) const {
    if (!bullet.Dangerous() || bullet.owner != BulletOwner::Enemy)
        return PlayerContact::None;
    const float distanceSquared = bullet.DistanceSquaredTo(position_);
    // 使用距离平方比较，命中优先于擦弹；球心或轴线段由 Bullet 统一计算。
    const float hitDistance = bullet.radius + kHitRadius;
    if (distanceSquared <= hitDistance * hitDistance) {
        // 光束持续到其寿命结束，不能因无敌玩家站在轴线上而吞掉整束光；普通弹仍消失。
        if (bullet.beamLength == 0) bullet.active = false;
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
    if (!challenge_) --lives_;
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

Vector3 Player::Up() const {
    return {-std::sin(yaw_) * std::sin(pitch_), std::cos(pitch_),
            -std::cos(yaw_) * std::sin(pitch_)};
}

void Player::Fire(BulletSystem& bullets, VisualId bulletVisual,
                  VisualId homingVisual) {
    const Vector3 forward = Forward();
    const Vector3 right = Right();
    // 相对屏幕下移而非世界 -Y，避免仰俯视时发射点重新遮挡准星。
    const Vector3 muzzle = game_math::Add(position_,
        game_math::Subtract(game_math::Scale(forward, kMuzzleForward),
                            game_math::Scale(Up(), kMuzzleDown)));
    const ShotLevel* level = &kShotLevels.front();
    for (const ShotLevel& candidate : kShotLevels) {
        if (power_ < candidate.threshold) break;
        level = &candidate;
    }
    for (int index = 0; index < level->straightCount; ++index) {
        // gold 只有一个中央模块：整组弹从同一点均匀展开，奇数发的中央弹沿视线直飞。
        const float lane = level->straightCount > 1
                               ? 2.0F * index / (level->straightCount - 1) - 1.0F
                               : 0.0F;
        const Vector3 direction = ShotSpreadDirection(forward, right,
            lane * (shot_ == ShotType::Needle ? 1.5F : level->straightHalfAngleDegrees));
        Bullet* bullet = bullets.Spawn(BulletOwner::Player, muzzle,
            game_math::Scale(direction, 20.0F),
            0.09F,
            Color{255, 255, 255, kPlayerBulletAlpha}, bulletVisual);
        if (bullet) bullet->damage = shot_ == ShotType::Needle ? 3.3F : 2.0F;
    }
    for (int index = 0; shot_ == ShotType::HomingAmulet && index < level->homingCount; ++index) {
        const float side = ShotSideSign(index, alternateCyanSide_);
        const Vector3 origin = game_math::Add(muzzle,
                                              game_math::Scale(right, side * kHomingSide));
        const float laneFraction = static_cast<float>(index / 2 + 1) /
                                   static_cast<float>((level->homingCount + 1) / 2);
        const Vector3 direction = ShotSpreadDirection(forward, right,
            side * laneFraction * level->homingHalfAngleDegrees);
        Bullet* bullet = bullets.Spawn(BulletOwner::Player, origin,
            game_math::Scale(direction, 14.0F),
            0.10F, Color{255, 255, 255, kPlayerBulletAlpha}, homingVisual);
        if (bullet) {
            bullet->damage = 1.0F;
            bullet->homingTurnRate = 5.5F;
        }
    }
    alternateCyanSide_ = !alternateCyanSide_;
}

bool Player::TryUseBomb() {
    if (bombs_ <= 0 || bombCooldown_ > 0.0F) return false;
    --bombs_;
    bombCooldown_ = 1.0F;
    invulnerableTimer_ = 2.6F;
    return true;
}

}  // namespace koumakyou
