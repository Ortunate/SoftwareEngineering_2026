// 本文件声明四面静态时间轴、素材和台词；所有返回存储须覆盖借用期，不持有运行中对象。
#pragma once
#include "stage/content.hpp"
namespace koumakyou::stage4 {
/// @brief 显式保存 395 次普通出生、两场 Boss、两段对白与开战通知，不在调用时生成事件表。
std::span<const StageEvent> Stage4Timeline();
/// @brief 本关演员、帕秋莉立绘与两首原包音乐；资源由 AssetManager 拥有。
const StageAssetManifest& Stage4Manifest();
/// @brief 两御札共用对应角色台词；afterBattle 区分关底战前/战后，全部时间已展开为内容帧。
std::span<const DialogueLine> Stage4Dialogue(bool afterBattle,bool marisa);
std::span<const DialogueCue> Stage4DialogueCues(bool afterBattle,bool marisa);
}
