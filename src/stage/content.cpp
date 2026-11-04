// 本文件负责关卡事件分派，以及按台词采样逐行文本和双方的对话表情状态。

#include "stage/content.hpp"

#include <cmath>
#include <stdexcept>

namespace koumakyou {

std::array<DialogueTextRow,2> DialogueTextRowsAt(const DialogueLine& line, float elapsed) {
    std::array<DialogueTextRow,2> rows{};
    if(elapsed+0.00001F<line.textDelay)return rows;
    rows[0]={line.speaker,line.text,line.bossSpeaking};
    if(elapsed+0.00001F>=line.textDelay+line.line2Delay) {
        const auto speaker=line.secondLineSpeaker.value_or(DialogueSpeaker{line.speaker,line.bossSpeaking,line.expression});
        const DialogueTextRow second{speaker.name,line.secondLine,speaker.bossSpeaking};
        rows[line.secondLineReplacesFirst ? 0 : 1]=second;
    }
    return rows;
}

std::array<int, 2> DialogueExpressionsAt(
    std::span<const DialogueLine> lines, std::size_t lineIndex, float elapsed) {
    std::array<int, 2> expressions{};
    // 从台词重建状态，保证跳句、战后独白和截图跳转都不继承上一次对话的表情。
    for (std::size_t index = 0; index < lines.size() && index <= lineIndex; ++index) {
        if(index<lineIndex || elapsed+0.00001F>=lines[index].expressionDelay)
            expressions[lines[index].bossSpeaking ? 1 : 0] = lines[index].expression;
        const auto& line=lines[index];
        if(line.secondLineSpeaker && (index<lineIndex || elapsed+0.00001F>=line.textDelay+line.line2Delay))
            expressions[line.secondLineSpeaker->bossSpeaking ? 1 : 0]=line.secondLineSpeaker->expression;
    }
    return expressions;
}
bool DialogueFlagAt(std::span<const DialogueCue> cues, DialogueCueKind kind, float seconds, bool initial) {
    for(const auto& cue:cues) {
        if(cue.frame>seconds*60+0.001F)break;
        if(cue.kind==kind)initial=cue.value!=0;
    }
    return initial;
}
std::array<int,2> DialogueCueExpressionsAt(std::span<const DialogueCue> cues,float seconds,std::array<int,2> fallback) {
    for(const auto& cue:cues) {
        if(cue.frame>seconds*60+0.001F)break;
        if(cue.kind==DialogueCueKind::Expression&&cue.slot>=0&&cue.slot<2)fallback[cue.slot]=cue.value;
    }
    return fallback;
}
namespace {
// 各重载明确列出动作类型；新增 variant 分支而未补充执行器时必须编译失败。
struct EventExecutor {
    const StageEvent& event;
    StageEventSink& sink;
    const StageContent& content;

    bool operator()(const SpawnActors& action) const {
        if (action.count != 1 || !std::isfinite(action.interval) ||
            action.interval < 0.0F || !behavior::Valid(action.actor)) {
            throw std::invalid_argument("Expected one valid enemy spawn request");
        }
        auto configured=content.ConfigureSpawn(action.actor);
        if(!behavior::Valid(configured))throw std::invalid_argument("Invalid configured actor spawn");
        sink.SpawnStageActor(configured);
        return true;
    }

    bool operator()(const StartBoss& action) const {
        if (!event.id || (!action.battle || !action.battle->create)) {
            throw std::invalid_argument("Invalid StartBoss request");
        }
        sink.BeginStageBoss(*action.battle, *event.id);
        return false;
    }

    bool operator()(const StartDialogue& action) const {
        if (!event.id || (action.scene!=DialogueScene::FinalBoss&&action.scene!=DialogueScene::Midboss&&action.scene!=DialogueScene::EncoreBoss)||
            (action.clearPolicy!=DialogueClearPolicy::ClearEnemiesAndBullets&&action.clearPolicy!=DialogueClearPolicy::PreserveWorld))
            throw std::invalid_argument("StartDialogue requires an id and valid scene");
        sink.BeginStageDialogue(*event.id,action.afterBattle,action.scene,action.clearPolicy);
        return false;
    }
    bool operator()(const StartSceneryTransition&) const { sink.BeginStageSceneryTransition();return true; }
    bool operator()(const ResumeBoss&) const { sink.ResumeStageBoss();return true; }
    bool operator()(const WaitDialogueSignal& action) const {
        if(!event.id||action.signal<0)throw std::invalid_argument("Invalid dialogue signal wait");
        return sink.AwaitStageDialogueSignal(*event.id,action.signal);
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
    return std::visit(EventExecutor{event, sink, *this}, event.action);
}

}  // namespace koumakyou
