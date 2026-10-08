// 本文件负责角色入场表现与脱离战斗对象生命周期的退场效果快照。

#pragma once

#include "core/fixed_pool.hpp"
#include "resources/visual_assets.hpp"
#include "raylib.h"

namespace redmist {

/// @brief 表现规格与退场原因；清场不等同于击破，不产生奖励或爆散。
enum class ActorKind { Enemy, Boss };
enum class ActorExitReason { Retreat, Defeated, StageClear };

/// @brief 纯视觉采样值；尺寸乘以外观高度，透明度为 [0,1]，环半径乘以判定半径。
/// burst 为击破粒子的扩散进度 [0,1]，burstAlpha 为零时不绘制粒子或闪光。
struct ActorAppearance {
    float scale = 1.0F;
    float alpha = 1.0F;
    float ringRadius = 0.0F;
    float ringAlpha = 0.0F;
    float burst = 0.0F;
    float burstAlpha = 0.0F;
};

/// @brief 活动角色的视觉时钟；不改变攻击、判定和移动，暂停时调用方停止推进。
class ActorPresentation {
public:
    /// @brief 新角色从入场首帧开始；Boss 使用 Boss 规格。
    explicit ActorPresentation(ActorKind kind = ActorKind::Enemy) : kind_(kind) {}
    /// @brief 设置本次遭遇的规格并重置入场；Boss 换阶段不得调用。
    void Reset(ActorKind kind = ActorKind::Enemy);
    /// @brief 推进视觉时钟，deltaTime 为非负秒数。
    void Update(float deltaTime);
    /// @brief 已知退场剩余秒数时提前淡出；始终保留战斗对象到原定退场时刻。
    void PrepareExit(float remainingSeconds);
    /// @brief 返回当前入场与预退场的组合表现。
    [[nodiscard]] ActorAppearance Sample() const;
    /// @brief 查询规格，供脱离对象的退场快照使用。
    [[nodiscard]] ActorKind Kind() const { return kind_; }
    /// @brief 普通退场淡出时长，单位为秒。
    [[nodiscard]] static float ExitDuration(ActorKind kind);

private:
    ActorKind kind_ = ActorKind::Enemy;
    float age_ = 0.0F;
    // 预退场只会加深，不会因曲线轨迹或 dt 波动重新显现。
    float departure_ = 0.0F;
};

/// @brief 值快照，不持有 Enemy/Boss 指针或纹理；VisualId 仅在当前关卡资源存活时有效。
struct ActorExitEffect {
    bool active = false;
    VisualId visual = kInvalidVisualId;
    Vector3 position{};
    float hitRadius = 0.55F;
    ActorKind kind = ActorKind::Enemy;
    ActorExitReason reason = ActorExitReason::Retreat;
    ActorAppearance initial{};
    float age = 0.0F;
    float duration = 0.25F;
    /// @brief 采样淡出、收缩或击破爆散；不参与碰撞或索敌。
    [[nodiscard]] ActorAppearance Sample() const;
};

/// @brief 最多保留 512 个退场快照，覆盖敌人池整池清场及连续短时退场。
/// 池满只丢弃视觉，不能影响计分和战斗对象回收；切换关卡前必须 Clear。
class ActorEffectSystem {
public:
    /// @brief 复制角色最后一帧外观；position 是世界坐标，hitRadius 是正的世界单位半径。
    /// 调用方须在一次实际退场时仅调用一次，传入当前关卡的 visual。
    /// @return 池满返回 false，不延长战斗对象寿命。
    bool Spawn(VisualId visual, Vector3 position, float hitRadius,
               const ActorPresentation& presentation, ActorExitReason reason);
    /// @brief 推进效果并回收到期槽位；deltaTime 为非负秒数，暂停时不调用。
    void Update(float deltaTime);
    /// @brief 立即丢弃全部效果；在卸载关卡纹理之前调用。
    void Clear() { effects_.Clear(); }
    /// @brief 只读借用快照池，借用期间不得修改本系统。
    [[nodiscard]] const FixedPool<ActorExitEffect, 512>& Pool() const { return effects_; }

private:
    FixedPool<ActorExitEffect, 512> effects_;
};

}  // namespace redmist
