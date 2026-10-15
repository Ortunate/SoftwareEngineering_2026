// 本文件负责定义 Boss 的抽象接口及血量、阶段等公共状态。

#pragma once

#include "gameplay/bullets/system.hpp"
#include "gameplay/effects/actor_effects.hpp"
#include "gameplay/actors/boss_health.hpp"

namespace koumakyou {

/// @brief Boss 抽象基类，保存判定、血量与阶段公共状态。
class Boss {
public:
    /// @brief 通过基类指针安全销毁具体 Boss。
    virtual ~Boss() = default;
    /// @brief 推进当前阶段的移动与攻击。
    /// 子类同时推进 presentation；只在整场遭遇启动时重置视觉时钟。
    /// @param deltaTime 本帧经过的秒数。
    /// @param playerPosition 玩家判定中心。
    /// @param bullets 接收技能弹幕的系统。
    /// @return 到达生命/时间回调或脚本结束时返回 true，调用方需执行 AdvancePhase。
    virtual bool Update(float deltaTime, Vector3 playerPosition,
                        BulletSystem& bullets) = 0;
    /// @brief 当前阶段结束后尝试进入下一阶段。
    /// @return 演员仍需继续更新（下一阶段或退场）时返回 true；整场结束返回 false。
    virtual bool AdvancePhase() = 0;
    /// @brief 查询当前阶段需要展示的符卡名。
    /// @return 符卡名指针；非符卡阶段返回 nullptr。
    [[nodiscard]] virtual const char* SpellName() const = 0;
    /// @brief 当前遭遇是否为道中 Boss。
    [[nodiscard]] virtual bool IsMidboss() const = 0;
    /// @brief HUD 中展示的 Boss 名称。
    [[nodiscard]] virtual const char* DisplayName() const = 0;

    /// @brief 当前准备/退场期是否接受玩家弹和 Bomb 伤害。
    [[nodiscard]] virtual bool CanTakeDamage() const { return true; }
    /// @brief 生命耗尽或达到下一阶段阈值时返回 true；Game 统一清弹并调用 AdvancePhase。
    [[nodiscard]] virtual bool ShouldAdvanceFromDamage() const { return health <= 0; }
    /// @brief 当前阶段原始时间上限（秒）；负值表示退场或无计时阶段。
    [[nodiscard]] virtual float TimeLimit() const { return -1; }
    /// @brief 返回当前组的独立血条快照；默认按 health/maxHealth 显示单段，组合阶段由具体 Boss 覆盖。
    /// 仅供展示，不能用于伤害或阶段判定；无效生命上限、未活动或退场应返回空快照。
    [[nodiscard]] virtual BossHealthBar HealthBar() const;
    /// @brief 接收关卡自适应难度值 10…32；普通演员默认忽略，原作脚本演员用于弹速/弹数。
    virtual void SetChallengeRank(int) {}
    /// @brief 借用本帧脚本产生的音效编号；下次更新失效，普通演员返回空表。
    [[nodiscard]] virtual std::span<const int> SoundEvents() const { return {}; }
    /// @brief 借用本帧脚本要求散落的道具数量；调用方按当前玩家 Power 决定 P 点或点数。
    [[nodiscard]] virtual std::span<const int> DropEvents() const { return {}; }
    /// @brief 整场退场是由击破触发还是超时触发；退场结束后仍保留结果。
    [[nodiscard]] virtual bool Defeated() const { return health <= 0; }
    bool active = false;
    /// @brief 整场遭遇的视觉时钟；阶段切换不能重置入场。
    ActorPresentation presentation{ActorKind::Boss};
    VisualId visual = kInvalidVisualId;
    battle::Cursor animation;
    VisualId defeatVisual = kInvalidVisualId; // 可覆盖清单的击破动画，invalid 时采用清单绑定。
    int animationScript = -1; // 关卡的主动画脚本；负值沿用清单，战斗图不使用对话表情。
    double animationAge = 0; // 本次脚本启动后的秒数，换阶段不自动重启待机动画。
    std::uint32_t animationRevision = 0;
    Vector3 position{0.0F, 2.0F, 7.2F};
    float health = 0.0F;
    float maxHealth = 0.0F;
    float hitRadius = 0.95F;
    /// @brief 当前阶段已运行秒数；换阶段时归零，超时由具体 Boss 决定。
    float phaseTimer = 0.0F;
    float fireTimer = 0.0F;
    float moveClock = 0.0F;
    float starRotation = 0.0F;
    /// @brief 当前阶段编号；0 表示未开始或已退场，正数由具体 Boss 定义。
    int phase = 0;

    /// @brief 沿瞄准方向求 Boss 的发弹点。
    /// @param playerPosition 玩家判定中心。
    /// @return 世界坐标中的发弹点。
    [[nodiscard]] Vector3 MuzzlePosition(Vector3 playerPosition) const;
};

}  // namespace koumakyou
