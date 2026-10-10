// 本文件负责实现第一面各类敌人的移动轨迹、技能计时和弹幕释放。

#include "stages/stage1/enemies.hpp"

#include "gameplay/arena.hpp"
#include "gameplay/bullets/patterns.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace koumakyou::stage1 {
namespace {
constexpr Color kEnemyRed{255, 70, 105, 255};
constexpr Color kEnemyBlue{75, 145, 255, 255};
// kArenaHalfExtents 是半长，前进距离必须取 Z 全长的五分之一，当前为 4.6 世界单位。
constexpr float kDarkFairyTravelDistance = kArenaHalfExtents.z * 2.0F / 5.0F;
constexpr float kDarkFairyMoveSpeed = 1.7F;
constexpr float kDarkFairyApproachTime = kDarkFairyTravelDistance / kDarkFairyMoveSpeed;
// 权重仅定义同类敌人死亡时的两种点数量；生成位置和拾取由 DropSystem 统一处理。
constexpr std::array<DropOutcome, 2> kFairyDrops{{
    {1, 0, 3}, {0, 1, 1},
}};
constexpr std::array<DropOutcome, 3> kDarkFairyDrops{{
    {1, 1, 2}, {2, 0, 1}, {0, 2, 1},
}};
constexpr std::array<DropOutcome, 2> kOrbDrops{{
    {8, 2, 3}, {4, 4, 1},
}};

/// @brief 仅封装倒计时运算；每种技能的计时器由具体敌人自己持有。
/// @param timer 技能剩余秒数，触发后重置为 interval。
/// @param interval 两次触发之间的秒数。
/// @param deltaTime 本帧经过的秒数。
/// @param age 从入场起算的秒数；初始保护期内不发弹。
/// @return 保护期已过且计时器到期时返回 true。
bool SkillReady(float& timer, float interval, float deltaTime, float age) {
    timer -= deltaTime;
    if (age <= 0.55F || timer > 0.0F) return false;
    timer = interval;
    return true;
}
}  // namespace

FairyEnemy::FairyEnemy(const FairyParams& params)
    : Enemy(kFairy, params.position, kFairyDrops),
      movement_(params.movement) {
    // 当前玩家追踪弹单发伤害最低为 1；保持血量为 1，使两种玩家弹都能一发击破。
    health = maxHealth = 1.0F;
    scoreValue = 500;
}

void FairyEnemy::Move(float) {
    constexpr float speed = 1.5F;
    switch (movement_) {
        case Movement::Straight:
            position = {origin.x, origin.y, origin.z - speed * age};
            break;
        case Movement::Helix: {
            const float angle = age * 2.1F + origin.x * 0.3F;
            position = {origin.x + std::cos(angle) * 1.25F,
                        origin.y + std::sin(angle) * 1.25F,
                        origin.z - speed * age};
            break;
        }
        case Movement::CrossFromLeft:
            position = {origin.x + speed * 0.9F * age,
                        origin.y + std::sin(age * 1.7F) * 0.7F,
                        origin.z - speed * 0.5F * age};
            break;
        case Movement::CrossFromRight:
            position = {origin.x - speed * 0.9F * age,
                        origin.y + std::cos(age * 1.7F) * 0.7F,
                        origin.z - speed * 0.5F * age};
            break;
    }
}

void FairyEnemy::UpdateSkills(float, Vector3, BulletSystem&) {}

DarkFairyEnemy::DarkFairyEnemy(const DarkFairyParams& params)
    : Enemy(kDarkFairy, params.position, kDarkFairyDrops),
      retreatDelay_(params.retreatDelay) {
    health = maxHealth = 12.0F;
    scoreValue = 650;
}

void DarkFairyEnemy::Move(float) {
    // 用出生年龄分段求位置，避免一帧跨过转折或返回时丢失剩余时间、越过目标位置。
    float distance = std::min(age * kDarkFairyMoveSpeed, kDarkFairyTravelDistance);
    const float retreatAge = age - kDarkFairyApproachTime - retreatDelay_;
    if (retreatAge > 0.0F) {
        distance = std::max(0.0F, kDarkFairyTravelDistance - retreatAge * kDarkFairyMoveSpeed);
    }
    position = {origin.x, origin.y, origin.z - distance};
    // 返回终点已知，预先淡出而不改变原路返回或战斗回收时刻。
    presentation.PrepareExit(kDarkFairyApproachTime * 2.0F + retreatDelay_ - age);
    if (age >= kDarkFairyApproachTime * 2.0F + retreatDelay_) {
        position = origin;
        active = false;
    }
}

void DarkFairyEnemy::UpdateSkills(float, Vector3, BulletSystem& bullets) {
    if (!hasFired_ && age >= kDarkFairyApproachTime) {
        hasFired_ = true;
        FireRadialBurst(bullets);
    }
}

