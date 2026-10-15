// 本文件负责声明按外观资源绘制弹幕、角色与道具的渲染接口。

#pragma once

#include "gameplay/actors/boss.hpp"
#include "gameplay/bullets/system.hpp"
#include "gameplay/actors/enemy.hpp"
#include "resources/loaded_assets.hpp"

#include "raylib.h"

#include <span>
#include <vector>

namespace koumakyou {

/// @brief 单个道具的临时绘制数据；position 为世界坐标，heightScale 乘以外观的世界高度。
/// visual 是当前 Stage 的外观 ID；渲染器只在本帧借用此数据。
struct PickupSprite {
    Vector3 position{};
    VisualId visual = kInvalidVisualId;
    float heightScale = 1.0F;
    const anm::Frame* animation = nullptr; // 借用道具的当前帧，只在本次 Draw 内有效。
};

/// @brief 绘制角色、弹幕和道具的公告板与光晕；不持有传入的战斗对象或 GPU 资源。
class SpriteRenderer {
public:
    /// @brief 绘制战斗或收尾帧；全部贴图、光效按相机深度排序且不写深度，恢复深度和剔除状态。
    /// 本次调用须汇总本帧所有透明战斗对象；分次调用不能保证跨批次遮挡正确。
    /// @param camera 当前 3D 相机。
    /// @param bullets 全部活动弹幕。
    /// @param enemies 当前可见敌人指针序列；调用期间所有指针须有效。
    /// @param boss 当前 Boss 状态。
    /// @param pickups 当前活动道具的临时绘制数据；调用期间 span 须有效。
    /// @param visuals 当前 Stage 的稠密外观资源表，索引即 VisualId。
    /// @param shaders 兼容旧清单的借用参数；透明绘制使用默认着色器，不再按阈值裁剪柔边。
    /// @param effects 本关卡退场快照；调用期间须保留对应外观资源。
    void Draw(const Camera3D& camera, const BulletSystem& bullets,
              std::span<const Enemy*> enemies, const Boss* boss,
              std::span<const PickupSprite> pickups,
              std::span<const VisualResource> visuals,
              const SpriteShaders& shaders,
              const ActorEffectSystem& effects) const;
#ifdef KOUMAKYOU_DEBUG_FEATURES
    /// @brief 在 Debug 构建中叠加判定球与发弹点。
    /// @param bullets 全部活动弹幕。
    /// @param enemies 当前可见敌人指针序列。
    /// @param boss 当前 Boss 状态。
    /// @param playerPosition 计算发弹点所需的玩家位置。
    void DrawDebugGeometry(const BulletSystem& bullets,
                           std::span<const Enemy*> enemies,
                           const Boss* boss, Vector3 playerPosition) const;
#endif
private:
    /// @brief 本帧角色绘制副本，半透明排序不借用战斗对象或效果池槽位。
    struct ActorSprite {
        Vector3 position;
        VisualId visual;
        float hitRadius;
        ActorKind kind;
        ActorAppearance appearance;
        anm::Frame animation{};
    };
    mutable std::vector<ActorSprite> translucentActors_;
    /// @brief index 借用本帧 actor/pickup 数组；bullet 借用弹池，排序期间不得推进或回收对象。
    /// depth 为沿相机前向的世界距离；光束 from/to 为原线段的归一化区间，同时用于 UV 裁分。
    struct TransparentDraw {
        enum class Kind { Actor, Pickup, Bullet, Beam, Glow } kind;
        float depth;
        std::size_t index=0;
        const Bullet* bullet=nullptr;
        float from=0,to=1;
    };
    mutable std::vector<TransparentDraw> transparentDraws_;
    // 缓冲逐帧复用；光束在精灵的深度平面处分段，避免一条长激光被整体排到演员前面或后面。
    mutable std::vector<const Bullet*> beams_;
    mutable std::vector<float> spriteDepths_,beamCuts_;
    mutable std::vector<int> glowCounts_;
};

}  // namespace koumakyou
