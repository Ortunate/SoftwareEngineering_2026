// 本文件声明由新典 Normal 移动与技能编排驱动的露米娅道中战与关底四阶段。
#pragma once
#include "gameplay/actors/boss.hpp"
#include "stages/stage1/behaviors.hpp"
namespace koumakyou::stage1 {
/// @brief 露米娅遭遇；HP 与生命阈值保持原始点数，阶段包含受保护的准备期与编排退场。
class RumiaBoss final : public Boss {
public:
    enum class Encounter { Midboss, Final };
    /// @brief 从 Midboss 行为入场，6000 原作 HP，1440 帧战斗上限，Normal 无道中符卡。
    void StartMidboss();
    /// @brief 从对话后的 FirstNonspell 开始；关底保持非符、Night Bird、非符、Demarcation 顺序。
    void StartFinalBoss();
    bool Update(float deltaTime, Vector3 playerPosition, BulletSystem& bullets) override;
    bool AdvancePhase() override;
    void SetChallengeRank(int rank) override { behavior_.SetRank(rank); }
    [[nodiscard]] std::span<const int> SoundEvents() const override { return behavior_.soundEvents; }
    [[nodiscard]] std::span<const int> DropEvents() const override { return behavior_.dropEvents; }
    [[nodiscard]] bool Defeated() const override { return defeated_; }
    [[nodiscard]] const char* SpellName() const override;
    [[nodiscard]] int SpellCardNumber() const override { return SpellName() ? (phase==2 ? 3 : 7) : 0; }
    [[nodiscard]] bool IsMidboss() const override { return encounter_==Encounter::Midboss; }
    [[nodiscard]] const char* DisplayName() const override { return "Rumia"; }
    [[nodiscard]] bool CanTakeDamage() const override { return !retiring_ && behavior_.damageable; }
    /// @brief 准备期的免伤不关闭本体；退场行为可能保留正生命和 active，但已经不再伤害玩家。
    [[nodiscard]] bool CanContactPlayer() const override { return Boss::CanContactPlayer() && !retiring_; }
    /// @brief 只在可受击期检查伤害阈值；阶段交接及行为准备期不能因碰撞重复推进。
    [[nodiscard]] bool ShouldAdvanceFromDamage() const override;
    [[nodiscard]] float TimeLimit() const override { return behavior_.timeoutFrames/60.0F; }
    /// @brief 道中为单段；关底将非符和后续符卡按原作 HP 分组，符卡始终保留左端小段，不铺满血条。
    [[nodiscard]] BossHealthBar HealthBar() const override;
    [[nodiscard]] Encounter CurrentEncounter() const { return encounter_; }
private:
    Encounter encounter_ = Encounter::Midboss;
    behavior::Actor behavior_;
    bool retiring_ = false;
    bool defeated_ = false;
};
} // namespace koumakyou::stage1
