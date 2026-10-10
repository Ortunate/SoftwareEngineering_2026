// 本文件负责把带参关卡事件分派为游戏总控的单次生成与演出请求。

#include "stage/content.hpp"

#include <cmath>
#include <stdexcept>

namespace koumakyou {
namespace {
// 各重载明确列出动作类型；新增 variant 分支而未补充执行器时必须编译失败。
struct EventExecutor {
    const StageEvent& event;
    StageEventSink& sink;

    bool operator()(const SpawnEnemies& action) const {
        if (action.count != 1 || !std::isfinite(action.interval) ||
            action.interval < 0.0F || !IsValidEnemySpec(action.enemy)) {
            throw std::invalid_argument("Expected one valid enemy spawn request");
        }
        sink.SpawnStageEnemy(action.enemy);
        return true;
    }

    bool operator()(const StartBoss& action) const {
        if (!event.id || (action.encounter != BossEncounter::Midboss &&
                          action.encounter != BossEncounter::Final)) {
            throw std::invalid_argument("Invalid StartBoss request");
        }
        sink.BeginStageBoss(action.encounter == BossEncounter::Final, *event.id);
        return false;
    }

    bool operator()(const StartDialogue&) const {
        if (!event.id) throw std::invalid_argument("StartDialogue requires an id");
        sink.BeginStageDialogue(*event.id, std::get<StartDialogue>(event.action).afterBattle);
        return false;
    }

    bool operator()(const ShowMessage& action) const {
        if (!std::isfinite(action.duration) || action.duration < 0.0F) {
            throw std::invalid_argument("Invalid ShowMessage duration");
        }
        sink.StageMessage(action.text, action.duration);
        return true;
    }
};
}  // namespace

bool StageContent::HandleEvent(const StageEvent& event, StageEventSink& sink) const {
    return std::visit(EventExecutor{event, sink}, event.action);
}

}  // namespace koumakyou
