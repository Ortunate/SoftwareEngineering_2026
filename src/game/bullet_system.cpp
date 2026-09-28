#include "game/bullet_system.hpp"

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

void BulletSystem::Update(float deltaTime) {
    bullets_.ForEachActive([deltaTime](Bullet& bullet) {
        bullet.age += deltaTime;
        // 先更新速度，再按新速度推进位置；角速度使弹道在三维空间转弯。
        bullet.velocity = Add(bullet.velocity, Scale(bullet.acceleration, deltaTime));
        if (bullet.angularVelocity != 0.0F) {
            bullet.velocity = RotateAroundAxis(
                bullet.velocity, bullet.rotationAxis,
                bullet.angularVelocity * deltaTime);
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
                            BulletVisualKind visual, Vector3 acceleration) {
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
