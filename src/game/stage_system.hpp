#pragma once

#include "game/game_types.hpp"

#include <cstddef>
#include <functional>

namespace redmist {

enum class StageEventKind : std::uint8_t {
    SpawnLine,
    SpawnStarFormation,
    SpawnCrossingPair,
    SpawnElite,
    StartMidboss,
    StartDialogue,
    StartFinalBoss,
    ShowMessage
};

struct StageEvent {
    float time = 0.0F;
    StageEventKind kind = StageEventKind::SpawnLine;
    EnemyKind enemy = EnemyKind::Fairy;
    int count = 1;
    Vector3 center{0.0F, 0.0F, 8.5F};
    float spacing = 1.0F;
};

class StageDirector {
public:
    using EventHandler = std::function<void(const StageEvent&)>;

    void Reset();
    void Update(float deltaTime, const EventHandler& handler);
    [[nodiscard]] float Elapsed() const { return elapsed_; }

private:
    float elapsed_ = 0.0F;
    std::size_t nextEvent_ = 0;
};

}  // namespace redmist
