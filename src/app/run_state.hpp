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
    Paused,
    Won,
    GameOver
};
}  // namespace redmist
