// 本文件负责实现单颗弹幕生成、运动更新与敌弹清除。

#include "gameplay/arena.hpp"
#include "gameplay/bullets/system.hpp"

#include <algorithm>
#include <cmath>

namespace redmist {
namespace {

/// @brief 对两个三维向量逐分量相加。
/// @param left 左操作数。
/// @param right 右操作数。
/// @return 相加后的向量。
Vector3 Add(Vector3 left, Vector3 right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

/// @brief 将三维向量乘以标量。
/// @param vector 原向量。
/// @param scale 缩放系数。
/// @return 缩放后的向量。
Vector3 Scale(Vector3 vector, float scale) {
    return {vector.x * scale, vector.y * scale, vector.z * scale};
}

/// @brief 计算两个向量的点积。
/// @param left 第一个向量。
/// @param right 第二个向量。
/// @return 点积标量。
float Dot(Vector3 left, Vector3 right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

/// @brief 计算两个向量的叉积。
/// @param left 第一个向量。
/// @param right 第二个向量。
/// @return 右手系叉积向量。
Vector3 Cross(Vector3 left, Vector3 right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

/// @brief 将向量归一化，零向量退化为正 Z 方向。
/// @param vector 原向量。
/// @return 单位方向向量。
Vector3 Normalize(Vector3 vector) {
    const float length = BulletSystem::Length(vector);
    if (length < 0.00001F) {
        return {0.0F, 0.0F, 1.0F};
    }
    return Scale(vector, 1.0F / length);
}

}  // namespace

void BulletSystem::Reset() { bullets_.Clear(); }

void BulletSystem::Update(float deltaTime, std::span<const Vector3> targets) {
    bullets_.ForEachActive([deltaTime, targets](Bullet& bullet) {
        bullet.age += deltaTime;
        // 先更新速度，再按新速度推进位置；角速度使弹道在三维空间转弯。
        bullet.velocity = Add(bullet.velocity, Scale(bullet.acceleration, deltaTime));
        if (bullet.angularVelocity != 0.0F) {
            bullet.velocity = RotateAroundAxis(
                bullet.velocity, bullet.rotationAxis,
                bullet.angularVelocity * deltaTime);
        }
        if (bullet.homingTurnRate > 0.0F && !targets.empty()) {
            const Vector3* nearest = nullptr;
            float nearestDistanceSquared = 0.0F;
            for (const Vector3& target : targets) {
                const Vector3 difference{target.x - bullet.position.x,
                                          target.y - bullet.position.y,
                                          target.z - bullet.position.z};
                const float distanceSquared = Dot(difference, difference);
                if (!nearest || distanceSquared < nearestDistanceSquared) {
                    nearest = &target;
                    nearestDistanceSquared = distanceSquared;
                }
            }
            if (nearest && nearestDistanceSquared > 0.0001F) {
                const float speed = Length(bullet.velocity);
                if (speed > 0.0001F) {
                    const Vector3 current = Scale(bullet.velocity, 1.0F / speed);
                    const Vector3 desired = DirectionTo(bullet.position, *nearest);
                    const float angle = std::acos(std::clamp(Dot(current, desired),
                                                              -1.0F, 1.0F));
                    if (angle > 0.0001F) {
                        Vector3 axis = Cross(current, desired);
                        // 反向目标的叉积接近零，改用固定垂直轴避免转向停滞。
                        if (Length(axis) < 0.0001F) {
                            axis = Cross(current, {0.0F, 1.0F, 0.0F});
                            if (Length(axis) < 0.0001F)
                                axis = Cross(current, {1.0F, 0.0F, 0.0F});
                        }
                        const float turn = std::min(angle,
                            bullet.homingTurnRate * deltaTime);
                        bullet.velocity = Scale(RotateAroundAxis(current, axis,
                                                                   turn), speed);
                    }
                }
            }
        }
        bullet.position = Add(bullet.position, Scale(bullet.velocity, deltaTime));

        constexpr float margin = 3.0F;
        if (bullet.age > bullet.maxAge ||
            std::abs(bullet.position.x) > kArenaHalfExtent + margin ||
            std::abs(bullet.position.y) > kArenaHalfExtent + margin ||
            std::abs(bullet.position.z) > kArenaHalfExtent + margin) {
            bullet.active = false;
        }
    });
}

Bullet* BulletSystem::Spawn(BulletOwner owner, Vector3 position,
                            Vector3 velocity, float radius, Color color,
                            VisualId visual, Vector3 acceleration) {
    Bullet* bullet = bullets_.Acquire();
    if (bullet == nullptr) {
        return nullptr;
    }
    bullet->owner = owner;
    bullet->visual = visual;
    bullet->position = position;
    bullet->velocity = velocity;
    bullet->acceleration = acceleration;
    bullet->radius = radius;
    bullet->color = color;
    bullet->maxAge = owner == BulletOwner::Player ? 3.0F : 14.0F;
    return bullet;
}

void BulletSystem::ClearEnemyBullets() {
    bullets_.ForEachActive([](Bullet& bullet) {
        if (bullet.owner == BulletOwner::Enemy) {
            bullet.active = false;
        }
    });
}

Vector3 BulletSystem::DirectionFromYawPitch(float yaw, float pitch) {
    const float cosinePitch = std::cos(pitch);
    return Normalize({
        std::sin(yaw) * cosinePitch,
        std::sin(pitch),
        std::cos(yaw) * cosinePitch,
    });
}

Vector3 BulletSystem::DirectionTo(Vector3 from, Vector3 to) {
    return Normalize({to.x - from.x, to.y - from.y, to.z - from.z});
}

Vector3 BulletSystem::RotateAroundAxis(Vector3 vector, Vector3 axis, float angle) {
    axis = Normalize(axis);
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const Vector3 first = Scale(vector, cosine);
    const Vector3 second = Scale(Cross(axis, vector), sine);
    const Vector3 third = Scale(axis, Dot(axis, vector) * (1.0F - cosine));
    return Add(Add(first, second), third);
}

float BulletSystem::Length(Vector3 vector) {
    return std::sqrt(vector.x * vector.x + vector.y * vector.y +
                     vector.z * vector.z);
}

}  // namespace redmist
