#include "game/sprite_renderer.hpp"

#include "rlgl.h"

#include <algorithm>
#include <cmath>

namespace redmist {
namespace {

struct SpriteSpec {
    Texture2D texture;
    float worldHeight;
    Vector2 pivotUv;
};

Vector3 Subtract(Vector3 first, Vector3 second) {
    return {first.x - second.x, first.y - second.y,
            first.z - second.z};
}

Vector3 Cross(Vector3 first, Vector3 second) {
    return {first.y * second.z - first.z * second.y,
            first.z * second.x - first.x * second.z,
            first.x * second.y - first.y * second.x};
}

float DistanceSquared(Vector3 first, Vector3 second) {
    const Vector3 difference = Subtract(first, second);
    return difference.x * difference.x +
           difference.y * difference.y +
           difference.z * difference.z;
}

Vector3 Normalized(Vector3 vector) {
    const float lengthSquared = DistanceSquared(vector, {});
    if (lengthSquared < 0.000001F) return {0.0F, 1.0F, 0.0F};
    const float scale = 1.0F / std::sqrt(lengthSquared);
    return {vector.x * scale, vector.y * scale, vector.z * scale};
}

Vector3 ScreenUp(const Camera3D& camera) {
    const Vector3 forward = Normalized(
        Subtract(camera.target, camera.position));
    const Vector3 right = Normalized(Cross(forward, camera.up));
    return Normalized(Cross(right, forward));
}

void DrawSprite(const Camera3D& camera, Vector3 screenUp,
                const SpriteSpec& spec, Vector3 position, Color tint = WHITE) {
    if (!IsTextureValid(spec.texture)) return;
    const float width = spec.worldHeight *
                        static_cast<float>(spec.texture.width) /
                        static_cast<float>(spec.texture.height);
    const Vector2 size{width, spec.worldHeight};
    const Vector2 origin{width * spec.pivotUv.x,
                         spec.worldHeight * (1.0F - spec.pivotUv.y)};
    DrawBillboardPro(camera, spec.texture,
                     {0.0F, 0.0F, static_cast<float>(spec.texture.width),
                      static_cast<float>(spec.texture.height)},
                     position, screenUp, size, origin, 0.0F, tint);
}

SpriteSpec EnemySpec(const Enemy& enemy, const SpriteAssets& assets) {
    switch (enemy.kind) {
        case EnemyKind::Fairy:
            return {assets.fairy, 1.8F, {0.51F, 0.40F}};
        case EnemyKind::DarkFairy:
            return {assets.elite, 1.8F, {0.50F, 0.35F}};
        case EnemyKind::Orb:
            return {assets.blueBullet, enemy.hitRadius * 2.4F,
                    {0.5F, 0.5F}};
        case EnemyKind::Elite:
            return {assets.elite, 3.06F, {0.50F, 0.35F}};
    }
    return {};
}

void DrawBulletKind(const Camera3D& camera, Vector3 screenUp,
                    const BulletSystem& bullets, BulletVisualKind visual,
                    Texture2D texture, float diameterScale) {
    if (!IsTextureValid(texture)) return;
    bullets.Pool().ForEachActive([&](const Bullet& bullet) {
        if (bullet.visual != visual) return;
        if (DistanceSquared(bullet.position, camera.position) <
            1.1F * 1.1F) return;
        DrawSprite(camera, screenUp,
                   {texture, bullet.radius * diameterScale, {0.5F, 0.5F}},
                   bullet.position);
    });
}

void DrawDebugGeometry(const BulletSystem& bullets,
                       std::span<const Enemy*> enemies, const RumiaBoss& boss,
                       Vector3 playerPosition) {
    rlDisableDepthTest();
    rlDisableDepthMask();
    int shownBullets = 0;
    bullets.Pool().ForEachActive([&](const Bullet& bullet) {
        if (shownBullets++ >= 256) return;
        DrawSphereWires(bullet.position, bullet.radius, 7, 10,
                        Color{255, 230, 90, 230});
    });
    for (const Enemy* enemy : enemies) {
        DrawSphereWires(enemy->position, enemy->hitRadius, 9, 12,
                        Color{65, 240, 250, 240});
        const Vector3 muzzle = enemy->MuzzlePosition(playerPosition);
        DrawLine3D(enemy->position, muzzle, YELLOW);
        DrawSphere(muzzle, 0.065F, MAGENTA);
    }
    if (boss.active) {
        DrawSphereWires(boss.position, boss.hitRadius, 12, 16,
                        Color{65, 240, 250, 240});
        const Vector3 muzzle = boss.MuzzlePosition(playerPosition);
        DrawLine3D(boss.position, muzzle, YELLOW);
        DrawSphere(muzzle, 0.09F, MAGENTA);
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    rlEnableDepthTest();
}

}  // namespace

SpriteRenderer::~SpriteRenderer() {
    if (bulletCoreShader_.id != 0 &&
        bulletCoreShader_.id != rlGetShaderIdDefault()) {
        UnloadShader(bulletCoreShader_);
    }
    if (cutoutShader_.id != 0 &&
        cutoutShader_.id != rlGetShaderIdDefault()) {
        UnloadShader(cutoutShader_);
    }
}

void SpriteRenderer::Load(const std::filesystem::path& assetsPath) {
    const std::filesystem::path bulletFragment =
        assetsPath / "shaders" / "bullet_core.fs";
    if (FileExists(bulletFragment.string().c_str())) {
        bulletCoreShader_ = LoadShader(nullptr,
                                       bulletFragment.string().c_str());
    }
    const std::filesystem::path fragment =
        assetsPath / "shaders" / "sprite_cutout.fs";
    if (FileExists(fragment.string().c_str())) {
        cutoutShader_ = LoadShader(nullptr, fragment.string().c_str());
    }
}

void SpriteRenderer::Draw(const Camera3D& camera, const BulletSystem& bullets,
                          std::span<const Enemy*> enemies,
                          const RumiaBoss& boss, Vector3 playerPosition,
                          const SpriteAssets& assets, bool debugHitboxes) const {
    const Vector3 screenUp = ScreenUp(camera);

    // Solid bullets write depth like ordinary 3D geometry.
    bullets.Pool().ForEachActive([](const Bullet& bullet) {
        if (bullet.visual == BulletVisualKind::SolidSphere) {
            DrawSphere(bullet.position, bullet.radius, bullet.color);
        }
    });

    const bool hasBulletShader = IsShaderValid(bulletCoreShader_) &&
                                 bulletCoreShader_.id != rlGetShaderIdDefault();
    const bool hasCutoutShader = IsShaderValid(cutoutShader_) &&
                                 cutoutShader_.id != rlGetShaderIdDefault();
    if (hasBulletShader) BeginShaderMode(bulletCoreShader_);
    // Group by texture so raylib can batch neighbouring quads.
    DrawBulletKind(camera, screenUp, bullets, BulletVisualKind::RedOrb,
                   assets.redBullet, 2.6F);
    DrawBulletKind(camera, screenUp, bullets, BulletVisualKind::BluePearl,
                   assets.blueBullet, 2.6F);
    if (hasBulletShader) EndShaderMode();

    if (hasCutoutShader) BeginShaderMode(cutoutShader_);
    for (const Enemy* enemy : enemies) {
        const SpriteSpec spec = EnemySpec(*enemy, assets);
        if (IsTextureValid(spec.texture)) {
            DrawSprite(camera, screenUp, spec, enemy->position);
        } else {
            DrawSphere(enemy->position, enemy->hitRadius, SKYBLUE);
        }
    }
    if (boss.active) {
        if (IsTextureValid(assets.boss)) {
            DrawSprite(camera, screenUp,
                       {assets.boss, 3.2F, {0.50F, 0.36F}},
                       boss.position);
        } else {
            DrawSphere(boss.position, boss.hitRadius, RED);
        }
    }
    if (hasCutoutShader) EndShaderMode();

    // Glow contributes light without placing transparent texels in the depth map.
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    constexpr float glowDistanceSquared = 8.0F * 8.0F;
    for (const BulletVisualKind visual : {BulletVisualKind::RedOrb,
                                          BulletVisualKind::BluePearl}) {
        const Texture2D texture = visual == BulletVisualKind::RedOrb
                                      ? assets.redBullet : assets.blueBullet;
        if (!IsTextureValid(texture)) continue;
        int glowCount = 0;
        bullets.Pool().ForEachActive([&](const Bullet& bullet) {
            if (bullet.visual != visual || glowCount >= 2048) return;
            const float distanceSquared =
                DistanceSquared(bullet.position, camera.position);
            if (distanceSquared > glowDistanceSquared) return;
            ++glowCount;
            const float distance = std::sqrt(distanceSquared);
            const float nearFade = std::clamp((distance - 0.35F) / 0.75F,
                                              0.0F, 1.0F);
            const float sizeScale = distance < 1.1F ? 2.6F : 3.15F;
            const unsigned char opacity = static_cast<unsigned char>(
                36.0F + 28.0F * nearFade);
            DrawSprite(camera, screenUp,
                       {texture, bullet.radius * sizeScale, {0.5F, 0.5F}},
                       bullet.position, Color{255, 255, 255, opacity});
        });
    }
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();

    if (debugHitboxes) {
        DrawDebugGeometry(bullets, enemies, boss, playerPosition);
    }
}

}  // namespace redmist
