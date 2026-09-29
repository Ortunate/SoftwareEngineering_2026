// 本文件负责声明第一面露米娅 Boss 的遭遇状态与技能接口。

#pragma once

#include "gameplay/actors/boss.hpp"

namespace redmist::stage1 {

/// @brief Stage 1 的露米娅道中战与关底四阶段。
class RumiaBoss final : public Boss {
public:
    enum class Encounter { Midboss, Final };
    void StartMidboss();
    void StartFinalBoss();
    bool Update(float deltaTime, Vector3 playerPosition,
                BulletSystem& bullets) override;
    bool AdvancePhase() override;
    [[nodiscard]] const char* SpellName() const override;
    [[nodiscard]] bool IsMidboss() const override {
        return encounter_ == Encounter::Midboss;
    }
    [[nodiscard]] const char* DisplayName() const override {
        return IsMidboss() ? "RUMIA  MIDBOSS  NORMAL"
                           : "RUMIA  STAGE BOSS  NORMAL";
    }
    [[nodiscard]] Encounter CurrentEncounter() const { return encounter_; }

private:
    Encounter encounter_ = Encounter::Midboss;
    void FireAimedVolley(BulletSystem& bullets, Vector3 playerPosition);
    void FireAimedStream(BulletSystem& bullets, Vector3 playerPosition);
    void CastNightBird(BulletSystem& bullets, Vector3 playerPosition);
    void FireMoonlightBeam(BulletSystem& bullets, Vector3 playerPosition);
    void CastDemarcation(BulletSystem& bullets, Vector3 playerPosition);
};

}  // namespace redmist::stage1
