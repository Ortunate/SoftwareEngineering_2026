// 本文件声明咲夜的公共 Boss 适配；对白、阶段生命和世界时停都由独占行为演员驱动。
#pragma once
#include "gameplay/actors/boss.hpp"
namespace koumakyou::stage5 {
/// @brief 道中和关底均先等待对白开战；默认 120 Hz 空间模式，60 Hz 中性模式用于源时序对照。
class Stage5Boss final : public Boss {
public:
    explicit Stage5Boss(bool final,behavior::UpdateRate rate=behavior::UpdateRate::Hz120,bool spatial=true);
    bool Update(float seconds,Vector3 player,BulletSystem& bullets) override;
    bool AdvancePhase() override;
    void ResumeCombat() override;
    bool MusicOnEntrance() const override {return false;}
    bool ControlsWorldTime() const override {return true;}
    bool WorldTimeStopped() const override {return active&&actor_.phaseClockPaused;}
    bool ShouldAdvanceFromDamage() const override;
    bool PhaseDefeated() const override;
    bool CanTakeDamage() const override {return active&&!waiting_&&!retiring_&&actor_.CanTakeDamage();}
    bool CanBeTargeted() const override {return active&&!retiring_&&actor_.CanBeTargeted()&&health>0;}
    bool CanContactPlayer() const override {return active&&!retiring_&&actor_.CanContactPlayer()&&health>0;}
    bool IsMidboss() const override {return !final_;}
    bool Defeated() const override {return defeated_;}
    const char* DisplayName() const override {return "Sakuya Izayoi";}
    const char* SpellName() const override;
    int SpellCardNumber() const override;
    float TimeLimit() const override {return waiting_||retiring_ ? -1 : actor_.timeoutFrames/60.0F;}
    BossHealthBar HealthBar() const override;
    void SetChallengeRank(int rank) override {actor_.SetRank(rank);}
    std::span<const int> SoundEvents() const override {return actor_.soundEvents;}
    std::span<const int> DropEvents() const override {return actor_.dropEvents;}
    std::span<const int> ItemEvents() const override {return actor_.itemEvents;}
    std::span<const behavior::Spawn> SpawnEvents() const override {return actor_.spawnEvents;}
    bool DefeatActorsRequested() const override {return actor_.defeatActors;}
    const battle::AnimationInterrupt* AnimationInterrupt() const override {return actor_.animationInterrupt ? &*actor_.animationInterrupt : nullptr;}
private:
    void StartPhase();
    behavior::Actor actor_;
    bool final_,waiting_=true,retiring_=false,defeated_=false;
};
}
