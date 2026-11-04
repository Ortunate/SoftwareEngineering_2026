// 本文件声明二面静态时间轴、素材及两位自机的对话；所有返回值均借用程序生命周期存储。
#pragma once
#include "stage/content.hpp"
namespace koumakyou::stage2 {
/// @brief 包含 464 次普通出生与四个 Boss/对话事件；按源事件顺序排列，数据不转移所有权。
std::span<const StageEvent> Stage2Timeline();
/// @brief 完整二面素材清单；字符串/span 须覆盖整个异步加载及关卡运行期。
const StageAssetManifest& Stage2Manifest();
/// @brief marisa 区分自机角色，两个御札共用对白；afterBattle 区分战前/战后。
std::span<const DialogueLine> Stage2Dialogue(bool afterBattle,bool marisa);
std::span<const DialogueCue> Stage2DialogueCues(bool afterBattle,bool marisa);
}
