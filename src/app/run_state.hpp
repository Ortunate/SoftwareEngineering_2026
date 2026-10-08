// 本文件负责定义游戏菜单、战斗和结算状态。

#pragma once

#include "raylib.h"
#include <cstdint>

namespace redmist {

/// @brief 游戏的菜单、战斗及结算状态。
enum class RunState : std::uint8_t {
    MainMenu,
    Options,
    Playing,
    // 战斗已结束，仅播放退场；关卡资源须保留到效果池清空。
    Finishing,
    Paused,
    Won,
    GameOver
};
}  // namespace redmist
