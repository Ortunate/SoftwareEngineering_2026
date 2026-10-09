// 本文件负责实现敌人、Boss、弹幕与道具的公告板绘制、光晕及调试图层。

#include "render/sprite_renderer.hpp"

#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <numbers>

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

/// @brief 用相机平面中的实心圆环提供入退场光效；不依赖额外纹理。
void DrawActorRing(const Camera3D& camera, Vector3 up, Vector3 position,
                   float radius, Color color) {
    const Vector3 right = Normalized(Cross(
        Normalized(Subtract(camera.target, camera.position)), up));
    const auto point = [&](float angle, float size) {
        const float x = std::cos(angle) * size;
        const float y = std::sin(angle) * size;
        return Vector3{position.x + right.x * x + up.x * y,
                       position.y + right.y * x + up.y * y,
                       position.z + right.z * x + up.z * y};
    };
    constexpr int segments = 48;
    for (int i = 0; i < segments; ++i) {
        const float a = 2.0F * std::numbers::pi_v<float> * i / segments;
        const float b = 2.0F * std::numbers::pi_v<float> * (i + 1) / segments;
        const Vector3 outerA = point(a, radius), outerB = point(b, radius);
        const Vector3 innerA = point(a, radius * 0.92F), innerB = point(b, radius * 0.92F);
        DrawTriangle3D(outerA, outerB, innerA, color);
        DrawTriangle3D(innerA, outerB, innerB, color);
    }
}

/// @brief 在三维球面分布爆散粒子，远近观察均有体积；progress 为 [0,1]。
void DrawActorBurst(Vector3 position, float radius, float progress, Color color,
                    ActorKind kind) {
    const int count = kind == ActorKind::Boss ? 24 : 12;
    for (int i = 0; i < count; ++i) {
        const float y = 1.0F - 2.0F * (i + 0.5F) / count;
        const float horizontal = std::sqrt(1.0F - y * y);
        const float angle = i * 2.39996323F;
        const float distance = radius * (0.4F + progress * 4.0F);
        // 远景粒子只占几像素，低细分避免整池击破时放大几何开销。
        DrawSphereEx({position.x + horizontal * std::cos(angle) * distance,
                      position.y + y * distance,
                      position.z + horizontal * std::sin(angle) * distance},
                     radius * 0.10F * (1.0F - progress), 4, 6, color);
    }
    // 很短的中心闪光衔接受击，之后只剩扩散的环与粒子。
    if (progress < 0.2F)
        DrawSphereEx(position, radius * 0.6F * (1.0F - progress / 0.2F), 8, 12, color);
}

}  // namespace

