// 本文件为独立 BombVolume 审阅提供与游戏相同的向量长度运算，避免链接对象池、音频和战斗状态。
#include "gameplay/bullets/system.hpp"
#include <cmath>
namespace koumakyou {
float BulletSystem::Length(Vector3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
}
