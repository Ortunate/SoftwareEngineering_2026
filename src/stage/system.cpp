// 本文件负责实现支持相对延迟与事件结束依赖的关卡时间轴调度。

#include "stage/system.hpp"

#include <cmath>
#include <stdexcept>

namespace redmist {
StageDirector::StageDirector() : StageDirector(std::span<const StageEvent>{}) {}

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

        // 先推进索引再执行回调；回调可以同步报告结束，也可能触发依赖后续事件。
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
