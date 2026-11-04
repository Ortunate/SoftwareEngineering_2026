// 本文件提供 Bomb 的世界固定网格与原生贴图绘制，不借用或修改相机、不推进战斗时钟。
#pragma once
#include "gameplay/effects/player_bomb.hpp"
namespace koumakyou {
/// @brief 返回圆台在轴向 t（0～1）、周向 angle（弧度）的表面点；与碰撞共用半径线性插值。
/// shellScale 为体积内部的光壳比例 (0,1]；结果不随观察相机改变。
[[nodiscard]] Vector3 BombBeamSurface(const BombVolume& volume,float t,float angle,float shellScale=1);
/// @brief 在 BeginMode3D/透明批次内绘制三维实体；from/to 为沿整束轴线的切片区间，其他形状忽略。
/// 外部几何使用原生 ANM 颜色、Alpha 和贴图，束内流光读取其淡出 Alpha；调用方负责排序和恢复剔除状态。
/// 绘制前须关闭深度写入，几何不会改动碰撞体积。
/// interiorBeam 表示镜头位于巨束出口沿轴观看：绘制内部纵向流光并保留中央视野，物理判定不变。
void DrawBombGeometry(Texture2D texture,const anm::Archive& archive,const PlayerBombSprite& sprite,
                      float from=0,float to=1,bool interiorBeam=false);
} // namespace koumakyou
