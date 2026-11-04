// 本文件负责输入界面和战斗呈现共用的固定参数。

#pragma once

#include <array>

namespace koumakyou {
inline constexpr float kSpellCutInDuration = 2.0F;
// 中文操作名与 kBindingMembers 的持久化成员顺序一一对应，显示翻译不得改变键位编号。
inline constexpr std::array<const char*, 10> kBindingNames{
    "前进", "后退", "向左移动", "向右移动", "上升",
    "下降", "低速移动", "射击", "使用符卡", "暂停",
};
}  // namespace koumakyou
