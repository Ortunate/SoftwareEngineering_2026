#pragma once

#include "game/boss.hpp"
#include "game/bullet_system.hpp"
#include "game/enemy.hpp"

#include "raylib.h"

#include <filesystem>
#include <span>

namespace redmist {

// Textures are owned by Game; this struct only passes them to the renderer.
struct SpriteAssets {
    Texture2D blueBullet{};
    Texture2D redBullet{};
    Texture2D fairy{};
    Texture2D elite{};
    Texture2D boss{};
};

class SpriteRenderer {
public:
    SpriteRenderer() = default;
    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;
    ~SpriteRenderer();

    void Load(const std::filesystem::path& assetsPath);
    void Draw(const Camera3D& camera, const BulletSystem& bullets,
              std::span<const Enemy*> enemies, const RumiaBoss& boss,
              Vector3 playerPosition, const SpriteAssets& assets,
              bool debugHitboxes) const;

private:
    Shader bulletCoreShader_{};
    Shader cutoutShader_{};
};

}  // namespace redmist
