// 本文件提供已核实的新开局rank初值；不从初始化表推导动态调整规则。
#pragma once

#include "gameplay/data/native_combat.hpp"
#include <stdexcept>

namespace koumakyou::rank {
/// @brief 返回原生五槽表的新开局值；stage采用1…7源关卡编号，Extra为7且仅接受内部Normal上下文。
/// 重开/练习应重新取值，连续切关应保留当前rank；非法身份抛invalid_argument。
/// 本函数只复现初始化写入，不证明动态rank规则或上下限；原生主线切关通过状态3跳过该写入。
[[nodiscard]] constexpr int Initial(int stage,native_combat::Difficulty difficulty) {
    const int index=native_combat::DifficultyIndex(difficulty);
    if(stage<1||stage>7||index<0||(stage==7&&difficulty!=native_combat::Difficulty::Normal))
        throw std::invalid_argument("Invalid initial rank identity");
    return stage==7?18:index==0?20:32;
}
} // namespace koumakyou::rank
