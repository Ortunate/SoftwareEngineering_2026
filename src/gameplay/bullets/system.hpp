// 本文件负责声明弹幕对象池及弹幕生成和方向计算接口。

#pragma once

#include "core/fixed_pool.hpp"
#include "gameplay/bullets/types.hpp"

#include <cstddef>
#include <memory>
#include <span>

namespace koumakyou {

/// @brief 管理弹幕池中每颗弹幕的创建、运动和回收；图案由技能决定。
/// 构造时一次性分配固定池，内存不足抛 bad_alloc；不允许复制，生成/更新不分配槽位内存。
class BulletSystem {
public:
    /// @brief 弹幕池可容纳的最大活动弹幕数。
    static constexpr std::size_t kCapacity = 8192;

    /// @brief 清空弹幕池及丢弃计数。
    void Reset();
    /// @brief 按速度、加速度与角速度推进弹幕，追踪弹朝最近目标转向，并清除过期或越界者。
    /// @param deltaTime 本帧经过的秒数。
    /// @param targets 当前可攻击目标的世界坐标；仅在本次调用期间借用。
    /// @param enemyAimTarget 敌弹延迟重新瞄准时的玩家世界坐标，普通弹不使用。
    void Update(float deltaTime, std::span<const Vector3> targets = {}, Vector3 enemyAimTarget = {});

    /// @brief 从定长池申请并初始化单颗弹幕。
    /// @param owner 弹幕所属阵营。
    /// @param position 初始判定中心。
    /// @param velocity 初始速度。
    /// @param radius 旧球形判定或模型兼容参考半径，单位为世界长度；模型窄相读取实际危险几何。
    /// @param color 渲染颜色。
    /// @param visual 当前关卡素材表中的外观 ID。
    /// @param acceleration 加速度，默认零向量。
    /// @return 系统持有的新弹幕指针；容量耗尽或速度非有限时返回 nullptr，指针仅在槽位复用或 Reset 前有效。
    Bullet* Spawn(BulletOwner owner, Vector3 position, Vector3 velocity,
                  float radius, Color color, VisualId visual,
                  Vector3 acceleration = {});
    /// @brief 将活动自机导弹切为 20/60 秒爆破：保留对象槽，速度减半，切换原生爆破外观。
    /// 每个 60 Hz 接触帧伤害为 max(1,floor(原始单发伤害/4))，按有效秒数累计，可命中多个目标。
    /// 无命中外观或并非导弹时仅令弹幕失活；调用后动画游标重置，不继续借用旧动画。
    void ExplodePlayerMissile(Bullet& bullet);

    /// @brief 生成固定三维敌方光束；方向在生成时锁定，预警后才有胶囊碰撞。
    /// direction 必须非零且有限并自动归一化；length/radius 为正世界单位，warning/duration 为非负/正秒数。
    /// @return 池持有的光束；容量不足、尺寸或时间非法时返回 nullptr；生命期与 Spawn 一致。
    Bullet* SpawnBeam(Vector3 origin, Vector3 direction, float length, float radius,
                      float warning, float duration, Color color);

    /// @brief 清除敌方弹幕和光束，保留玩家弹幕。
    void ClearEnemyBullets();
    /// @brief 获取可修改的弹幕池。
    /// @return 弹幕池引用。
    FixedPool<Bullet, kCapacity>& Pool() { return *bullets_; }
    /// @brief 获取只读弹幕池。
    /// @return 弹幕池常量引用。
    const FixedPool<Bullet, kCapacity>& Pool() const { return *bullets_; }

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
    // 独立 ANM 实例增大了单弹状态；固定池放在堆上，避免 Windows 默认线程栈溢出。
    std::unique_ptr<FixedPool<Bullet, kCapacity>> bullets_ =
        std::make_unique<FixedPool<Bullet, kCapacity>>();
};

}  // namespace koumakyou
