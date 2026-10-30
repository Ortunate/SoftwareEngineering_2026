// 本文件定义传入模型的显现变换；只影响显示，不控制运动或碰撞。
#pragma once
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace koumakyou::projectiles {
// position 为世界位置，rotation 为单位四元数；scale 是模型的正常统一缩放。
// age>=0、duration>0，单位为秒；startMultiplier>=endMultiplier>0。
// opacity [0,1]；tint 为颜色乘数。projectileMask 仅用于本仓库普通敌弹 COLOR_0 约定。
struct SpawnParams {Vector3 position{};Quaternion rotation{0,0,0,1};float scale=1,age=0,duration=.65F,startMultiplier=2.4F,endMultiplier=1,opacity=1;Color tint=WHITE;bool projectileMask=false;};
struct EffectSample {float scale,alpha;bool visible;};
/// @brief 非法或非有限参数抛 invalid_argument；透明度由高到低，即 alpha 从 0 增至 opacity。
/// 到期后保持最终大小和 alpha，调用方可继续绘制或切回正常模型绘制。
inline EffectSample SampleEffect(const SpawnParams&p){
 for(float v:{p.position.x,p.position.y,p.position.z,p.rotation.x,p.rotation.y,p.rotation.z,p.rotation.w,p.scale,p.age,p.duration,p.startMultiplier,p.endMultiplier,p.opacity})if(!std::isfinite(v))throw std::invalid_argument("Spawn nonfinite");
 float norm=p.rotation.x*p.rotation.x+p.rotation.y*p.rotation.y+p.rotation.z*p.rotation.z+p.rotation.w*p.rotation.w;
 if(std::abs(norm-1)>1e-4F||p.scale<=0||p.duration<=0||p.age<0||p.endMultiplier<=0||p.startMultiplier<p.endMultiplier||p.opacity<0||p.opacity>1)throw std::invalid_argument("Spawn range");
 float t=std::clamp(p.age/p.duration,0.F,1.F),smooth=t*t*(3-2*t);
 return {p.scale*(p.startMultiplier+(p.endMultiplier-p.startMultiplier)*smooth),smooth*p.opacity,smooth>0&&p.opacity>0&&p.tint.a>0};
}
}
