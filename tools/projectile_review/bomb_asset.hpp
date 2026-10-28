// 本文件定义 Bomb 资产预览的参数与生命周期；不持有模型、不结算伤害。
#pragma once
#include "raylib.h"
#include <cmath>
#include <stdexcept>
namespace review {
inline constexpr float BombDurations[]{5,140/60.F,250/60.F,5};
// kind=封印 0、星群 2、光炮 3；编号 1 的封魔阵已取消建模，palette=-1 为混色，其余为原图层色号。
// 位置和圆台尺寸为世界单位，方向为单位向量，scale>0；seconds 为非负秒。
// 颜色为 RGB 乘数 [0,1]，opacity [0,1]，strength [0,2]；拖影和外壳不进入碰撞。
struct BombAssetInstance {int kind=0,palette=-1;Vector3 position{},direction{0,1,0},color{1,1,1};float seconds=2,length=4,nearRadius=.3F,farRadius=1.05F,scale=1,opacity=1,strength=1;bool combination=false;};
/// @brief 无副作用的参数验证；非法输入抛 invalid_argument。
inline void ValidateBomb(const BombAssetInstance&p){
 if(p.kind<0||p.kind>3||p.kind==1||p.palette< -1||p.palette>3)throw std::invalid_argument("Bomb kind/palette");
 for(float v:{p.position.x,p.position.y,p.position.z,p.direction.x,p.direction.y,p.direction.z,p.color.x,p.color.y,p.color.z,p.seconds,p.length,p.nearRadius,p.farRadius,p.scale,p.opacity,p.strength})if(!std::isfinite(v))throw std::invalid_argument("Bomb nonfinite");
 float n=p.direction.x*p.direction.x+p.direction.y*p.direction.y+p.direction.z*p.direction.z;
 if(std::abs(n-1)>1e-4F||p.color.x<0||p.color.x>1||p.color.y<0||p.color.y>1||p.color.z<0||p.color.z>1||p.seconds<0||p.length<=0||p.nearRadius<=0||p.farRadius<=0||p.scale<=0||p.opacity<0||p.opacity>1||p.strength<0||p.strength>2)throw std::invalid_argument("Bomb parameter range");
}
}
