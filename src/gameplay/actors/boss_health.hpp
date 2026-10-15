// 本文件定义 Boss 当前一组攻击的血条快照，使界面无需根据阶段编号推测血量或符卡预留区间。
#pragma once
#include <array>
#include <cstddef>

namespace koumakyou {
/// @brief 血条段的攻击类别，只决定显示颜色，不改变伤害、超时或阶段切换。
enum class BossHealthKind { NonSpell, Spell };

/// @brief capacity 和 remaining 均为整条血条的比例；0≤remaining≤capacity≤1。
/// capacity 是固定预留宽度，remaining 是当前填充宽度；切换到符卡后不能重新归一化为满条。
struct BossHealthSegment {
    float capacity = 0;
    float remaining = 0;
    BossHealthKind kind = BossHealthKind::NonSpell;
};

/// @brief 当前组的值快照，最多容纳“非符＋对应符卡”两段；不持有 Boss 或资源引用。
/// segments 按屏幕从左到右排列，count≤2；有效段 capacity 总和为 1，从最右侧攻击开始消耗。
/// remainingBars 是当前组之后剩余的血条组数；count=0 表示隐藏，包括未入场及退场。
struct BossHealthBar {
    std::array<BossHealthSegment, 2> segments{};
    std::size_t count = 0;
    int remainingBars = 0;
};
} // namespace koumakyou
