// 本文件负责输入界面和战斗呈现共用的固定参数。

#pragma once

#include <array>

namespace redmist {
inline constexpr float kSpellCutInDuration = 2.6F;
inline constexpr std::array<const char*, 10> kBindingNames{
    "FORWARD", "BACKWARD", "MOVE LEFT", "MOVE RIGHT", "ASCEND",
    "DESCEND", "FOCUS / SLOW", "FIRE", "BOMB", "PAUSE",
};
}  // namespace redmist
