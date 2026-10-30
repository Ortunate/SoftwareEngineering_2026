// 本文件负责实现玩家移动、视角、射击、Bomb 消耗及敌弹/敌方本体接触规则。

#include "gameplay/actors/player.hpp"

#include "app/math.hpp"
#include "gameplay/arena.hpp"
#include "gameplay/effects/player_bomb.hpp"

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
constexpr float kHitRadius = native_sizes::kPlayerHitRadius;
constexpr float kGrazeRadius = native_sizes::kPlayerGrazeRadius;
// 发射点使用玩家局部坐标：gold 位于前方偏下，cyan 与太极圈共用视线两侧、略偏下的位置。
// 三个模块基准位置不随 Power 变化；梦札多束侧针仅在对应圈内叠加原始通道偏移。
// 贴近目标时三个偏移等比缩短，保证发射点仍位于玩家与目标之间。
constexpr float kMuzzleForward = 1.2F;
constexpr float kMuzzleDown = 0.6F;
constexpr float kHomingSide = 0.78F;
constexpr float kOptionDown = 0.35F;
// 第一视角的模块局部布局保留既有投影比例，与本次原生显示/判定尺寸独立，避免针束移到视线上方。
constexpr float kMuzzleLayoutPixel = 0.42F/36;
// 原作横向间距由 24 像素缩至 8 像素，过渡用 8 个 60 Hz 帧；世界侧距沿用现有三维布局。
constexpr float kFocusedHomingSide = kHomingSide / 3.0F;
constexpr float kOptionFocusSeconds = 8.0F / 60.0F;
constexpr float kDefaultAimDepth = 20.0F;
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
    netMisses_ = 0;
}

void Player::ResetPose() {
    position_ = {0.0F, 0.0F, -6.5F};
    yaw_ = 0.0F;
    pitch_ = 0.0F;
    secondaryFrame_ = -1;
    secondaryFractionalFrame_ = 0.0;
    laserReadyAt_={};++weaponEpoch_;
    optionFocus_ = 0.0F;
    optionAimDepth_ = kDefaultAimDepth;
    visualAge_ = 0.0;
    bombCooldown_ = 0.0F;
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
                    VisualId secondaryVisual, bool allowFire,
                    std::span<const PlayerAimTarget> aimTargets) {
    PlayerShotVisuals visuals;visuals.primary=bulletVisual;visuals.secondary.fill(secondaryVisual);
    return Update(deltaTime,input,bullets,visuals,allowFire,aimTargets);
}

