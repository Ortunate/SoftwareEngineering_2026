// 本文件负责实现玩家移动、视角、射击、Bomb 消耗及敌弹接触规则。

#include "gameplay/actors/player.hpp"

#include "app/math.hpp"
#include "gameplay/arena.hpp"
#include "gameplay/actors/reimu_homing.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace koumakyou {
namespace {
constexpr float kSpeed = 6.5F;
constexpr float kFocusSpeed = 2.8F;
constexpr float kMouseSensitivity = 0.0024F;
constexpr float kHitRadius = 0.18F;
constexpr float kGrazeRadius = 0.72F;
// 发射点使用玩家局部坐标：gold 位于前方偏下，cyan 与太极圈共用视线两侧、略偏下的位置。
// Power 只改变弹数和张角，不能随通道编号移动这三个发射点。
// 贴近目标时三个偏移等比缩短，保证发射点仍位于玩家与目标之间。
constexpr float kMuzzleForward = 1.2F;
constexpr float kMuzzleDown = 0.6F;
constexpr float kHomingSide = 0.78F;
constexpr float kOptionDown = 0.35F;
// 原作横向间距由 24 像素缩至 8 像素，过渡用 8 个 60 Hz 帧；世界侧距沿用现有三维布局。
constexpr float kFocusedHomingSide = kHomingSide / 3.0F;
constexpr float kOptionFocusSeconds = 8.0F / 60.0F;
constexpr float kDefaultAimDepth = 20.0F;
/// @brief 直射火力档位决定弹数和散射宽度；角度为相对准星的单侧最大偏转（度）。
struct ShotLevel {
    int threshold;
    int straightCount;
    float straightHalfAngleDegrees;
};
constexpr std::array<ShotLevel, 9> kShotLevels{{
    {0, 2, 2.0F}, {8, 2, 3.0F}, {16, 3, 4.0F}, {32, 3, 4.5F},
    {48, 4, 5.0F}, {64, 4, 6.0F}, {80, 5, 6.5F}, {96, 5, 7.0F}, {128, 6, 8.0F},
}};

/// @brief 求最近准星命中球体的深度；forward 须为单位向量，返回正世界单位，未命中时使用默认深度。
float AimDepth(Vector3 position, Vector3 forward, std::span<const PlayerAimTarget> targets) {
    float nearestEntry = std::numeric_limits<float>::max();
    float depth = kDefaultAimDepth;
    for (const auto& target : targets) {
        if (target.radius <= 0.0F) continue;
        const auto relative = game_math::Subtract(target.position, position);
        const float along = relative.x * forward.x + relative.y * forward.y + relative.z * forward.z;
        const auto perpendicular = game_math::Subtract(relative, game_math::Scale(forward, along));
        const float halfChordSquared = target.radius * target.radius -
            (perpendicular.x * perpendicular.x + perpendicular.y * perpendicular.y + perpendicular.z * perpendicular.z);
        if (along <= 0.0F || halfChordSquared < 0.0F) continue;
        const float entry = std::max(0.0F, along - std::sqrt(halfChordSquared));
        if (entry >= nearestEntry) continue;
        nearestEntry = entry;
        // 选择首先遮住射线的判定体，但瞄向射线上穿过其内部的点，避免从偏下的模块擦过球面。
        // 不能直接瞄准球心，否则准星边缘的目标会获得额外吸附；极近距离保留正深度防止零向量。
        depth = std::max(0.05F, along);
    }
    return depth;
}

/// @brief 从模块 origin 指向准星目标平面的散射落点；angleDegrees 为相对准星的横向角（度）。
/// position 为视点，forward/right 须正交归一化，depth 为正世界单位；返回单位方向。
Vector3 ConvergingShotDirection(Vector3 origin, Vector3 position, Vector3 forward,
                               Vector3 right, float depth, float angleDegrees) {
    const float angle = angleDegrees * std::numbers::pi_v<float> / 180.0F;
    // 散射落点对称分布在垂直视线的同一深度平面，任意仰俯角下仍以准星为中心。
    const auto point = game_math::Add(position, game_math::Add(game_math::Scale(forward, depth),
        game_math::Scale(right, std::tan(angle) * depth)));
    return BulletSystem::DirectionTo(origin, point);
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
    homingFrame_ = -1;
    homingFractionalFrame_ = 0.0;
    optionFocus_ = 0.0F;
    optionAimDepth_ = kDefaultAimDepth;
    visualAge_ = 0.0;
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
                    VisualId homingVisual, bool allowFire,
                    std::span<const PlayerAimTarget> aimTargets) {
    invulnerableTimer_ = std::max(0.0F, invulnerableTimer_ - deltaTime);
    bombCooldown_ = std::max(0.0F, bombCooldown_ - deltaTime);
    fireTimer_ -= deltaTime;
    visualAge_ += deltaTime;
    optionFocus_ = std::clamp(optionFocus_ + (input.focus ? deltaTime : -deltaTime) / kOptionFocusSeconds, 0.0F, 1.0F);

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
    optionAimDepth_ = AimDepth(position_, forward, aimTargets);

    if (allowFire && input.fire && fireTimer_ <= 0.0F) {
        Fire(bullets, bulletVisual, aimTargets);
        fireTimer_ = input.focus ? 0.075F : 0.09F;
    }
    if (!allowFire || shot_ != ShotType::HomingAmulet) {
        homingFrame_ = -1;
        homingFractionalFrame_ = 0.0;
    } else if (homingFrame_ < 0) {
        if (input.fire) {
            homingFrame_ = 0;
            homingFractionalFrame_ = 0.0;
            FireHomingFrame(bullets, homingVisual, aimTargets);
        }
    } else {
        homingFractionalFrame_ += static_cast<double>(deltaTime) * reimu_homing::kFramesPerSecond;
        // 用独立的整数逻辑帧补发通道，避免帧率、Focus 和金色弹冷却改变追踪弹相位。
        // 原作每次开火至少完成 0…29 帧；满火力的 period=16 不能改成连续计时，否则跨窗口节奏会变。
        while (homingFractionalFrame_ >= 1.0 - 0.000001) {
            homingFractionalFrame_ = std::max(0.0, homingFractionalFrame_ - 1.0);
            if (++homingFrame_ == reimu_homing::kCycleFrames) {
                if (!input.fire) {
                    homingFrame_ = -1;
                    homingFractionalFrame_ = 0.0;
                    break;
                }
                homingFrame_ = 0;
            }
            FireHomingFrame(bullets, homingVisual, aimTargets);
        }
    }
    return input.bomb && TryUseBomb();
}

bool Player::AdjustPower(int delta) {
    const int previousPower = power_;
    const long long updated = static_cast<long long>(power_) + delta;
    power_ = static_cast<int>(std::clamp(updated, static_cast<long long>(kMinPower),
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

std::array<Vector3, 2> Player::OptionPositions() const {
    const float scale = OptionScale();
    // 平滑插值只改变局部侧距；不跟随世界坐标轴，转向或仰俯视时仍停留在画面两侧。
    const float blend = optionFocus_ * optionFocus_ * (3.0F - 2.0F * optionFocus_);
    const float side = (kHomingSide + (kFocusedHomingSide - kHomingSide) * blend) * scale;
    const auto center = game_math::Add(position_, game_math::Subtract(
        game_math::Scale(Forward(), kMuzzleForward * scale), game_math::Scale(Up(), kOptionDown * scale)));
    const auto offset = game_math::Scale(Right(), side);
    return {game_math::Subtract(center, offset), game_math::Add(center, offset)};
}

float Player::OptionScale() const {
    return std::min(1.0F, optionAimDepth_ / (2.0F * kMuzzleForward));
}

void Player::Fire(BulletSystem& bullets, VisualId bulletVisual,
                  std::span<const PlayerAimTarget> aimTargets) {
    const Vector3 forward = Forward();
    const Vector3 right = Right();
    const float depth = AimDepth(position_, forward, aimTargets);
    const float muzzleScale = std::min(1.0F, depth / (2.0F * kMuzzleForward));
    // 相对屏幕下移而非世界 -Y，避免仰俯视时发射点重新遮挡准星。
    const Vector3 muzzle = game_math::Add(position_,
        game_math::Subtract(game_math::Scale(forward, kMuzzleForward * muzzleScale),
                            game_math::Scale(Up(), kMuzzleDown * muzzleScale)));
    const ShotLevel* level = &kShotLevels.front();
    for (const ShotLevel& candidate : kShotLevels) {
        if (power_ < candidate.threshold) break;
        level = &candidate;
    }
    for (int index = 0; index < level->straightCount; ++index) {
        // gold 从同一模块出射，奇数发的中央弹穿过准星射线，偶数发围绕准星对称展开。
        const float lane = level->straightCount > 1
                               ? 2.0F * index / (level->straightCount - 1) - 1.0F
                               : 0.0F;
        const Vector3 direction = ConvergingShotDirection(muzzle, position_, forward, right, depth,
            lane * (shot_ == ShotType::Needle ? 1.5F : level->straightHalfAngleDegrees));
        Bullet* bullet = bullets.Spawn(BulletOwner::Player, muzzle,
            game_math::Scale(direction, 20.0F),
            0.09F,
            Color{255, 255, 255, kShotAlpha}, bulletVisual);
        if (bullet) bullet->damage = shot_ == ShotType::Needle ? 3.3F : 2.0F;
    }
}

void Player::FireHomingFrame(BulletSystem& bullets, VisualId homingVisual,
                             std::span<const PlayerAimTarget> aimTargets) {
    const auto& level = reimu_homing::ForPower(power_);
    const auto origins = OptionPositions();
    for (const auto& pair : level.pairs) {
        if (homingFrame_ % pair.periodFrames != pair.phaseFrame) continue;
        const Vector3 forward = Forward();
        const Vector3 right = Right();
        const float depth = AimDepth(position_, forward, aimTargets);
        for (int index = 0; index < 2; ++index) {
            const float side = index == 0 ? -1.0F : 1.0F;
            const auto origin = origins[index];
            const auto direction = ConvergingShotDirection(origin, position_, forward, right, depth,
                                                           side * pair.halfAngleDegrees);
            if (auto* bullet = bullets.Spawn(BulletOwner::Player, origin, game_math::Scale(direction, 14.0F),
                                            0.10F, Color{255, 255, 255, kShotAlpha}, homingVisual)) {
                bullet->damage = 1.0F;
                bullet->homingTurnRate = 5.5F;
            }
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

}  // namespace koumakyou
