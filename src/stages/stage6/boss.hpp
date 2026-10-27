// 本文件声明六面两场遭遇的公共 Boss 适配；主战保留对白中两次不同含义的恢复通知。
#pragma once
#include "gameplay/actors/boss.hpp"
#include "stages/stage6/behaviors.hpp"
namespace koumakyou::stage6 {
/// @brief 道中咲夜单次通知开战，蕾米莉亚先进入翅膀演出再由第二次通知开战；独占演员及跨阶段语义状态。
class Stage6Boss final : public Boss {
public:
    explicit Stage6Boss(bool final,behavior::UpdateRate rate=behavior::UpdateRate::Hz120,bool spatial=true);
    bool Update(float seconds,Vector3 player,BulletSystem& bullets) override;
    bool AdvancePhase() override;
    void ResumeCombat() override;
    void SetPlayerShot(ShotType shot) override;
    void SetPlayerBombActive(bool activeBomb) override {bombActive_=activeBomb;}
    bool MusicOnEntrance() const override {return false;}
    bool ShouldAdvanceFromDamage() const override;
    bool PhaseDefeated() const override;
    bool CanTakeDamage() const override {return active&&!waiting_&&!retiring_&&actor_.CanTakeDamage();}
    bool CanBeTargeted() const override {return active&&!retiring_&&actor_.CanBeTargeted()&&health>0;}
    bool CanContactPlayer() const override {return active&&!retiring_&&actor_.CanContactPlayer()&&health>0;}
    bool IsMidboss() const override {return !final_;}
    bool Defeated() const override {return defeated_;}
    const char* DisplayName() const override {return final_ ? "Remilia Scarlet" : "Sakuya Izayoi";}
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
    void UpdateBatTransformation(float seconds);
    behavior::Actor actor_;
    remilia::PhaseMemory memory_;
    ShotType shot_=ShotType::HomingAmulet;
    bool final_,waiting_=true,retiring_=false,defeated_=false,bombActive_=false,batTransformed_=false;
    int notifications_=0;
    double batRecoveryFrames_=0; // Bomb 停止后的 60 Hz 内容帧；暂停时不推进，活动 Bomb 每步重置为 60。
};
}