void SpriteRenderer::Draw(const Camera3D& camera, const BulletSystem& bullets,
                          std::span<const Enemy*> enemies, const Boss* boss,
                          std::span<const PickupSprite> pickups,
                          std::span<const VisualResource> visuals,
                          const SpriteShaders& shaders,
                          const ActorEffectSystem& effects) const {
    const Vector3 screenUp = ScreenUp(camera);
    bulletGroups_.resize(visuals.size());
    for (auto& group : bulletGroups_) group.clear();
    translucentPlayerBullets_.clear();
    translucentActors_.clear();
    bullets.Pool().ForEachActive([&](const Bullet& bullet) {
        if (bullet.beamLength > 0) return;
        if (bullet.visual < bulletGroups_.size()) {
            bulletGroups_[bullet.visual].push_back(&bullet);
            if (bullet.owner == BulletOwner::Player &&
                visuals[bullet.visual].mode == VisualMode::Bullet)
                translucentPlayerBullets_.push_back(&bullet);
        }
    });

    // 光束亮芯和判定共用轴线；原作预警最后 30 帧开始展开，但命中仍等待 hitboxStartTime。
    bullets.Pool().ForEachActive([](const Bullet& bullet) {
        if (bullet.beamLength <= 0) return;
        const auto end = bullet.BeamEnd();
        const float width=bullet.radius*bullet.BeamVisualScale();
        if (!bullet.Dangerous() && bullet.beamGrowSeconds<=0) {
            DrawLine3D(bullet.position, end, Fade(bullet.color, 0.7F));
            DrawCylinderWiresEx(bullet.position, end, bullet.radius * 0.3F,
                                bullet.radius * 0.3F, 6, Fade(bullet.color, 0.4F));
        } else {
            DrawCylinderEx(bullet.position, end, width * 0.45F,
                           width * 0.45F, 10, WHITE);
        }
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
            if (bullet->owner == BulletOwner::Player) continue;
            if (DistanceSquared(bullet->position, camera.position) < 1.1F * 1.1F)
                continue;
            DrawSprite(camera, screenUp,
                       Spec(visual, bullet->radius * visual.bulletDiameterScale),
                       bullet->position);
        }
    }
    if (hasBulletShader) EndShaderMode();

    if (hasCutoutShader) BeginShaderMode(shaders.cutout);
    const auto drawActor = [&](Vector3 position, VisualId id, float hitRadius,
                               ActorKind kind, ActorAppearance appearance) {
        // 第一人称穿过近处角色时逐渐淡去贴图，避免巨幅裙摆遮住整屏弹幕；判定不变。
        const float nearOpacity = std::clamp((std::sqrt(DistanceSquared(position, camera.position)) - 0.6F) / 1.2F, 0.0F, 1.0F);
        appearance.alpha *= nearOpacity;
        if (appearance.alpha < 0.999F || appearance.ringAlpha > 0.001F) {
            translucentActors_.push_back({position, id, hitRadius, kind, appearance});
            return;
        }
        const VisualResource& visual = FindVisual(visuals, id);
        if (IsTextureValid(visual.texture))
            DrawSprite(camera, screenUp, Spec(visual, visual.worldHeight), position);
        else DrawSphere(position, hitRadius, kind == ActorKind::Boss ? RED : SKYBLUE);
    };
    for (const Enemy* enemy : enemies) {
        drawActor(enemy->position, enemy->visual, enemy->hitRadius,
                  ActorKind::Enemy, enemy->presentation.Sample());
    }
    if (boss && boss->active) {
        drawActor(boss->position, boss->visual, boss->hitRadius,
                  ActorKind::Boss, boss->presentation.Sample());
    }
    for (const PickupSprite& pickup : pickups) {
        const VisualResource& visual = FindVisual(visuals, pickup.visual);
        const float height = visual.worldHeight * pickup.heightScale;
        if (IsTextureValid(visual.texture))
            DrawSprite(camera, screenUp, Spec(visual, height), pickup.position);
        else DrawSphere(pickup.position, height * 0.35F, RED);
    }
    if (hasCutoutShader) EndShaderMode();
    effects.Pool().ForEachActive([&](const ActorExitEffect& effect) {
        translucentActors_.push_back({effect.position, effect.visual, effect.hitRadius,
                                      effect.kind, effect.Sample()});
    });

    // 玩家符卡与过渡角色保留连续透明度；分别排序后合并为同一远近序列。
    std::sort(translucentPlayerBullets_.begin(), translucentPlayerBullets_.end(),
              [&camera](const Bullet* left, const Bullet* right) {
                  return DistanceSquared(left->position, camera.position) >
                         DistanceSquared(right->position, camera.position);
              });
    std::sort(translucentActors_.begin(), translucentActors_.end(),
              [&camera](const ActorSprite& left, const ActorSprite& right) {
                  return DistanceSquared(left.position, camera.position) >
                         DistanceSquared(right.position, camera.position);
              });
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    BeginBlendMode(BLEND_ALPHA);
    const auto drawTransparentActor = [&](const ActorSprite& actor) {
        const ActorAppearance& appearance = actor.appearance;
        const VisualResource& visual = FindVisual(visuals, actor.visual);
        const unsigned char opacity = static_cast<unsigned char>(appearance.alpha * 255.0F);
        if (opacity != 0) {
            if (IsTextureValid(visual.texture))
                DrawSprite(camera, screenUp,
                           Spec(visual, visual.worldHeight * appearance.scale),
                           actor.position, Color{255, 255, 255, opacity});
            else DrawSphere(actor.position, actor.hitRadius * appearance.scale,
                            Fade(SKYBLUE, appearance.alpha));
        }
        const Color color = appearance.burstAlpha > 0.0F ? GOLD
            : actor.kind == ActorKind::Boss ? Color{230, 140, 255, 255}
                                          : Color{150, 215, 255, 255};
        if (appearance.ringAlpha > 0.001F)
            DrawActorRing(camera, screenUp, actor.position,
                          actor.hitRadius * appearance.ringRadius,
                          Fade(color, appearance.ringAlpha));
        if (appearance.burstAlpha > 0.001F)
            DrawActorBurst(actor.position, actor.hitRadius, appearance.burst,
                           Fade(color, appearance.burstAlpha), actor.kind);
    };
    // 淡入淡出角色、退场快照与玩家透明弹合并远近顺序，不写深度；
    // 默认 shader 保留连续 alpha，不能让 cutout 的阈值截断淡出尾部。
    std::size_t actorIndex = 0;
    for (const Bullet* bullet : translucentPlayerBullets_) {
        while (actorIndex < translucentActors_.size() &&
               DistanceSquared(translucentActors_[actorIndex].position, camera.position) >
               DistanceSquared(bullet->position, camera.position))
            drawTransparentActor(translucentActors_[actorIndex++]);
        const VisualResource& visual = visuals[bullet->visual];
        if (!IsTextureValid(visual.texture) ||
            DistanceSquared(bullet->position, camera.position) < 0.7F * 0.7F)
            continue;
        DrawSprite(camera, screenUp,
                   Spec(visual, bullet->radius * visual.bulletDiameterScale),
                   bullet->position, Color{255, 255, 255, bullet->color.a});
    }
    while (actorIndex < translucentActors_.size())
        drawTransparentActor(translucentActors_[actorIndex++]);
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();

    // 光晕只做视觉叠加，不写深度以免遮挡后续透明层；每种外观最多绘制 2048 个近景光晕。
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    bullets.Pool().ForEachActive([](const Bullet& bullet) {
        if (bullet.beamLength > 0 && (bullet.Dangerous() || bullet.beamGrowSeconds>0)) {
            // 色壳在亮芯之后叠加，不能用不透明外柱挡住内部白芯；镜头进入光束也可见。
            rlDisableBackfaceCulling();
            const float width=bullet.radius*bullet.BeamVisualScale();
            DrawCylinderEx(bullet.position, bullet.BeamEnd(), width,
                           width, 12, Fade(bullet.color, 0.7F));
            DrawCylinderEx(bullet.position, bullet.BeamEnd(), width * 1.5F,
                           width * 1.5F, 12, Fade(bullet.color, 0.2F));
            rlDrawRenderBatchActive();
            rlEnableBackfaceCulling();
        }
    });
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
        if (bullet.beamLength > 0) {
            DrawCylinderWiresEx(bullet.position, bullet.BeamEnd(), bullet.radius, bullet.radius,
                                10, Color{255, 230, 90, 230});
            return;
        }
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