bool Player::Update(float deltaTime, const PlayerInput& input, BulletSystem& bullets,
                    const PlayerShotVisuals& visuals, bool allowFire,
                    std::span<const PlayerAimTarget> aimTargets) {
    // 恋札在整个 300 帧演出内保持原作 0.3 倍速度；跨过终点的大帧只缩放仍有效的时间段。
    const float slowedSeconds=shot_==ShotType::IllusionLaser ? std::min(deltaTime,bombCooldown_) : 0;
    invulnerableTimer_ = std::max(0.0F, invulnerableTimer_ - deltaTime);
    bombCooldown_ = std::max(0.0F, bombCooldown_ - deltaTime);
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
    const float speed = (input.focus ? kFocusSpeed : kSpeed) * (IsMarisa() ? 1.25F : 1.0F);
    position_ = game_math::Add(position_,
                               game_math::Scale(movement, speed * (deltaTime-0.7F*slowedSeconds)));
    constexpr Vector3 limit{kArenaHalfExtents.x - kArenaPadding,
                            kArenaHalfExtents.y - kArenaPadding,
                            kArenaHalfExtents.z - kArenaPadding};
    position_.x = std::clamp(position_.x, -limit.x, limit.x);
    position_.y = std::clamp(position_.y, -limit.y, limit.y);
    position_.z = std::clamp(position_.z, -limit.z, limit.z);
    optionAimDepth_ = AimDepth(position_, forward, aimTargets);

    UpdateLasers(allowFire,bullets,aimTargets);
    if (!allowFire) {
        secondaryFrame_ = -1;
        secondaryFractionalFrame_ = 0.0;
    } else if (secondaryFrame_ < 0) {
        if (input.fire) {
            secondaryFrame_ = 0;
            secondaryFractionalFrame_ = 0.0;
            FireNativeFrame(bullets, visuals, aimTargets);
        }
    } else {
        secondaryFractionalFrame_ += static_cast<double>(deltaTime) * native_combat::kFramesPerSecond;
        // 四机体主副弹共用原始逻辑窗口，Focus 不再改变主弹冷却。
        // 原作每次开火至少完成 0…29 帧；满火力的 period=16 不能改成连续计时，否则跨窗口节奏会变。
        while (secondaryFractionalFrame_ >= 1.0 - 0.000001) {
            secondaryFractionalFrame_ = std::max(0.0, secondaryFractionalFrame_ - 1.0);
            if (++secondaryFrame_ == native_combat::kShotWindowFrames) {
                if (!input.fire) {
                    secondaryFrame_ = -1;
                    secondaryFractionalFrame_ = 0.0;
                    break;
                }
                secondaryFrame_ = 0;
            }
            FireNativeFrame(bullets, visuals, aimTargets);
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
        if (!bullet.IsBeam()) bullet.active = false;
        return invulnerableTimer_ <= 0.0F ? PlayerContact::Hit
                                          : PlayerContact::None;
    }
    const float grazeDistance = bullet.radius + kGrazeRadius;
    if (distanceSquared <= grazeDistance * grazeDistance && bullet.ConsumeGraze()) {
        return PlayerContact::Graze;
    }
    return PlayerContact::None;
}

bool Player::ContactEnemyBody(Vector3 position,float radius) const {
    if(invulnerableTimer_>0 || !std::isfinite(radius) || radius<=0)return false;
    const float contactRadius=kHitRadius+radius;
    return game_math::DistanceSquared(position_,position)<=contactRadius*contactRadius;
}

bool Player::TakeHit() {
    ++netMisses_;
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
void Player::CollectResources(int extraLives,int bombs) {
    if(extraLives<0||bombs<0)throw std::invalid_argument("Negative resource pickup");
    // 原生Challenge用残机抵扣已确认的Miss，不能套库存上限或把负数钳至零。
    if(challenge_)netMisses_-=extraLives;
    else lives_=std::min(8,lives_+std::min(extraLives,8));
    bombs_=std::min(8,bombs_+std::min(bombs,8));
}

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

void Player::UpdateLasers(bool allowFire, BulletSystem& bullets,
                          std::span<const PlayerAimTarget> aimTargets) {
    if(!allowFire)laserReadyAt_={};
    const auto centers=OptionPositions();
    bullets.Pool().ForEachActive([&](Bullet& bullet) {
        if(bullet.owner!=BulletOwner::Player || bullet.playerShotKind!=PlayerShotKind::Laser)return;
        if(!allowFire || !IsMarisa() || bullet.weaponEpoch!=weaponEpoch_ || bullet.playerSlot<0 || bullet.playerSlot>1) {bullet.active=false;return;}
        bullet.position=centers[bullet.playerSlot];
        const float depth=AimDepth(position_,Forward(),aimTargets);
        bullet.beamDirection=ConvergingShotDirection(bullet.position,position_,Forward(),Right(),depth,0);
    });
}

void Player::FireNativeFrame(BulletSystem& bullets, const PlayerShotVisuals& visuals,
                             std::span<const PlayerAimTarget> aimTargets) {
    const auto centers=OptionPositions();
    const auto forward=Forward(),right=Right(),up=Up();
    const float depth=AimDepth(position_,forward,aimTargets);
    const float scale=OptionScale(),pixelUnit=kMuzzleLayoutPixel*scale;
    // 低帧率一次补发多个逻辑帧时，按各帧实际时刻检查副机槽，不能让整个显示帧提前冷却完成。
    const double fireTime=visualAge_-secondaryFractionalFrame_/60;
    const auto main=game_math::Add(position_,game_math::Subtract(
        game_math::Scale(forward,kMuzzleForward*scale),game_math::Scale(up,kMuzzleDown*scale)));
    for(const auto& shot:native_combat::ForPower(shot_,power_).shots) {
        if(shot.kind==3) {
            const int slot=shot.phase;
            // 原表的 period 是槽冷却（120～330 帧），phase 是槽号；持续射击也只能各保留一束。
            if(laserReadyAt_[slot]>fireTime+0.000001)continue;
            bool existing=false;
            bullets.Pool().ForEachActive([&](const Bullet& bullet) {
                existing|=bullet.owner==BulletOwner::Player && bullet.playerShotKind==PlayerShotKind::Laser &&
                          bullet.weaponEpoch==weaponEpoch_ && bullet.playerSlot==slot;
            });
            if(existing)continue;
            const auto origin=centers[slot];
            if(auto* beam=bullets.Spawn(BulletOwner::Player,origin,{},shot.width*native_sizes::kWorldPerPixel*0.5F,
                                       Color{255,255,255,kShotAlpha},visuals.secondary[shot.script-1125])) {
                beam->visualScale=scale;beam->sourceShotRva=shot.sourceRva;beam->playerShotKind=PlayerShotKind::Laser;beam->playerSlot=slot;beam->weaponEpoch=weaponEpoch_;
                beam->beamDirection=ConvergingShotDirection(origin,position_,forward,right,depth,0);
                beam->beamLength=shot.height*native_sizes::kWorldPerPixel;beam->damage=0;beam->damagePerSecond=shot.damage*60;
                // 原动画在剩余 70 帧收到标签 1，再淡出 30 帧；槽冷却仍持续至表中 period 结束。
                beam->laserFadeAt=(shot.period-70)/60.0F;beam->maxAge=(shot.period-40)/60.0F;
                laserReadyAt_[slot]=fireTime+shot.period/60.0;
            }
            continue;
        }
        // 保留原表相位，即使 phase>=period 不可达也不能归一化，否则会凭空增加低档导弹。
        if(secondaryFrame_%shot.period!=shot.phase)continue;
        const auto base=shot.module==0 ? main : centers[shot.module-1];
        const auto origin=game_math::Add(base,game_math::Subtract(game_math::Scale(right,shot.x*pixelUnit),
                                                                 game_math::Scale(up,shot.y*pixelUnit)));
        const auto direction=ConvergingShotDirection(origin,position_,forward,right,depth,shot.angle*180/std::numbers::pi_v<float>+90);
        const auto visual=shot.module==0 ? visuals.primary : IsMarisa() ? visuals.secondary[shot.script-1121] : visuals.secondary[0];
        // 原生矩形按较短边内接为三维球，宽高是全尺寸；视觉尺寸独立读取 ANM。
        if(auto* bullet=bullets.Spawn(BulletOwner::Player,origin,game_math::Scale(direction,shot.speed*20/12),
                                     std::min(shot.width,shot.height)*native_sizes::kWorldPerPixel*0.5F,Color{255,255,255,kShotAlpha},visual)) {
            bullet->visualScale=scale;bullet->damage=shot.damage;bullet->sourceShotRva=shot.sourceRva;
            if(shot.kind==1)bullet->homingTurnRate=5.5F; // 转向仍使用三维引导；原生转向规则未导出。
            if(shot.kind==2) {
                bullet->playerShotKind=PlayerShotKind::Missile;
                bullet->acceleration=game_math::Scale(forward,0.3F*60*20/12);
                bullet->impactVisual=visuals.impact[shot.script-1121];
                const float pixels=shot.script==1121 ? 32 : shot.script==1122 ? 42 : 48;
                bullet->explosionRadius=pixels*native_sizes::kWorldPerPixel*0.5F;
            }
        }
    }
}

bool Player::TryUseBomb() {
    if (bombs_ <= 0 || bombCooldown_ > 0.0F) return false;
    --bombs_;
    const auto& bomb=BombDefinition(shot_);
    // 原作同一时间只允许一场 Bomb，保护期覆盖完整演出及其后的恢复时间。
    bombCooldown_ = bomb.durationFrames/60.0F;
    invulnerableTimer_ = bomb.invulnerabilityFrames/60.0F;
    return true;
}

}  // namespace koumakyou
