// 本文件定义 Bomb 共用的三维攻击体积；碰撞和渲染读取同一份世界几何，不依赖相机朝向。
#pragma once
#include "raylib.h"
namespace koumakyou {
/// @brief 所有长度均为世界单位；Sphere 使用 origin/radius，Slab 使用 origin/halfExtents。
/// Frustum 为 origin 到 end 的有限圆台，端面半径为 radius/endRadius，端面也参与接触。
/// Slab 沿世界 XYZ 轴排列；半尺寸和半径非负，圆台轴长必须大于零。
struct BombVolume {
    enum class Shape { Sphere, Slab, Frustum } shape=Shape::Sphere;
    Vector3 origin{},end{},halfExtents{};
    float radius=0,endRadius=0;
    /// @brief 点到实体的最短距离平方，实体内部为零；无图形资源访问。
    [[nodiscard]] float DistanceSquared(Vector3 point) const;
    /// @brief 目标球或胶囊与实体接触；targetRadius 为非负世界半径，清弹可加原作额外容差。
    [[nodiscard]] bool IntersectsSphere(Vector3 center,float targetRadius) const;
    [[nodiscard]] bool IntersectsCapsule(Vector3 start,Vector3 finish,float targetRadius) const;
};
} // namespace koumakyou
