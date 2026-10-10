// 本文件负责提供三维弹幕所需的向量运算、球面与锥面方向生成算法。

#pragma once

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace koumakyou::pattern {

/// @brief 将三维向量按系数缩放。
/// @param value 原向量。
/// @param factor 缩放系数。
/// @return 缩放后的向量。
inline Vector3 Scale(Vector3 value, float factor) {
    return {value.x * factor, value.y * factor, value.z * factor};
}

/// @brief 对两个三维向量逐分量相加。
/// @param a 第一个向量。
/// @param b 第二个向量。
/// @return 相加后的向量。
inline Vector3 Add(Vector3 a, Vector3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

/// @brief 计算三维向量叉积。
/// @param a 第一个向量。
/// @param b 第二个向量。
/// @return 垂直于 a、b 的向量。
inline Vector3 Cross(Vector3 a, Vector3 b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

/// @brief 将向量归一化，零长度时使用正 Z 方向。
/// @param value 待归一化向量。
/// @return 单位方向向量。
inline Vector3 Normalize(Vector3 value) {
    const float length = std::sqrt(value.x * value.x + value.y * value.y +
                                   value.z * value.z);
    if (length < 0.00001F) return {0.0F, 0.0F, 1.0F};
    return Scale(value, 1.0F / length);
}

/// @brief 枚举瞄准锥的中心方向与各环方向，不创建弹幕对象。
/// @tparam Emit 接收 Vector3 方向的回调类型。
/// @param axis 锥体中心轴。
/// @param ringCount 环数。
/// @param bulletsPerRing 每环方向数。
/// @param coneAngle 最大张角（弧度）。
/// @param roll 绕中心轴的旋转角（弧度）。
/// @param emit 逐方向回调。
template <typename Emit>
void ForEachConeDirection(Vector3 axis, int ringCount, int bulletsPerRing,
                          float coneAngle, float roll, Emit&& emit) {
    if (ringCount <= 0 || bulletsPerRing <= 0) return;
    const Vector3 forward = Normalize(axis);
    const Vector3 reference = std::abs(forward.y) < 0.9F
                                  ? Vector3{0.0F, 1.0F, 0.0F}
                                  : Vector3{1.0F, 0.0F, 0.0F};
    const Vector3 right = Normalize(Cross(reference, forward));
    const Vector3 up = Normalize(Cross(forward, right));
    emit(forward);
    for (int ring = 1; ring <= ringCount; ++ring) {
        const float ringAngle = coneAngle * static_cast<float>(ring) /
                                static_cast<float>(ringCount);
        for (int index = 0; index < bulletsPerRing; ++index) {
            const float azimuth = roll + 2.0F * std::numbers::pi_v<float> *
                                             static_cast<float>(index) /
                                             static_cast<float>(bulletsPerRing) +
                                  static_cast<float>(ring) * 0.31F;
            const Vector3 direction = Add(
                Scale(forward, std::cos(ringAngle)),
                Add(Scale(right, std::sin(ringAngle) * std::cos(azimuth)),
                    Scale(up, std::sin(ringAngle) * std::sin(azimuth))));
            emit(Normalize(direction));
        }
    }
}

/// @brief 枚举带开口的三维锥面圆环，不发射中心弹；用于多层边界弹幕。
/// axis 自动归一化，count 为采样数，angle/roll/opening 为弧度；opening 是开口半宽。
/// 开口朝向 roll，所有层使用相同 roll 才能保留贯穿层间的通道。
template <typename Emit>
void ForEachOpenRingDirection(Vector3 axis, int count, float angle, float roll,
                              float opening, Emit&& emit) {
    if (count <= 0) return;
    const Vector3 forward = Normalize(axis);
    const Vector3 reference = std::abs(forward.y) < 0.9F ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
    const Vector3 right = Normalize(Cross(reference, forward));
    const Vector3 up = Normalize(Cross(forward, right));
    for (int index = 0; index < count; ++index) {
        const float offset = 2.0F * std::numbers::pi_v<float> * index / count;
        if (std::min(offset, 2.0F * std::numbers::pi_v<float> - offset) < opening) continue;
        const float azimuth = roll + offset;
        emit(Normalize(Add(Scale(forward, std::cos(angle)),
            Add(Scale(right, std::sin(angle) * std::cos(azimuth)),
                Scale(up, std::sin(angle) * std::sin(azimuth))))));
    }
}

/// @brief 用黄金角在单位球面上近似均匀枚举方向，不创建弹幕对象。
/// @tparam Emit 接收 Vector3 方向的回调类型。
/// @param count 方向数量。
/// @param rotation 绕竖直轴的旋转角（弧度）。
/// @param emit 逐方向回调。
template <typename Emit>
void ForEachSphereDirection(int count, float rotation, Emit&& emit) {
    if (count <= 0) return;
    constexpr float goldenAngle = 2.39996322973F;
    for (int index = 0; index < count; ++index) {
        const float y = 1.0F - 2.0F *
            (static_cast<float>(index) + 0.5F) / static_cast<float>(count);
        const float planarRadius = std::sqrt(std::max(0.0F, 1.0F - y * y));
        const float azimuth = goldenAngle * static_cast<float>(index) + rotation;
        emit(Vector3{planarRadius * std::cos(azimuth), y,
                     planarRadius * std::sin(azimuth)});
    }
}

}  // namespace koumakyou::pattern
