// 本文件负责游戏循环各模块共享的轻量三维向量运算。

#pragma once

#include "gameplay/bullets/system.hpp"

namespace redmist {
namespace game_math {
/// @brief 对两个三维向量逐分量相加。
/// @param left 左操作数。
/// @param right 右操作数。
/// @return 相加后的向量。
inline Vector3 Add(Vector3 left, Vector3 right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

/// @brief 计算两个三维向量的差。
/// @param left 被减向量。
/// @param right 减去的向量。
/// @return left - right。
inline Vector3 Subtract(Vector3 left, Vector3 right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

/// @brief 将三维向量乘以标量。
/// @param vector 原向量。
/// @param scale 缩放系数。
/// @return 缩放后的向量。
inline Vector3 Scale(Vector3 vector, float scale) {
    return {vector.x * scale, vector.y * scale, vector.z * scale};
}

/// @brief 计算两点的距离平方，供球形碰撞判定使用。
/// @param left 第一个位置。
/// @param right 第二个位置。
/// @return 两点距离的平方。
inline float DistanceSquared(Vector3 left, Vector3 right) {
    const Vector3 difference = Subtract(left, right);
    return difference.x * difference.x + difference.y * difference.y +
           difference.z * difference.z;
}

/// @brief 对移动方向归一化，零向量保持为零。
/// @param vector 原向量。
/// @return 长度为一或零的向量。
inline Vector3 Normalize(Vector3 vector) {
    const float length = BulletSystem::Length(vector);
    if (length < 0.00001F) {
        return {};
    }
    return Scale(vector, 1.0F / length);
}

 }  // namespace game_math
}  // namespace redmist
