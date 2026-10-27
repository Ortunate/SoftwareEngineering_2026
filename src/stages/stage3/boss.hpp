// 本文件声明红美铃的公共 Boss 生命周期适配，入场和开战分离，命名阶段负责实际攻击。
#pragma once
#include "gameplay/actors/boss.hpp"
#include "stages/stage3/behaviors.hpp"
namespace koumakyou::stage3 {
/// @brief 三面 Normal 遭遇，独占行为演员；正式工厂默认 120 Hz 三维模式，测试可选中性 60 Hz。
class Stage3Boss final : public Boss {
public:
    explicit Stage3Boss(bool final,behavior::UpdateRate rate=behavior::UpdateRate::Hz120,bool spatial=true);
    bool Update(float seconds,Vector3 player,BulletSystem& bullets) override;
    bool AdvancePhase() override;
    void ResumeCombat() override;
    bool MusicOnEntrance() const override { return false; }
    bool ShouldAdvanceFromDamage() const override;
    bool PhaseDefeated() const override;
    bool CanTakeDamage() const override {return !waiting_&&!retiring_&&actor_.CanTakeDamage();}
    bool CanBeTargeted() const override {return active&&!retiring_&&actor_.CanBeTargeted()&&health>0;}
    bool CanContactPlayer() const override {return active&&!waiting_&&!retiring_&&actor_.CanContactPlayer()&&health>0;}
    bool IsMidboss() const override {return !final_;}
    bool Defeated() const override {return defeated_;}
    const char* DisplayName() const override {return "Hong Meiling";}
    const char* SpellName() const override;
    int SpellCardNumber() const override;
    float TimeLimit() const override {return waiting_ ? -1 : actor_.timeoutFrames/60.0F;}
    BossHealthBar HealthBar() const override;
    void SetChallengeRank(int rank) override {actor_.SetRank(rank);}
    std::span<const int> SoundEvents() const override {return actor_.soundEvents;}
    std::span<const int> DropEvents() const override {return actor_.dropEvents;}
    std::span<const behavior::Spawn> SpawnEvents() const override {return actor_.spawnEvents;}
    const battle::AnimationInterrupt* AnimationInterrupt() const override {return actor_.animationInterrupt ? &*actor_.animationInterrupt : nullptr;}
    bool DefeatActorsRequested() const override {return actor_.defeatActors;}
private:
    behavior::Actor actor_;
    bool final_,waiting_,retiring_=false,defeated_=false;
};
}
