// 本文件声明Extra六段命名对白及其实际结束/结算边界；条件后继由关卡流程选择，不在对白表猜测模式。
#pragma once
#include "stage/content.hpp"
namespace koumakyou::stage_extra {
/// @brief 独立命名主战、道中和条件后继的演出；MainAfter只用于未进入后继的正常战后分支。
/// EncoreInterlude保留源无台词清字/换曲/放行演出，不构造占位台词。
enum class DialogueSection { MainBefore, MainAfter, Midboss, EncoreBefore, EncoreAfter, EncoreInterlude };
inline constexpr std::array<std::string_view,2> kFlandreIntroduction{"恶魔的妹妹","芙兰朵露・斯卡蕾特"};
/// @brief 静态对白借用覆盖整个关卡；所有帧为含wait冻结后的60 Hz实际播放帧。
/// clearFrame<0表示不请求结算，非负时在该帧请求，endFrame仍可能更晚，不能提前结束后续保护等待。
/// 主战战后条件须在选择本表前求值；本表不实现条件判定、关卡跳转或Boss创建。
struct DialogueScript {
    std::span<const DialogueLine> lines;
    std::span<const DialogueCue> cues;
    int endFrame,clearFrame=-1;
};
/// @brief 按真实机体角色选择静态原文与演出；非法section/shot抛invalid_argument，不降级为其他分支。
[[nodiscard]] const DialogueScript& ExtraDialogue(DialogueSection section,ShotType shot);
}
