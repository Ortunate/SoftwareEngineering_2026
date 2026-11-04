// 本文件负责定义游戏菜单、战斗和结算状态。

#pragma once

#include "raylib.h"
#include <cstdint>

namespace koumakyou {

/// @brief 游戏的菜单、战斗及结算状态。
enum class RunState : std::uint8_t {
    MainMenu,
    ReplayList, // 本地录像目录，不加载关卡素材。
    Options,
    StartSetup,
    MusicRoom,
    Score,
    Credit,
    Loading, // 加载页只推进界面；素材全部就绪后才重置并启动战斗时钟。
    Playing,
    StageClear, // 关卡奖励明细等待确认或 10 秒，后续流程和战斗时钟暂停。
    // 战斗已结束，仅播放退场；关卡资源须保留到效果池清空。
    Finishing,
    Ending, // 主线结局使用自己的图片/音乐和 60 Hz 演出时钟，结束后进入成绩录入。
    ScoreSave, // 展示本轮成绩及前十排名，选择保存或放弃。
    ScoreName, // 八字符命名；保存失败保留输入，不离开当前页面。
    Paused,
    Won,
    GameOver
};
}  // namespace koumakyou
