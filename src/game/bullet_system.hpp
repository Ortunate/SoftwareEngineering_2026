#pragma once

#include "core/fixed_pool.hpp"
#include "game/game_types.hpp"

#include <cstddef>

namespace redmist {

/// @brief 管理弹幕池中每颗弹幕的创建、运动和回收；图案由技能决定。
class BulletSystem {
public:
    /// @brief 弹幕池可容纳的最大活动弹幕数。
    static constexpr std::size_t kCapacity = 8192;

    /// @brief 清空弹幕池及丢弃计数。
    void Reset();
    /// @brief 按速度、加速度与角速度推进弹幕并清除过期或越界者。
    /// @param deltaTime 本帧经过的秒数。
    void Update(float deltaTime);

    /// @brief 从定长池申请并初始化单颗弹幕。
    /// @param owner 弹幕所属阵营。
    /// @param position 初始判定中心。
    /// @param velocity 初始速度。
    /// @param radius 球形判定半径。
    /// @param color 渲染颜色。
    /// @param visual 渲染形态。
    /// @param acceleration 加速度，默认零向量。
    /// @return 新弹幕指针；容量耗尽时返回 nullptr。
    Bullet* Spawn(BulletOwner owner, Vector3 position, Vector3 velocity,
                  float radius, Color color, VisualId visual,
                  Vector3 acceleration = {});

    /// @brief 清除敌方弹幕，保留玩家弹幕。
    void ClearEnemyBullets();
    /// @brief 获取可修改的弹幕池。
    /// @return 弹幕池引用。
    FixedPool<Bullet, kCapacity>& Pool() { return bullets_; }
    /// @brief 获取只读弹幕池。
    /// @return 弹幕池常量引用。
    const FixedPool<Bullet, kCapacity>& Pool() const { return bullets_; }

    /// @brief 根据视角水平角和仰俯角求朝向。
    /// @param yaw 水平角（弧度）。
    /// @param pitch 仰俯角（弧度）。
    /// @return 单位方向向量。
    static Vector3 DirectionFromYawPitch(float yaw, float pitch);
    /// @brief 计算从一个位置指向另一个位置的单位方向。
    /// @param from 起点。
    /// @param to 终点。
    /// @return 单位方向；两点重合时返回正 Z 方向。
    static Vector3 DirectionTo(Vector3 from, Vector3 to);
    /// @brief 用 Rodrigues 公式使向量绕指定轴旋转。
    /// @param vector 原向量。
    /// @param axis 旋转轴。
    /// @param angle 旋转角（弧度）。
    /// @return 旋转后的向量。
    static Vector3 RotateAroundAxis(Vector3 vector, Vector3 axis, float angle);
    /// @brief 计算三维向量长度。
    /// @param vector 输入向量。
    /// @return 欧氏长度。
    static float Length(Vector3 vector);

private:
    FixedPool<Bullet, kCapacity> bullets_;
};

}  // namespace redmist
