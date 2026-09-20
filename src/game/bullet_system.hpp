#pragma once

#include "core/fixed_pool.hpp"
#include "game/game_types.hpp"

#include <cstddef>

namespace redmist {

class BulletSystem {
public:
    static constexpr std::size_t kCapacity = 8192;

    void Reset();
    void Update(float deltaTime);

    Bullet* Spawn(BulletOwner owner, Vector3 position, Vector3 velocity,
                  float radius, Color color, Vector3 acceleration = {});

    // Emits concentric rings across a cone oriented along `axis`.
    void SpawnCone(BulletOwner owner, Vector3 origin, Vector3 axis,
                   int ringCount, int bulletsPerRing, float coneAngle,
                   float speed, float roll, Color color,
                   float radius = 0.16F);

    // Emits uniform directions over a complete sphere using a Fibonacci
    // lattice. This is the 3D replacement for a planar star burst.
    void SpawnSphereStar(BulletOwner owner, Vector3 origin, int count,
                         float speed, float rotation, Color color,
                         float radius = 0.16F);

    void SpawnAimedCone(Vector3 origin, Vector3 target, int ringCount,
                        int bulletsPerRing, float coneAngle, float speed,
                        float roll, Color color);

    void ClearEnemyBullets();
    FixedPool<Bullet, kCapacity>& Pool() { return bullets_; }
    const FixedPool<Bullet, kCapacity>& Pool() const { return bullets_; }

    static Vector3 DirectionFromYawPitch(float yaw, float pitch);
    static Vector3 DirectionTo(Vector3 from, Vector3 to);
    static Vector3 RotateAroundAxis(Vector3 vector, Vector3 axis, float angle);
    static float Length(Vector3 vector);

private:
    FixedPool<Bullet, kCapacity> bullets_;
};

}  // namespace redmist
