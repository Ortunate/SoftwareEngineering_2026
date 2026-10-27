// 本文件声明四面 Boss 生命周期适配；小恶魔独立退场，帕秋莉按四机体选择 Normal 六阶段。
#pragma once
#include "gameplay/actors/boss.hpp"
namespace koumakyou::stage4 {
/// @brief 独占行为演员及机体分支；默认使用 120 Hz 三维模式，60 Hz 中性模式只供源行为验证。
class Stage4Boss final : public Boss {
public:
    explicit Stage4Boss(bool final,behavior::UpdateRate rate=behavior::UpdateRate::Hz120,bool spatial=true);
    bool Update(float seconds,Vector3 player,BulletSystem& bullets) override;
    bool AdvancePhase() override;
    void ResumeCombat() override;
    void SetPlayerShot(ShotType shot) override;
    bool MusicOnEntrance() const override {return false;}
    bool ShouldAdvanceFromDamage() const override;
    bool PhaseDefeated() const override;
    bool CanTakeDamage() const override {return active&&!waiting_&&!retiring_&&actor_.CanTakeDamage();}
    bool CanBeTargeted() const override {return active&&!retiring_&&actor_.CanBeTargeted()&&health>0;}
    bool CanContactPlayer() const override {return active&&!waiting_&&!retiring_&&actor_.CanContactPlayer()&&health>0;}
    bool IsMidboss() const override {return !final_;}
    bool Defeated() const override {return defeated_;}
    const char* DisplayName() const override {return final_ ? "Patchouli Knowledge" : "Koakuma";}
    const char* SpellName() const override;
    int SpellCardNumber() const override;
    float TimeLimit() const override;
    BossHealthBar HealthBar() const override;
    void SetChallengeRank(int rank) override {actor_.SetRank(rank);}
    std::span<const int> SoundEvents() const override {return actor_.soundEvents;}
    std::span<const int> DropEvents() const override {return actor_.dropEvents;}
    std::span<const int> ItemEvents() const override {return actor_.itemEvents;}
    std::span<const behavior::Spawn> SpawnEvents() const override {return actor_.spawnEvents;}
    const battle::AnimationInterrupt* AnimationInterrupt() const override {return actor_.animationInterrupt ? &*actor_.animationInterrupt : nullptr;}
    bool DefeatActorsRequested() const override {return actor_.defeatActors;}
private:
    void StartPhase();
    behavior::Actor actor_;
    ShotType shot_=ShotType::HomingAmulet;
    bool final_,waiting_,retiring_=false,defeated_=false;
    double phaseElapsed_=0; // 外层组合等待独立于攻击重置后的阶段时钟，按秒累计，暂停不推进。
};
}
