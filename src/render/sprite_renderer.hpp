// 本文件负责声明按外观资源绘制弹幕、角色与道具的渲染接口。

#pragma once

#include "gameplay/actors/boss.hpp"
#include "gameplay/bullets/system.hpp"
#include "gameplay/actors/behavior.hpp"
#include "resources/loaded_assets.hpp"
#include "gameplay/effects/player_bomb.hpp"

#include "raylib.h"

#include <span>
#include <vector>

namespace koumakyou {
class CombatRenderer;
class ProjectileAssets;

/// @brief 道具、副机或纯视觉演出的临时绘制对象；position 为世界坐标，heightScale 乘以外观的世界高度。
/// visual 是当前 Stage 的外观 ID；渲染器只在本帧借用此数据。
struct PickupSprite {
    Vector3 position{};
    VisualId visual = kInvalidVisualId;
    float heightScale = 1.0F;
    const anm::Frame* animation = nullptr; // 借用道具或副机的当前帧，只在本次 Draw 内有效。
    unsigned char alpha = 255; // 额外不透明度（0～255），与贴图及原生动画 Alpha 相乘。
    float itemRotation = 0; // 本地 +Y 长轴弧度；由 DropSystem 模拟年龄产生，不跟随相机。
};
/// @brief 本帧借用的 Bomb 三维实体；几何由攻击实例提供，visual 决定原图集回退，模型按演出形状选择，绘制不做二次伤害计算。
struct BombVolumeSprite {
    const PlayerBombSprite* sprite=nullptr;
    VisualId visual=kInvalidVisualId;
};

/// @brief 协调角色、道具公告板与正式模型绘制；不持有传入的战斗对象或 GPU 资源。
class SpriteRenderer {
public:
    SpriteRenderer(){modelCommands_.reserve(BulletSystem::kCapacity);}

    /// @brief 绘制战斗或收尾帧；不透明模型先写深度，其他对象按相机深度排序，末尾恢复深度和剔除状态。
    /// 本次调用须汇总本帧所有透明战斗对象；分次调用不能保证跨批次遮挡正确。
    /// @param camera 当前 3D 相机。
    /// @param playerPosition 实际自机世界坐标；用于敌弹近镜头淡化，不跟随回放观察相机。
    /// @param bullets 全部活动弹幕。
    /// @param enemies 当前可见敌人指针序列；调用期间所有指针须有效。
    /// @param boss 当前 Boss 状态。
    /// @param pickups 当前道具与副机的临时绘制数据；调用期间 span 须有效。
    /// @param visuals 当前 Stage 的稠密外观资源表，索引即 VisualId。
    /// @param shaders 兼容旧清单的借用参数；透明绘制使用默认着色器，不再按阈值裁剪柔边。
    /// @param effects 本关卡退场快照；调用期间须保留对应外观资源。
    /// @param bombVolumes 借用战斗模拟的世界实体；光束按轴向和精灵深度切分，其余实体按中心排序。
    /// @param models 可选的本帧三维模型渲染器；资源借用期仅本次 Draw。
    /// @param external 可选的独立粒子池；与演员/弹幕进入同一透明队列，借用期仅本次Draw。
    void Draw(const Camera3D& camera, Vector3 playerPosition, const BulletSystem& bullets,
              std::span<const behavior::Actor*> enemies, const Boss* boss,
              std::span<const PickupSprite> pickups,
              std::span<const VisualResource> visuals,
              const SpriteShaders& shaders,
              const ActorEffectSystem& effects,
              std::span<const BombVolumeSprite> bombVolumes={},const effects::ExternalEffectSystem* external=nullptr,CombatRenderer* models=nullptr) const;
    /// @brief 最近一次 Draw 排入的普通激光片段数，含预警、不含 Bomb；用于定位随弹量放大的渲染开销。
    [[nodiscard]] std::size_t LastBeamSegmentCount() const { return beamSegmentCount_; }
#ifdef KOUMAKYOU_DEBUG_FEATURES
    /// @brief 在 Debug 构建中叠加判定球与发弹点。
    /// @param bullets 全部活动弹幕。
    /// @param enemies 当前可见敌人指针序列。
    /// @param boss 当前 Boss 状态。
    /// @param playerPosition 计算发弹点所需的玩家位置。
    void DrawDebugGeometry(const BulletSystem& bullets,
                           std::span<const behavior::Actor*> enemies,
                           const Boss* boss, Vector3 playerPosition,const ProjectileAssets* models=nullptr,std::span<const VisualResource> visuals={}) const;
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
        enum class Kind { Actor, Pickup, Bullet, Beam, Glow, BombVolume, ModelGroup } kind;
        float depth;
        std::size_t index=0;
        const Bullet* bullet=nullptr;
        float from=0,to=1;
    };
    mutable std::vector<TransparentDraw> transparentDraws_;
    mutable std::vector<std::size_t> modelCommands_;
    /// @brief 精灵沿相机前向的世界深度及投影包围框；透视坐标为相机 X/Z、Y/Z，正交坐标为 X、Y。
    /// bounds.width < 0 表示无法安全界定（如跨越相机平面），必须保留该深度处的激光切分。
    struct SpritePlane {
        float depth;
        Rectangle bounds{0,0,-1,-1};
        // 实际四角覆盖的相机深度区间；倾斜公告板不能只用中心平面排除光束交集。
        float nearDepth=0,farDepth=0;
    };
    // 缓冲逐帧复用；仅在投影可能重叠的精灵深度处分段，保留跨越演员前后的遮挡及连续 UV。
    mutable std::vector<const Bullet*> beams_;
    mutable std::vector<SpritePlane> spritePlanes_;
    mutable std::vector<float> bombActorDepths_,beamCuts_;
    mutable std::size_t beamSegmentCount_=0;
    mutable std::vector<int> glowCounts_;
};

}  // namespace koumakyou
