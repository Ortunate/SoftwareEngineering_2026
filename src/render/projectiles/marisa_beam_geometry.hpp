// 本文件计算魔理沙激光的局内显示胶囊；不修改弹体发射点、碰撞或原生寿命。
#pragma once
#include "gameplay/bullets/types.hpp"
#include "raymath.h"
#include <algorithm>
#include <stdexcept>
namespace koumakyou::projectiles {
/// @brief origin/end 为圆柱段两端的世界坐标，length、radius、haloRadius 均为世界单位。
/// haloRadius 已包含 shader 的 1.5 倍外壳放大；length=0 时没有可绘制区间。
struct MarisaBeamGeometry {Vector3 origin{},end{};float length=0,radius=0,haloRadius=0;};
/// @brief 为已校验的玩家光束计算共享显示范围；tier 必须为 0..2，否则抛 invalid_argument。
/// visualScale 使用当前副机缩放，方向须为单位向量，半径/长度为正；不依赖观察相机。
/// 绘制与透明切分必须共用此结果，不能拿判定起点解释显示区间的归一化参数。
inline MarisaBeamGeometry SampleMarisaBeamGeometry(const Bullet& beam,int tier){
 if(tier<0||tier>2)throw std::invalid_argument("Invalid Marisa beam tier");
 // 副机已缩至原尺寸六成，主体半径取原判定半径的 35%，最宽档主体也能收在星形副机内。
 constexpr float radiusScale=.35F;
 constexpr float haloScale[]{1,1.5F,2};
 MarisaBeamGeometry result;result.radius=beam.radius*beam.visualScale*beam.BeamVisualScale()*radiusScale;
 result.haloRadius=result.radius*1.5F*haloScale[tier];
 // 整个后端帽移到副机前方 0.1 世界单位，避免外壳倒伸进副机或镜头；远端中心保持原射程。
 // 这个间隙不随近距离瞄准缩至零，因此副机贴近镜头时仍保留近裁面余量。
 const float inset=std::min(beam.beamLength,result.haloRadius+.1F);
 result.origin=Vector3Add(beam.position,Vector3Scale(beam.Direction(),inset));result.end=beam.BeamEnd();
 result.length=std::max(0.F,beam.beamLength-inset);return result;
}
}
