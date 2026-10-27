// 本文件负责实现相对时间轴、连续出怪进度及事件结束依赖调度。

#include "stage/system.hpp"

#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace koumakyou {
StageDirector::StageDirector() : StageDirector(std::span<const StageEvent>{}) {}

StageDirector::StageDirector(std::span<const StageEvent> events)
    : events_(events), states_(events.size()) {
    for (std::size_t index = 0; index < events_.size(); ++index) {
        const StageEvent& event = events_[index];
        if (event.atFrame && (*event.atFrame < 0 || event.afterEndOf))
            throw std::invalid_argument("Invalid absolute StageEvent frame");
        if (!std::isfinite(event.delay) || event.delay < 0.0F) {
            throw std::invalid_argument("StageEvent delay must be nonnegative and finite");
        }
        std::visit([&event](const auto& action) {
            using Action = std::decay_t<decltype(action)>;
            if constexpr (std::is_same_v<Action, SpawnActors>) {
                if (action.count < 1 || !std::isfinite(action.interval) ||
                    action.interval < 0.0F ||
                    !std::isfinite((action.count - 1) * action.interval) ||
                    !behavior::Valid(action.actor)) {
                    throw std::invalid_argument("Invalid SpawnActors parameters");
                }
            } else if constexpr (std::is_same_v<Action, StartBoss>) {
                if (!event.id || (action.encounter != BossEncounter::Midboss &&
                                  action.encounter != BossEncounter::Final)) {
                    throw std::invalid_argument("StartBoss requires an id and valid encounter");
                }
            } else if constexpr (std::is_same_v<Action, StartDialogue>) {
                if (!event.id || (action.scene!=DialogueScene::FinalBoss&&action.scene!=DialogueScene::Midboss)||
                    (action.clearPolicy!=DialogueClearPolicy::ClearEnemiesAndBullets&&action.clearPolicy!=DialogueClearPolicy::PreserveWorld))
                    throw std::invalid_argument("StartDialogue requires an id and valid scene");
            } else if constexpr (std::is_same_v<Action, ResumeBoss>) {
                // 此动作是否拥有可恢复的 Boss，由接收方按当前遭遇生命周期校验。
            } else if constexpr (std::is_same_v<Action, WaitDialogueSignal>) {
                if(!event.id||action.signal<0)throw std::invalid_argument("Dialogue signal wait requires id and nonnegative signal");
            } else if constexpr (std::is_same_v<Action, ShowMessage>) {
                if (!std::isfinite(action.duration) || action.duration < 0.0F) {
                    throw std::invalid_argument("Invalid ShowMessage duration");
                }
            } else {
                static_assert(std::is_same_v<Action, void>, "New StageAction requires validation");
            }
        }, event.action);
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
        state.spawned = 0;
    }
}

void StageDirector::Update(float deltaTime, const EventHandler& handler) {
    if (!std::isfinite(deltaTime) || deltaTime < 0.0F) {
        throw std::invalid_argument("StageDirector deltaTime must be nonnegative and finite");
    }
    elapsed_ += deltaTime;
    // 先补发已启动批次，再检查结束依赖，使最后一次生成与其依赖可在同帧发生。
    for (std::size_t index = 0; index < nextEvent_; ++index) {
        if (!states_[index].endedAt &&
            std::holds_alternative<SpawnActors>(events_[index].action)) {
            UpdateSpawns(index, handler);
        }
    }
    while (nextEvent_ < events_.size()) {
        const StageEvent& event = events_[nextEvent_];
        double anchor = 0;
        if (states_[nextEvent_].dependencyIndex) {
            const auto& dependency = states_[*states_[nextEvent_].dependencyIndex];
            if (!dependency.endedAt) break;
            anchor = *dependency.endedAt;
        } else if (nextEvent_ > 0) {
            anchor = *states_[nextEvent_ - 1].startedAt;
        }
        const double due = event.atFrame ? *event.atFrame / 60.0 : anchor + event.delay;
        if (elapsed_ + 0.00001F < due) break;

        // 先推进索引再执行回调；回调可以同步报告结束，也可能触发依赖后续事件。
        const std::size_t startedIndex = nextEvent_;
        states_[startedIndex].startedAt = elapsed_;
        ++nextEvent_;
        if (std::holds_alternative<SpawnActors>(event.action)) {
            UpdateSpawns(startedIndex, handler);
        } else if (handler(event)) {
            states_[startedIndex].endedAt = elapsed_;
        }
    }
}

void StageDirector::UpdateSpawns(std::size_t index, const EventHandler& handler) {
    const auto& batch = std::get<SpawnActors>(events_[index].action);
    EventState& state = states_[index];
    StageEvent request = events_[index];
    std::get<SpawnActors>(request.action).count = 1;
    // 以批次开始时间计算每次截止时刻；不能重置为“当前时刻 + interval”，否则低帧率会漂移。
    while (!state.endedAt && state.spawned < batch.count &&
           elapsed_ >= *state.startedAt + state.spawned * batch.interval) {
        ++state.spawned;
        handler(request);
    }
    if (state.spawned == batch.count && !state.endedAt) state.endedAt = elapsed_;
}

bool StageDirector::EndEvent(StageEventId id) {
    const auto index = FindEvent(id);
    if (!index || !states_[*index].startedAt || states_[*index].endedAt ||
        std::holds_alternative<SpawnActors>(events_[*index].action)) {
        return false;
    }
    states_[*index].endedAt = elapsed_;
    return true;
}

void StageDirector::CancelEnemySpawns() {
    for (std::size_t index = 0; index < nextEvent_; ++index) {
        if (!states_[index].endedAt &&
            std::holds_alternative<SpawnActors>(events_[index].action)) {
            states_[index].endedAt = elapsed_;
        }
    }
}

std::optional<std::size_t> StageDirector::FindEvent(StageEventId id) const {
    for (std::size_t index = 0; index < events_.size(); ++index) {
        if (events_[index].id == id) return index;
    }
    return std::nullopt;
}

}  // namespace koumakyou
