#pragma once

#include "raylib.h"

#include <cstdint>

namespace redmist {

constexpr float kArenaHalfExtent = 10.0F;
constexpr float kArenaPadding = 0.35F;

enum class BulletOwner : std::uint8_t { Player, Enemy };
enum class BulletVisualKind : std::uint8_t { BluePearl, RedOrb, SolidSphere };

struct Bullet {
    bool active = false;
    bool grazed = false;
    BulletOwner owner = BulletOwner::Enemy;
    BulletVisualKind visual = BulletVisualKind::SolidSphere;
    Vector3 position{};
    Vector3 velocity{};
    Vector3 acceleration{};
    Vector3 rotationAxis{0.0F, 1.0F, 0.0F};
    float radius = 0.16F;
    float age = 0.0F;
    float maxAge = 12.0F;
    float angularVelocity = 0.0F;
    Color color = WHITE;
};

enum class EnemyKind : std::uint8_t { Fairy, DarkFairy, Orb, Elite };
enum class PathKind : std::uint8_t { Straight, Helix, SweepLeft, SweepRight };

struct Player {
    // Camera position and collision center are always identical.
    Vector3 position{0.0F, 0.0F, -6.5F};
    float yaw = 0.0F;
    float pitch = 0.0F;
    float speed = 6.5F;
    float focusSpeed = 2.8F;
    float mouseSensitivity = 0.0024F;
    float hitRadius = 0.18F;
    float grazeRadius = 0.72F;
    float fireTimer = 0.0F;
    float invulnerableTimer = 1.5F;
    float bombCooldown = 0.0F;
    int lives = 3;
    int bombs = 3;
    int power = 1;
};

struct KeyBindings {
    int forward = KEY_W;
    int backward = KEY_S;
    int moveLeft = KEY_A;
    int moveRight = KEY_D;
    int ascend = KEY_SPACE;
    int descend = KEY_LEFT_SHIFT;
    int focus = KEY_C;
    int fire = KEY_Z;
    int bomb = KEY_X;
    int pause = KEY_P;
};

enum class RunState : std::uint8_t {
    MainMenu,
    Options,
    Playing,
    Paused,
    Won,
    GameOver
};

}  // namespace redmist
