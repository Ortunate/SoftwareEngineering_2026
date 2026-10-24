// 本文件负责关卡事件分派，以及按台词恢复双方的对话表情状态。

#include "stage/content.hpp"

#include <cmath>
#include <stdexcept>

namespace koumakyou {

std::array<int, 2> DialogueExpressionsAt(
    std::span<const DialogueLine> lines, std::size_t lineIndex, float elapsed) {
    std::array<int, 2> expressions{};
    // 从台词重建状态，保证跳句、战后独白和截图跳转都不继承上一次对话的表情。
    for (std::size_t index = 0; index < lines.size() && index <= lineIndex; ++index)
        if(index<lineIndex || elapsed+0.00001F>=lines[index].expressionDelay)
            expressions[lines[index].bossSpeaking ? 1 : 0] = lines[index].expression;
    return expressions;
}
bool DialogueFlagAt(std::span<const DialogueCue> cues, DialogueCueKind kind, float seconds, bool initial) {
    for(const auto& cue:cues) {
        if(cue.frame>seconds*60+0.001F)break;
        if(cue.kind==kind)initial=cue.value!=0;
    }
    return initial;
}
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
