// 本文件声明大妖精与琪露诺遭遇的公共 Boss 适配层，行为由二面命名阶段协程持有。
#pragma once
#include "gameplay/actors/boss.hpp"
#include "stages/stage2/behaviors.hpp"
namespace koumakyou::stage2 {
/// @brief 二面 Normal 遭遇；独占演员/协程，素材游标借用当前关卡，销毁须先于关卡卸载。
/// rate/spatial 仅供独立差分选择；正式工厂使用默认 120 Hz 与三维发弹/移动。
class Stage2Boss final : public Boss {
public:
    explicit Stage2Boss(bool final,behavior::UpdateRate rate=behavior::UpdateRate::Hz120,bool spatial=true);
    bool Update(float seconds,Vector3 player,BulletSystem& bullets) override;
    bool AdvancePhase() override;
    bool ShouldAdvanceFromDamage() const override;
    bool PhaseDefeated() const override;
    bool CanTakeDamage() const override { return !retiring_&&actor_.CanTakeDamage(); }
    bool CanBeTargeted() const override { return active&&!retiring_&&actor_.interactable&&health>0; }
    bool CanContactPlayer() const override { return active&&!retiring_&&actor_.interactable&&health>0; }
    bool IsMidboss() const override { return !final_; }
    bool Defeated() const override { return defeated_; }
    const char* DisplayName() const override { return final_ ? "Cirno" : "Daiyousei"; }
    const char* SpellName() const override;
    int SpellCardNumber() const override { return SpellName() ? (phase==2 ? 11 : phase==4 ? 15 : 18) : 0; }
    float TimeLimit() const override { return actor_.timeoutFrames/60.0F; }
    BossHealthBar HealthBar() const override;
    void SetChallengeRank(int rank) override { actor_.SetRank(rank); }
    std::span<const int> SoundEvents() const override { return actor_.soundEvents; }
    std::span<const int> DropEvents() const override { return actor_.dropEvents; }
    std::span<const int> ItemEvents() const override { return actor_.itemEvents; }
private:
    behavior::Actor actor_;
    bool final_,retiring_=false,defeated_=false;
};
}
