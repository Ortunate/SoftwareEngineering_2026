// 本文件声明由新典 Normal 原始帧脚本驱动的露米娅道中战与关底四阶段。
#pragma once
#include "gameplay/actors/boss.hpp"
#include "stages/stage1/reference_script.hpp"
namespace redmist::stage1 {
/// @brief 露米娅遭遇；原作 HP 与生命阈值统一换算，阶段包含受保护的准备期与脚本退场。
class RumiaBoss final : public Boss {
public:
    enum class Encounter { Midboss, Final };
    /// @brief 从道中 Sub9 入场，6000 原作 HP，1440 帧战斗上限，Normal 无道中符卡。
    void StartMidboss();
    /// @brief 从对话后的 Sub14 开始；关底保持非符、Night Bird、非符、Demarcation 顺序。
    void StartFinalBoss();
    bool Update(float deltaTime, Vector3 playerPosition, BulletSystem& bullets) override;
    bool AdvancePhase() override;
    void SetChallengeRank(int rank) override { script_.SetRank(rank); }
    [[nodiscard]] std::span<const int> SoundEvents() const override { return script_.soundEvents; }
    [[nodiscard]] std::span<const int> DropEvents() const override { return script_.dropEvents; }
    [[nodiscard]] bool Defeated() const override { return defeated_; }
    [[nodiscard]] const char* SpellName() const override;
    [[nodiscard]] bool IsMidboss() const override { return encounter_==Encounter::Midboss; }
    [[nodiscard]] const char* DisplayName() const override { return "Rumia"; }
    [[nodiscard]] bool CanTakeDamage() const override { return !retiring_ && script_.damageable; }
    [[nodiscard]] bool ShouldAdvanceFromDamage() const override;
    [[nodiscard]] float TimeLimit() const override { return script_.timeoutFrames/60.0F; }
    [[nodiscard]] Encounter CurrentEncounter() const { return encounter_; }
private:
    Encounter encounter_ = Encounter::Midboss;
    ReferenceScript script_;
    bool retiring_ = false;
    bool defeated_ = false;
};
} // namespace redmist::stage1
