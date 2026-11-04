// 本文件声明五面静态事件、对白和素材，借用存储覆盖关卡生命周期。
#pragma once
#include "stage/content.hpp"
namespace koumakyou::stage5 {
/// @brief 保存 227 次普通出生、两场 Boss、三段对白和两次开战通知的显式表。
std::span<const StageEvent> Stage5Timeline();
/// @brief 素材由关卡资源管理器拥有，事件/动画游标须先销毁再卸载。
const StageAssetManifest& Stage5Manifest();
/// @brief 按场景、战前/战后与角色选择已展开 MSG 的静态对白和提示，时间单位为 60 Hz 内容帧。
std::span<const DialogueLine> Stage5Dialogue(DialogueScene scene,bool afterBattle,bool marisa);
std::span<const DialogueCue> Stage5DialogueCues(DialogueScene scene,bool afterBattle,bool marisa);
}
