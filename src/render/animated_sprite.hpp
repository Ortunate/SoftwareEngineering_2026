// 本文件提供原始动画帧的 2D、3D 公告板与三维激光绘制接口，不推进动画或拥有素材。
#pragma once
#include "resources/animation.hpp"
namespace koumakyou {
/// @brief 返回公告板四角的世界坐标，顺序为左上、右上、右下、左下；不访问 GPU。
/// 参数单位与 DrawAnimatedBillboard 相同，供透明排序复用实际变换；无精灵高度时返回四个 position。
[[nodiscard]] std::array<Vector3,4> AnimatedBillboardVertices(const Camera3D& camera,const anm::Frame& frame,
                          Vector3 position,float height,Vector2 pivot={0.5F,0.5F},Vector3 motion={});
/// @brief 在 BeginMode3D 内绘制当前帧；height 为未缩放世界高度，pivot 为判定点的归一化位置。
/// 三维接口会关闭背面剔除；调用方在整个批次绘制完毕后 flush 并恢复剔除状态。
/// motion 用于 autoRotate，原像素位移按 height/source.height 换算；支持 XYZ 旋转、镜像、UV 与混合。
void DrawAnimatedBillboard(const Camera3D& camera,Texture2D texture,const anm::Archive& archive,
                          const anm::Frame& frame,Vector3 position,float height,Vector2 pivot={0.5F,0.5F},
                          Color tint=WHITE,Vector3 motion={});
/// @brief 在 2D 模式按原逻辑像素比例及 XYZ 旋转的正交投影绘制，position 是中心或原 ANM 左上锚点；scale 为像素倍率。
void DrawAnimatedSprite2D(Texture2D texture,const anm::Archive& archive,const anm::Frame& frame,
                          Vector2 position,float scale=1,Color tint=WHITE);
/// @brief 绘制沿 origin/end 的相机朝向激光带；width 为世界全宽，不改变线段胶囊判定；与公告板相同，关闭背面剔除。
void DrawAnimatedBeam(const Camera3D& camera,Texture2D texture,const anm::Archive& archive,
                      const anm::Frame& frame,Vector3 origin,Vector3 end,float width,Color tint=WHITE);
}
