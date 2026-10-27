// 本文件负责定义 Boss 的抽象接口及血量、阶段等公共状态。

#pragma once

#include "gameplay/bullets/system.hpp"
#include "gameplay/effects/actor_effects.hpp"
#include "gameplay/actors/boss_health.hpp"
#include "gameplay/actors/behavior.hpp"
#include "gameplay/data/native_combat.hpp"
#include <stdexcept>

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
    /// @brief 对话后的外部开战通知；仅延迟开战的遭遇覆盖，默认拒绝非法调用。
    virtual void ResumeCombat() { throw std::logic_error("Boss is not waiting for combat"); }
    /// @brief 是否入场即换 Boss 音乐；先入场再对白的遭遇可返回 false，由 DialogueCue 决定换曲。
    [[nodiscard]] virtual bool MusicOnEntrance() const { return true; }
    /// @brief 查询当前阶段需要展示的符卡名。
    /// @return 符卡名指针；非符卡阶段返回 nullptr。
    [[nodiscard]] virtual const char* SpellName() const = 0;
    /// @brief 当前符卡在全局只读目录中的编号；0 表示非符或未绑定，不据此开放原生符卡练习入口。
    [[nodiscard]] virtual int SpellCardNumber() const { return 0; }
    /// @brief 当前遭遇是否为道中 Boss。
    [[nodiscard]] virtual bool IsMidboss() const = 0;
    /// @brief HUD 中展示的 Boss 名称。
    [[nodiscard]] virtual const char* DisplayName() const = 0;

    /// @brief 当前准备/退场期是否接受玩家弹和 Bomb 伤害。
    [[nodiscard]] virtual bool CanTakeDamage() const { return true; }
    /// @brief 自机弹与追踪可接触的本体；隐藏/瞬移可关闭，受伤保护仍可保持弹接触。
    [[nodiscard]] virtual bool CanBeTargeted() const { return active&&health>0; }
    /// @brief 活动且存活的本体可接触玩家，独立于受伤开关；子类须排除只保留演员的无害退场状态。
    [[nodiscard]] virtual bool CanContactPlayer() const { return active && health>0; }
    /// @brief 生命耗尽或达到下一阶段阈值时返回 true；Game 统一清弹并调用 AdvancePhase。
    [[nodiscard]] virtual bool ShouldAdvanceFromDamage() const { return health <= 0; }
    /// @brief 当前攻击是否由伤害完成；正 HP 符卡阈值须由子类明确覆盖，超时不得视为击破。
    /// Game 在 AdvancePhase 之前读取，用于符卡取得与奖励，和整场 Defeated 分开。
    [[nodiscard]] virtual bool PhaseDefeated() const { return health<=0; }
    /// @brief 当前阶段原始时间上限（秒）；负值表示退场或无计时阶段。
    [[nodiscard]] virtual float TimeLimit() const { return -1; }
    /// @brief 返回当前组的独立血条快照；默认按 health/maxHealth 显示单段，组合阶段由具体 Boss 覆盖。
    /// 仅供展示，不能用于伤害或阶段判定；无效生命上限、未活动或退场应返回空快照。
    [[nodiscard]] virtual BossHealthBar HealthBar() const;
    /// @brief 接收关卡自适应难度值 10…32；普通演员默认忽略，原作脚本演员用于弹速/弹数。
    virtual void SetChallengeRank(int) {}
    /// @brief 工厂创建后、开战前接收当前机体；按机体分支的遭遇保存它，其他 Boss 默认忽略。
    virtual void SetPlayerShot(ShotType) {}
    /// @brief 固定步开始前接收当前 Bomb 活动状态；用于内容明确定义的化蝠等反应，不持有自机对象。
    virtual void SetPlayerBombActive(bool) {}
    /// @brief 支持操纵世界时钟的遭遇须先更新敌方，再采样玩家输入；默认保持既有关卡的更新顺序。
    virtual bool ControlsWorldTime() const {return false;}
    /// @brief 当前是否冻结玩家、全部弹幕、道具与背景；敌人脚本/移动和关卡事件继续推进。
    /// 支持者必须在阶段交接与退场时解除，不能将该状态当作菜单暂停。
    virtual bool WorldTimeStopped() const {return false;}
    /// @brief 当前本体动画的最后一次中断，只借用至下一次 Update；缺省不发送中断。
    virtual const battle::AnimationInterrupt* AnimationInterrupt() const {return nullptr;}
    /// @brief 借用本帧脚本产生的音效编号；下次更新失效，普通演员返回空表。
    [[nodiscard]] virtual std::span<const int> SoundEvents() const { return {}; }
    /// @brief 借用本帧脚本要求散落的道具数量；调用方按当前玩家 Power 决定 P 点或点数。
    [[nodiscard]] virtual std::span<const int> DropEvents() const { return {}; }
    /// @brief 本次更新要求生成的原生道具号 0～6；与散落数量分开，调用方须在下一次 Update 前消费。
    [[nodiscard]] virtual std::span<const int> ItemEvents() const { return {}; }
    /// @brief 本次更新请求生成的独立子演员；借用期到下次 Update，接收方不按时间轴的 Boss 活动过滤。
    [[nodiscard]] virtual std::span<const behavior::Spawn> SpawnEvents() const { return {}; }
    /// @brief 本次更新是否请求普通演员生命归零；总控保留正常击破、奖励与死亡回调，不删除 Boss。
    [[nodiscard]] virtual bool DefeatActorsRequested() const { return false; }
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
    float hitRadius = 0.95F; // 世界半径，自机弹与本体接触共用，不随视觉入退场缩放。
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
