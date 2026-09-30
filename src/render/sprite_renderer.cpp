// 本文件负责实现敌人、Boss 和弹幕的公告板绘制、光晕及调试图层。

#include "render/sprite_renderer.hpp"

#include "rlgl.h"

#include <algorithm>
#include <cmath>

namespace redmist {
namespace {

/// @brief 单张精灵的贴图、世界高度与归一化锚点。
struct SpriteSpec {
    Texture2D texture;
    float worldHeight;
    Vector2 pivotUv;
};

/// @brief 计算两个三维向量的差。
/// @param first 被减向量。
/// @param second 减去的向量。
/// @return first - second。
Vector3 Subtract(Vector3 first, Vector3 second) {
    return {first.x - second.x, first.y - second.y,
            first.z - second.z};
}

/// @brief 计算叉积以构造朝向相机的正交基。
/// @param first 第一个向量。
/// @param second 第二个向量。
/// @return 叉积向量。
Vector3 Cross(Vector3 first, Vector3 second) {
    return {first.y * second.z - first.z * second.y,
            first.z * second.x - first.x * second.z,
            first.x * second.y - first.y * second.x};
}

/// @brief 计算两点的距离平方，避免绘制判断使用平方根。
/// @param first 第一个位置。
/// @param second 第二个位置。
/// @return 距离平方。
float DistanceSquared(Vector3 first, Vector3 second) {
    const Vector3 difference = Subtract(first, second);
    return difference.x * difference.x +
           difference.y * difference.y +
           difference.z * difference.z;
}

/// @brief 归一化朝向向量，零长度时退化为正 Y 方向。
/// @param vector 输入向量。
/// @return 单位方向向量。
Vector3 Normalized(Vector3 vector) {
    const float lengthSquared = DistanceSquared(vector, {});
    if (lengthSquared < 0.000001F) return {0.0F, 1.0F, 0.0F};
    const float scale = 1.0F / std::sqrt(lengthSquared);
    return {vector.x * scale, vector.y * scale, vector.z * scale};
}

/// @brief 根据相机朝向计算公告板贴图的屏幕上方向。
/// @param camera 当前相机。
/// @return 与视线垂直的单位上向量。
Vector3 ScreenUp(const Camera3D& camera) {
    // 从相机基向量求屏幕上方向，仰俯视时贴图仍完整朝向镜头。
    const Vector3 forward = Normalized(
        Subtract(camera.target, camera.position));
    const Vector3 right = Normalized(Cross(forward, camera.up));
    return Normalized(Cross(right, forward));
}

/// @brief 将一张贴图按世界尺寸和锚点放在指定位置。
/// @param camera 当前相机。
/// @param screenUp 贴图的屏幕上方向。
/// @param spec 贴图、大小和锚点参数。
/// @param position 锚点对应的世界坐标。
/// @param tint 颜色与透明度调制值。
void DrawSprite(const Camera3D& camera, Vector3 screenUp,
                const SpriteSpec& spec, Vector3 position, Color tint = WHITE) {
    if (!IsTextureValid(spec.texture)) return;
    const float width = spec.worldHeight *
                        static_cast<float>(spec.texture.width) /
                        static_cast<float>(spec.texture.height);
    const Vector2 size{width, spec.worldHeight};
    // 贴图锚点是判定中心在图片中的位置，而非图片外框的中心。
    const Vector2 origin{width * spec.pivotUv.x,
                         spec.worldHeight * (1.0F - spec.pivotUv.y)};
    DrawBillboardPro(camera, spec.texture,
                     {0.0F, 0.0F, static_cast<float>(spec.texture.width),
                      static_cast<float>(spec.texture.height)},
                     position, screenUp, size, origin, 0.0F, tint);
}

/// @brief 将 Stage 外观参数转为公告板绘制参数。
SpriteSpec Spec(const VisualResource& visual, float height) {
    return {visual.texture, height, visual.pivot};
}

const VisualResource& FindVisual(std::span<const VisualResource> visuals,
                                 VisualId id) {
    static const VisualResource empty{};
    return id < visuals.size() ? visuals[id] : empty;
}

}  // namespace

void SpriteRenderer::Draw(const Camera3D& camera, const BulletSystem& bullets,
                          std::span<const Enemy*> enemies, const Boss* boss,
                          std::span<const VisualResource> visuals,
                          const SpriteShaders& shaders) const {
    const Vector3 screenUp = ScreenUp(camera);
    bulletGroups_.resize(visuals.size());
    for (auto& group : bulletGroups_) group.clear();
    bullets.Pool().ForEachActive([&](const Bullet& bullet) {
        if (bullet.visual < bulletGroups_.size())
            bulletGroups_[bullet.visual].push_back(&bullet);
    });

    // 球体先写入深度；公告板按照外观分组以合并贴图调用。
    for (std::size_t id = 0; id < visuals.size(); ++id) {
        if (visuals[id].mode != VisualMode::Sphere) continue;
        for (const Bullet* bullet : bulletGroups_[id])
            DrawSphere(bullet->position, bullet->radius, bullet->color);
    }
    const bool hasBulletShader = IsShaderValid(shaders.bulletCore) &&
                                 shaders.bulletCore.id != rlGetShaderIdDefault();
    const bool hasCutoutShader = IsShaderValid(shaders.cutout) &&
                                 shaders.cutout.id != rlGetShaderIdDefault();
    if (hasBulletShader) BeginShaderMode(shaders.bulletCore);
    for (std::size_t id = 0; id < visuals.size(); ++id) {
        const VisualResource& visual = visuals[id];
        if (visual.mode != VisualMode::Bullet ||
            !IsTextureValid(visual.texture)) continue;
        for (const Bullet* bullet : bulletGroups_[id]) {
            if (DistanceSquared(bullet->position, camera.position) < 1.1F * 1.1F)
                continue;
            DrawSprite(camera, screenUp,
                       Spec(visual, bullet->radius * visual.bulletDiameterScale),
                       bullet->position);
        }
    }
    if (hasBulletShader) EndShaderMode();

    if (hasCutoutShader) BeginShaderMode(shaders.cutout);
    for (const Enemy* enemy : enemies) {
        const VisualResource& visual = FindVisual(visuals, enemy->visual);
        if (IsTextureValid(visual.texture))
            DrawSprite(camera, screenUp, Spec(visual, visual.worldHeight),
                       enemy->position);
        else DrawSphere(enemy->position, enemy->hitRadius, SKYBLUE);
    }
    if (boss && boss->active) {
        const VisualResource& visual = FindVisual(visuals, boss->visual);
        if (IsTextureValid(visual.texture))
            DrawSprite(camera, screenUp, Spec(visual, visual.worldHeight),
                       boss->position);
        else DrawSphere(boss->position, boss->hitRadius, RED);
    }
    if (hasCutoutShader) EndShaderMode();

    // 光晕只做视觉叠加，不写深度以免遮挡后续透明层；每种外观最多绘制 2048 个近景光晕。
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    constexpr float glowDistanceSquared = 8.0F * 8.0F;
    for (std::size_t id = 0; id < visuals.size(); ++id) {
        const VisualResource& visual = visuals[id];
        if (visual.mode != VisualMode::Bullet || !visual.glow ||
            !IsTextureValid(visual.texture)) continue;
        int glowCount = 0;
        for (const Bullet* bullet : bulletGroups_[id]) {
            if (glowCount >= 2048) break;
            const float distanceSquared =
                DistanceSquared(bullet->position, camera.position);
            if (distanceSquared > glowDistanceSquared) continue;
            ++glowCount;
            const float distance = std::sqrt(distanceSquared);
            const float nearFade = std::clamp((distance - 0.35F) / 0.75F,
                                              0.0F, 1.0F);
            const float sizeScale = distance < 1.1F ? 2.6F : 3.15F;
            const unsigned char opacity = static_cast<unsigned char>(
                36.0F + 28.0F * nearFade);
            DrawSprite(camera, screenUp,
                       Spec(visual, bullet->radius * sizeScale),
                       bullet->position, Color{255, 255, 255, opacity});
        }
    }
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

#ifdef RED_MIST_DEBUG_FEATURES
void SpriteRenderer::DrawDebugGeometry(const BulletSystem& bullets,
                                       std::span<const Enemy*> enemies,
                                       const Boss* boss,
                                       Vector3 playerPosition) const {
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
    if (boss && boss->active) {
        DrawSphereWires(boss->position, boss->hitRadius, 12, 16,
                        Color{65, 240, 250, 240});
        const Vector3 muzzle = boss->MuzzlePosition(playerPosition);
        DrawLine3D(boss->position, muzzle, YELLOW);
        DrawSphere(muzzle, 0.09F, MAGENTA);
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    rlEnableDepthTest();
}
#endif

}  // namespace redmist
