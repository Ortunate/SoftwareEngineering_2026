#pragma once

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace redmist::pattern {

inline Vector3 Scale(Vector3 value, float factor) {
    return {value.x * factor, value.y * factor, value.z * factor};
}

inline Vector3 Add(Vector3 a, Vector3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vector3 Cross(Vector3 a, Vector3 b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

inline Vector3 Normalize(Vector3 value) {
    const float length = std::sqrt(value.x * value.x + value.y * value.y +
                                   value.z * value.z);
    if (length < 0.00001F) return {0.0F, 0.0F, 1.0F};
    return Scale(value, 1.0F / length);
}

// These helpers only calculate directions. The concrete enemy skill decides
// which bullets to create and owns their speed, color and timing.
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

}  // namespace redmist::pattern
