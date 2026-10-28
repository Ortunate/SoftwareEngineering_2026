// 本文件声明由 Boss 解析的阶段技能组合、血条绘制及完整战斗生命周期。
#pragma once
#include "gameplay/bosses/config.hpp"
namespace koumakyou::bosses {
/// @brief 独占角色演员、已解析技能和启动配置；默认 120 Hz，60 Hz 中性模式只供原作差分。
class ConfiguredBoss : public Boss {
public:
    explicit ConfiguredBoss(behavior::UpdateRate rate=behavior::UpdateRate::Hz120,bool spatial=true):actor_(rate),spatial_(spatial) {}
    ~ConfiguredBoss() override;
    /// @brief 解析并复制阶段、技能和血条；非法引用/比例/跳转抛 invalid_argument，验证失败保留旧战斗。
    virtual void StartBoss(const StartConfig& config);
    bool Update(float seconds,Vector3 player,BulletSystem& bullets) override;
    bool AdvanceSkill() override;
    void ResumeCombat() override;
    bool MusicOnEntrance() const override {return config_.musicOnEntrance;}
    bool ControlsWorldTime() const override {return config_.controlsWorldTime;}
    bool WorldTimeStopped() const override {return active&&actor_.phaseClockPaused;}
    bool CanTakeDamage() const override {return active&&!waiting_&&!retiring_&&actor_.CanTakeDamage();}
    bool CanBeTargeted() const override {return active&&!retiring_&&actor_.CanBeTargeted()&&health>0;}
    bool CanContactPlayer() const override {return active&&!waiting_&&!retiring_&&actor_.CanContactPlayer()&&health>0;}
    bool ShouldAdvanceFromDamage() const override;
    bool SkillDefeated() const override;
    bool Defeated() const override {return defeated_;}
    int CompletionScore() const override {return defeated_ ? config_.defeatScore : config_.retreatScore;}
    const char* SpellName() const override;
    int SpellCardNumber() const override;
    float TimeLimit() const override;
    BossHealthBar HealthBar() const override;
    void DrawHealthBar(Rectangle area) const override;
    void SetChallengeRank(int rank) override {actor_.SetRank(rank);}
    std::span<const int> SoundEvents() const override {return actor_.soundEvents;}
    std::span<const int> DropEvents() const override {return actor_.dropEvents;}
    std::span<const int> ItemEvents() const override {return actor_.itemEvents;}
    std::span<const behavior::Spawn> SpawnEvents() const override {return actor_.spawnEvents;}
    bool DefeatActorsRequested() const override {return actor_.defeatActors;}
    const battle::AnimationInterrupt* AnimationInterrupt() const override {return actor_.animationInterrupt ? &*actor_.animationInterrupt : nullptr;}
    /// @brief 当前技能定义；入场、退场或未启动返回 nullptr，借用期到下次 StartBoss。
    const SkillDefinition* CurrentSkill() const;
    /// @brief 查询角色固有技能定义的值副本，不启动行为；不匹配角色或非技能入口抛 invalid_argument。
    SkillDefinition DescribeSkill(BehaviorId id,ShotType shot=ShotType::HomingAmulet) const {return ResolveSkill(id,shot);}
protected:
    behavior::Actor actor_;
    /// @brief 角色解析自身技能/演出；不匹配的角色、未定义或非战斗行为作为技能时须抛 invalid_argument。
    virtual SkillDefinition ResolveSkill(BehaviorId id,ShotType shot) const=0;
    virtual Attack ResolveBehavior(BehaviorId id) const=0;
    virtual void AfterUpdate(float) {}
private:
    struct ResolvedPhase {std::vector<SkillDefinition> skills;};
    void StartSkill(std::size_t stage,std::size_t skill);
    void Sync();
    StartConfig config_;
    std::vector<ResolvedPhase> resolved_;
    Attack intro_,defeat_,retreat_;
    std::vector<Attack> resumeSteps_;
    bool spatial_,waiting_=false,retiring_=false,defeated_=false;
    std::size_t notifications_=0,stage_=0,skill_=0;
    double skillElapsed_=0;
};
} // namespace koumakyou::bosses
