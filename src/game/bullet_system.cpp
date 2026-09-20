#include "game/bullet_system.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

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

void BulletSystem::SpawnCone(BulletOwner owner, Vector3 origin, Vector3 axis,
                             int ringCount, int bulletsPerRing,
                             float coneAngle, float speed, float roll,
                             Color color, float radius) {
    if (ringCount <= 0 || bulletsPerRing <= 0) {
        return;
    }

    const Vector3 forward = Normalize(axis);
    const Vector3 reference = std::abs(forward.y) < 0.9F
                                  ? Vector3{0.0F, 1.0F, 0.0F}
                                  : Vector3{1.0F, 0.0F, 0.0F};
    const Vector3 right = Normalize(Cross(reference, forward));
    const Vector3 up = Normalize(Cross(forward, right));

    Spawn(owner, origin, Scale(forward, speed), radius, color);
    for (int ring = 1; ring <= ringCount; ++ring) {
        const float ringAngle = coneAngle * static_cast<float>(ring) /
                                static_cast<float>(ringCount);
        const float axial = std::cos(ringAngle);
        const float radial = std::sin(ringAngle);
        for (int index = 0; index < bulletsPerRing; ++index) {
            const float azimuth = roll + 2.0F * std::numbers::pi_v<float> *
                                             static_cast<float>(index) /
                                             static_cast<float>(bulletsPerRing) +
                                  static_cast<float>(ring) * 0.31F;
            Vector3 direction = Scale(forward, axial);
            direction = Add(direction, Scale(right, radial * std::cos(azimuth)));
            direction = Add(direction, Scale(up, radial * std::sin(azimuth)));
            Spawn(owner, origin, Scale(Normalize(direction), speed), radius, color);
        }
    }
}

void BulletSystem::SpawnSphereStar(BulletOwner owner, Vector3 origin, int count,
                                   float speed, float rotation, Color color,
                                   float radius) {
    if (count <= 0) {
        return;
    }
    constexpr float goldenAngle = 2.39996322973F;
    for (int index = 0; index < count; ++index) {
        const float t = (static_cast<float>(index) + 0.5F) /
                        static_cast<float>(count);
        const float y = 1.0F - 2.0F * t;
        const float planarRadius = std::sqrt(std::max(0.0F, 1.0F - y * y));
        const float azimuth = goldenAngle * static_cast<float>(index) + rotation;
        const Vector3 direction{
            planarRadius * std::cos(azimuth),
            y,
            planarRadius * std::sin(azimuth),
        };
        Spawn(owner, origin, Scale(direction, speed), radius, color);
    }
}

void BulletSystem::SpawnAimedCone(Vector3 origin, Vector3 target, int ringCount,
                                  int bulletsPerRing, float coneAngle,
                                  float speed, float roll, Color color) {
    SpawnCone(BulletOwner::Enemy, origin, DirectionTo(origin, target), ringCount,
              bulletsPerRing, coneAngle, speed, roll, color);
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
