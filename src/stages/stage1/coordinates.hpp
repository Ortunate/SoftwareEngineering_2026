// 本文件集中定义一面原始像素坐标到世界坐标的投影，战斗数值保持原始 HP 单位。
#pragma once
#include "gameplay/actors/movement.hpp"
namespace koumakyou::stage1 {
inline constexpr float kReferenceScale=20.0F/448.0F;
inline constexpr float kReferenceHealthUnit=1;
/// @brief 将原作像素坐标转换为世界坐标，原作 Y 向下映射世界 -Z，原作 Z 映射世界 -Y。
inline Vector3 ReferencePosition(Vector3 source) { return movement::Space{}.World(source); }
/// @brief 将世界坐标还原为原作逻辑像素，与 ReferencePosition 互为逆映射。
inline Vector3 ReferenceSourcePosition(Vector3 world) { return movement::Space{}.Source(world); }
} // namespace koumakyou::stage1
