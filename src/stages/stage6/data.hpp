// 本文件声明六面静态事件、对白和素材，借用存储覆盖关卡生命周期。
#pragma once
#include "stage/content.hpp"
namespace koumakyou::stage6 {
/// @brief 保存 124 次普通出生、两场 Boss、三段演出、三次恢复通知和三次时间轴等待的显式表。
std::span<const StageEvent> Stage6Timeline();
/// @brief 素材由关卡资源管理器拥有，事件/动画游标须先销毁再卸载。
const StageAssetManifest& Stage6Manifest();
/// @brief 按场景、战前/战后与角色选择已展开 MSG 的静态对白和提示，时间单位为 60 Hz 内容帧。
std::span<const DialogueLine> Stage6Dialogue(DialogueScene scene,bool afterBattle,bool marisa);
std::span<const DialogueCue> Stage6DialogueCues(DialogueScene scene,bool afterBattle,bool marisa);
}
