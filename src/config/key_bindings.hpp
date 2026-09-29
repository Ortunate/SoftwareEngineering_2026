// 本文件负责定义可配置的玩家操作键位。

#pragma once

#include "raylib.h"

namespace redmist {

/// @brief 可由设置页面修改的战斗按键映射。
struct KeyBindings {
    int forward = KEY_W;
    int backward = KEY_S;
    int moveLeft = KEY_A;
    int moveRight = KEY_D;
    int ascend = KEY_SPACE;
    int descend = KEY_LEFT_SHIFT;
    int focus = KEY_C;
    int fire = KEY_Z;
    int bomb = KEY_X;
    int pause = KEY_P;
};

}  // namespace redmist
