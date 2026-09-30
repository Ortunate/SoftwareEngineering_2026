// 本文件负责定义敌人提供给道具生成器的掉落结果与概率权重。

#pragma once

#include <cstdint>

namespace redmist {

/// @brief 一次击败敌人可能得到的 P 点和分数点个数，weight 为相对概率权重。
/// 两个个数须非负，weight 为零的结果不会被抽中；空分布表示不掉落。
struct DropOutcome {
    int powerCount = 0;
    int scoreCount = 0;
    std::uint32_t weight = 0;
};

}  // namespace redmist
