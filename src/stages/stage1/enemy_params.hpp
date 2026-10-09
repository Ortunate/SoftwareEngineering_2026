// 本文件负责定义第一面各类小怪的出生位置与显式移动参数。

#pragma once

#include "raylib.h"

namespace redmist::stage1 {

/// @brief 普通妖精参数；position 为有限的世界坐标，movement 必须为有效枚举值。
/// 每个实例从自身出生时刻起算轨迹，不再通过编队或编号选择运动方式。
struct FairyParams {
    enum class Movement { Straight, Helix, CrossFromLeft, CrossFromRight };
    Vector3 position{0.0F, 0.0F, 8.5F};
    Movement movement = Movement::Straight;
};

/// @brief 暗色妖精参数；position 为有限的世界坐标，仅沿 Z 轴往返并发射一次球面弹幕。
struct DarkFairyParams {
    Vector3 position{0.0F, 0.0F, 8.5F};
    /// @brief 发弹后原地等待的非负有限秒数；之后沿 +Z 返回出生位置并退场。
    float retreatDelay = 1.0F;
};

/// @brief 光球参数；position 为有限的世界坐标，movement 必须为有效枚举值。
/// CrossArena 按出生位置 X 的正负选择横穿方向，X 为零时向负方向移动。
struct OrbParams {
    enum class Movement { FloatForward, OrbitForward, CrossArena };
    Vector3 position{0.0F, 0.0F, 8.5F};
    Movement movement = Movement::FloatForward;
};

/// @brief 新典逐只出生配置；position 为世界坐标，sub 为 0…4，HP/score 保留原作单位。
/// randomX 在生成瞬间确定横坐标；seed 为实例独立的可重放种子；mirrored 反转 X 轴位移。
struct ReferenceEnemyParams {
    Vector3 position{};
    int sub = 0;
    bool mirrored = false, randomX = false;
    float sourceHealth = 32;
    int sourceScore = 300;
    unsigned seed = 1;
    int sourceItem = -1; // 原作 -1 为全关共享轮换掉落，0 为固定小 P 点。
};
}  // namespace redmist::stage1
