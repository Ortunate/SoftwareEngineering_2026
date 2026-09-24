#pragma once

#include "game/bullet_system.hpp"

namespace redmist {

class Boss {
public:
    virtual ~Boss() = default;
    // Returns true when the current phase has reached its time limit.
    virtual bool Update(float deltaTime, Vector3 playerPosition,
                        BulletSystem& bullets) = 0;
    // Returns true if another phase has started; false when defeated.
    virtual bool AdvancePhase() = 0;
    [[nodiscard]] virtual const char* SpellName() const = 0;

    bool active = false;
    Vector3 position{0.0F, 2.0F, 7.2F};
    float health = 0.0F;
    float maxHealth = 0.0F;
    float phaseTimer = 0.0F;
    float fireTimer = 0.0F;
    float moveClock = 0.0F;
    float starRotation = 0.0F;
    int phase = 0;
};

class RumiaBoss final : public Boss {
public:
    void Start();
    bool Update(float deltaTime, Vector3 playerPosition,
                BulletSystem& bullets) override;
    bool AdvancePhase() override;
    [[nodiscard]] const char* SpellName() const override;

private:
    void FireAimedVolley(BulletSystem& bullets, Vector3 playerPosition);
};

}  // namespace redmist
