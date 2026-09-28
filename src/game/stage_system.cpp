#include "game/stage_system.hpp"

#include <array>
#include <cmath>
#include <stdexcept>

namespace redmist {
namespace {

constexpr StageEventId kMidbossEventId = 1;
constexpr StageEventId kDialogueEventId = 2;
constexpr StageEventId kFinalBossEventId = 3;

// 普通波次的 time 相对上一事件实际开始；Boss 和对话后的波次等待相应事件结束。
// 参考 187 秒 Normal 录像近似编排；录像开头的选人时间不计入。
constexpr std::array<StageEvent, 21> kNormalStageOneEvents{{
    {0.2F, StageEventKind::ShowMessage},
    {0.8F, StageEventKind::SpawnLine, EnemyKind::Fairy, 8,
     {0.0F, 1.2F, 8.8F}, 1.15F},
    {3.5F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 8},
    {3.5F, StageEventKind::SpawnLine, EnemyKind::DarkFairy, 6,
     {0.0F, 2.0F, 8.8F}, 1.5F},
    {3.5F, StageEventKind::SpawnCrossingPair, EnemyKind::DarkFairy, 6},
    {3.5F, StageEventKind::SpawnLine, EnemyKind::Fairy, 8,
     {0.0F, -0.5F, 8.8F}, 1.2F},
    {3.5F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 8},
    {3.5F, StageEventKind::SpawnLine, EnemyKind::Fairy, 10,
     {0.0F, 1.5F, 8.8F}, 1.0F},
    {4.0F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 10},
    {.time = 4.0F, .kind = StageEventKind::StartMidboss,
     .id = kMidbossEventId},
    {.time = 3.0F, .kind = StageEventKind::SpawnLine,
     .enemy = EnemyKind::DarkFairy, .count = 6,
     .center = {0.0F, 1.2F, 8.7F}, .spacing = 1.5F,
     .afterEndOf = kMidbossEventId},
    {4.0F, StageEventKind::SpawnCrossingPair, EnemyKind::DarkFairy, 8},
    {4.0F, StageEventKind::SpawnLine, EnemyKind::DarkFairy, 7,
     {0.0F, -0.8F, 8.8F}, 1.25F},
    {4.0F, StageEventKind::SpawnCrossingPair, EnemyKind::DarkFairy, 8},
    {5.0F, StageEventKind::SpawnLine, EnemyKind::Fairy, 10,
     {0.0F, 1.2F, 8.8F}, 1.0F},
    {4.0F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 10},
    {4.0F, StageEventKind::SpawnLine, EnemyKind::Fairy, 10,
     {0.0F, -1.0F, 8.8F}, 1.0F},
    {4.0F, StageEventKind::SpawnCrossingPair, EnemyKind::Fairy, 10},
    {4.0F, StageEventKind::SpawnLine, EnemyKind::Fairy, 8,
     {0.0F, 1.2F, 8.8F}, 1.2F},
    {.time = 4.0F, .kind = StageEventKind::StartDialogue,
     .id = kDialogueEventId},
    {.time = 0.0F, .kind = StageEventKind::StartFinalBoss,
     .id = kFinalBossEventId, .afterEndOf = kDialogueEventId},
}};

}  // namespace

StageDirector::StageDirector() : StageDirector(kNormalStageOneEvents) {}

StageDirector::StageDirector(std::span<const StageEvent> events)
    : events_(events), states_(events.size()) {
    for (std::size_t index = 0; index < events_.size(); ++index) {
        const StageEvent& event = events_[index];
        if (!std::isfinite(event.time) || event.time < 0.0F) {
            throw std::invalid_argument("StageEvent time must be nonnegative and finite");
        }
        if (event.id) {
            for (std::size_t previous = 0; previous < index; ++previous) {
                if (events_[previous].id == event.id) {
                    throw std::invalid_argument("Duplicate StageEvent id");
                }
            }
        }
        if (event.afterEndOf) {
            for (std::size_t previous = 0; previous < index; ++previous) {
                if (events_[previous].id == event.afterEndOf) {
                    states_[index].dependencyIndex = previous;
                    break;
                }
            }
            if (!states_[index].dependencyIndex) {
                throw std::invalid_argument("StageEvent dependency must reference an earlier id");
            }
        }
    }
}

void StageDirector::Reset() {
    elapsed_ = 0.0F;
    nextEvent_ = 0;
    for (EventState& state : states_) {
        state.startedAt.reset();
        state.endedAt.reset();
    }
}

void StageDirector::Update(float deltaTime, const EventHandler& handler) {
    if (!std::isfinite(deltaTime) || deltaTime < 0.0F) {
        throw std::invalid_argument("StageDirector deltaTime must be nonnegative and finite");
    }
    elapsed_ += deltaTime;
    while (nextEvent_ < events_.size()) {
        const StageEvent& event = events_[nextEvent_];
        float anchor = 0.0F;
        if (states_[nextEvent_].dependencyIndex) {
            const auto& dependency = states_[*states_[nextEvent_].dependencyIndex];
            if (!dependency.endedAt) break;
            anchor = *dependency.endedAt;
        } else if (nextEvent_ > 0) {
            anchor = *states_[nextEvent_ - 1].startedAt;
        }
        if (elapsed_ < anchor + event.time) break;

        const std::size_t startedIndex = nextEvent_;
        states_[startedIndex].startedAt = elapsed_;
        ++nextEvent_;
        if (handler(event)) states_[startedIndex].endedAt = elapsed_;
    }
}

bool StageDirector::EndEvent(StageEventId id) {
    const auto index = FindEvent(id);
    if (!index || !states_[*index].startedAt || states_[*index].endedAt) {
        return false;
    }
    states_[*index].endedAt = elapsed_;
    return true;
}

std::optional<std::size_t> StageDirector::FindEvent(StageEventId id) const {
    for (std::size_t index = 0; index < events_.size(); ++index) {
        if (events_[index].id == id) return index;
    }
    return std::nullopt;
}

}  // namespace redmist
