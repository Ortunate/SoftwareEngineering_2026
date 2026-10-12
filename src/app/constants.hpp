// 本文件负责输入界面和战斗呈现共用的固定参数。

#pragma once

#include <array>

namespace koumakyou {
inline constexpr float kSpellCutInDuration = 2.0F;
inline constexpr std::array<const char*, 12> kBindingNames{
    "FORWARD", "BACKWARD", "MOVE LEFT", "MOVE RIGHT", "ASCEND",
    "DESCEND", "FOCUS / SLOW", "FIRE", "BOMB", "PAUSE", "FIRE (ALTERNATE)", "BOMB (ALTERNATE)",
};
}  // namespace koumakyou
