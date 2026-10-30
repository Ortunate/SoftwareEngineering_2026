// 本文件仅供旧解释器和差分测试使用，独立将原作像素坐标映射为世界坐标；生产引擎不依赖它。
#pragma once
#include "raylib.h"
namespace koumakyou::stage1 {
inline constexpr float kReferenceScale=20.0F/448.0F;
/// @brief 旧解释器的像素到世界映射；保持旧浮点求值顺序，供独立行为对照。
inline Vector3 ReferencePosition(Vector3 p) { return {(p.x-192)*kReferenceScale,-p.z*kReferenceScale,10-p.y*kReferenceScale}; }
} // namespace koumakyou::stage1