void DarkFairyEnemy::FireRadialBurst(BulletSystem& bullets) {
    // Move 先于技能执行，低帧率下 position 可能已返回；发弹原点始终取前进终点。
    const Vector3 center{origin.x, origin.y, origin.z - kDarkFairyTravelDistance};
    const auto emit = [&bullets, center](Vector3 direction) {
        bullets.Spawn(BulletOwner::Enemy, center, pattern::Scale(direction, 3.5F),
                      0.17F, kEnemyRed, kRedBullet);
    };
    // 保留参考图 X/Y 平面约 45° 的八向疏散环，再沿 Z 轴补前后纬圈各 4 向及两极。
    // 纬圈错开半个环间隔，保证三维相邻方向最小夹角约 45°，每个方向只发一颗。
    constexpr float pi = std::numbers::pi_v<float>;
    for (int index = 0; index < 8; ++index) {
        const float azimuth = index * pi / 4.0F;
        emit({std::cos(azimuth), std::sin(azimuth), 0.0F});
    }
    const float diagonal = std::sqrt(0.5F);
    for (float side : {-1.0F, 1.0F}) {
        for (int index = 0; index < 4; ++index) {
            const float azimuth = pi / 8.0F + index * pi / 2.0F;
            emit({diagonal * std::cos(azimuth), diagonal * std::sin(azimuth),
                  side * diagonal});
        }
        emit({0.0F, 0.0F, side});
    }
}

OrbEnemy::OrbEnemy(const OrbParams& params)
    : Enemy(kOrb, params.position, kOrbDrops),
      movement_(params.movement), shotTimer_(0.8F) {
    health = maxHealth = 18.0F;
    scoreValue = 900;
    hitRadius = 0.62F;
}

void OrbEnemy::Move(float) {
    constexpr float speed = 1.15F;
    switch (movement_) {
        case Movement::FloatForward:
            position = {origin.x, origin.y + std::sin(age * 1.6F) * 0.45F,
                        origin.z - speed * age};
            break;
        case Movement::OrbitForward:
            position = {origin.x + std::cos(age * 1.3F) * 0.8F,
                        origin.y + std::sin(age * 1.3F) * 0.8F,
                        origin.z - speed * age};
            break;
        case Movement::CrossArena: {
            const float side = origin.x < 0.0F ? 1.0F : -1.0F;
            position = {origin.x + side * speed * age,
                        origin.y + std::sin(age * 1.6F) * 0.45F,
                        origin.z - speed * 0.45F * age};
            break;
        }
    }
}

void OrbEnemy::UpdateSkills(float deltaTime, Vector3 playerPosition,
                            BulletSystem& bullets) {
    if (SkillReady(shotTimer_, 1.5F, deltaTime, age))
        FireCone(bullets, playerPosition);
}

void OrbEnemy::FireCone(BulletSystem& bullets, Vector3 playerPosition) {
    const Vector3 muzzle = MuzzlePosition(playerPosition);
    const Vector3 axis = BulletSystem::DirectionTo(muzzle, playerPosition);
    pattern::ForEachConeDirection(axis, 1, 5, 0.24F, age * 0.2F,
        [muzzle, &bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, muzzle,
                          pattern::Scale(direction, 3.0F), 0.16F, kEnemyBlue,
                          kBlueBullet);
        });
}

ReferenceEnemy::ReferenceEnemy(const ReferenceEnemyParams& params)
    : Enemy(params.sub < 3 ? kFairy : kDarkFairy, params.position, {}), sourceItem_(params.sourceItem) {
    Vector3 source = ReferenceSourcePosition(position);
    if (params.randomX) source.x = static_cast<float>((params.seed * 1664525U + 1013904223U) >> 8) / 16777216.0F * 384;
    script_.Reset(params.sub,source,params.sourceHealth,params.seed,params.mirrored);
    position = origin = script_.Position();
    health = maxHealth = params.sourceHealth/kReferenceHealthUnit;
    hitRadius = 14*kReferenceScale;
    scoreValue = params.sourceScore;
}
void ReferenceEnemy::Update(float deltaTime, Vector3 playerPosition, BulletSystem& bullets) {
    age += deltaTime; presentation.Update(deltaTime);
    script_.health = health*kReferenceHealthUnit;
    script_.Update(deltaTime,playerPosition,bullets);
    position = script_.Position();
    const Vector3 p = script_.sourcePosition;
    // 出生在 y=-32 的场外缓冲带；只有实际入场后再越界才回收，不能在第一帧销毁。
    if (p.x >= -16 && p.x <= 400 && p.y >= -16 && p.y <= 464) entered_ = true;
    if (script_.finished || (entered_ && (p.x < -16 || p.x > 400 || p.y < -16 || p.y > 464))) active = false;
}
}  // namespace koumakyou::stage1
