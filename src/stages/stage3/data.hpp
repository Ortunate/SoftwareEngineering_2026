// 本文件声明三面静态事件、素材及分遭遇对白；返回存储覆盖程序生命周期。
#pragma once
#include "stage/content.hpp"
namespace koumakyou::stage3 {
/// @brief 437 次普通出生、两场遭遇、三段对白和一次开战通知；源时钟等待由显式依赖表达。
std::span<const StageEvent> Stage3Timeline();
/// @brief 借用本关独立素材清单，不转移其字符串或动画所有权。
const StageAssetManifest& Stage3Manifest();
/// @brief 两种御札共用对应自机对白；scene 区分道中战后和关底前后，不以关卡完成状态推导台词。
std::span<const DialogueLine> Stage3Dialogue(DialogueScene scene,bool afterBattle,bool marisa);
std::span<const DialogueCue> Stage3DialogueCues(DialogueScene scene,bool afterBattle,bool marisa);
}
