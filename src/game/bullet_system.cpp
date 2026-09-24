#include "game/bullet_system.hpp"

#include <algorithm>
#include <cmath>

namespace redmist {
namespace {

Vector3 Add(Vector3 left, Vector3 right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vector3 Scale(Vector3 vector, float scale) {
    return {vector.x * scale, vector.y * scale, vector.z * scale};
}

float Dot(Vector3 left, Vector3 right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vector3 Cross(Vector3 left, Vector3 right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

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
                            Vector3 acceleration) {
    Bullet* bullet = bullets_.Acquire();
    if (bullet == nullptr) {
        return nullptr;
    }
    bullet->owner = owner;
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
