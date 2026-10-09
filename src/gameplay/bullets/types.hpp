// 本文件负责定义弹幕归属与对象池中的单颗弹幕。

#pragma once

#include "raylib.h"
#include "resources/visual_assets.hpp"
#include <cstdint>
#include <algorithm>

namespace redmist {

/// @brief 区分玩家弹与敌弹，用于碰撞、清弹和计分。
enum class BulletOwner : std::uint8_t { Player, Enemy };
/// @brief 对象池中的单颗弹幕；位置和半径采用世界单位，速度为世界单位/秒，加速度为世界单位/秒²。
/// 普通弹 position 为球形判定中心；光束 position 为起点，beamDirection 为单位方向。
/// beamLength > 0 时采用线段胶囊判定，预警期不造成接触；active=false 后槽位可复用。
/// age 和 maxAge 为秒；rotationAxis 是旋转轴，angularVelocity 为弧度/秒。
/// homingTurnRate 为追踪转向上限（弧度/秒），零表示不追踪；damage 是单次命中伤害。
struct Bullet {
    bool active = false;
    bool grazed = false;
    BulletOwner owner = BulletOwner::Enemy;
    VisualId visual = kInvalidVisualId;
    Vector3 position{};
    Vector3 velocity{};
    Vector3 acceleration{};
    Vector3 rotationAxis{0.0F, 1.0F, 0.0F};
    float radius = 0.16F;
    float age = 0.0F;
    float maxAge = 12.0F;
    float angularVelocity = 0.0F;
    float homingTurnRate = 0.0F;
    float damage = 2.0F;
    float beamLength = 0.0F; // 世界单位；零表示普通球形弹，正值表示固定光束。
    Vector3 beamDirection{0, 0, 1};
    float warningSeconds = 0.0F; // 光束预警秒数；maxAge 包含预警和生效两段。
    /// @brief 光束的世界终点；普通弹返回其位置。
    Vector3 BeamEnd() const {
        return {position.x + beamDirection.x * beamLength,
                position.y + beamDirection.y * beamLength,
                position.z + beamDirection.z * beamLength};
    }
    /// @brief 点到球心或光束轴线段的距离平方（世界单位²），不扣除判定半径。
    /// 渲染预警、危险提示、命中和擦弹共用该几何约定，端点也参与判定。
    float DistanceSquaredTo(Vector3 point) const {
        const Vector3 relative{point.x - position.x, point.y - position.y, point.z - position.z};
        const float along = beamLength > 0 ? std::clamp(relative.x * beamDirection.x +
            relative.y * beamDirection.y + relative.z * beamDirection.z, 0.0F, beamLength) : 0;
        const Vector3 offset{relative.x - beamDirection.x * along,
                             relative.y - beamDirection.y * along,
                             relative.z - beamDirection.z * along};
        return offset.x * offset.x + offset.y * offset.y + offset.z * offset.z;
    }
    /// @brief 当前是否可命中或擦弹；暂停时 age 不推进，预警期一直无害。
    bool Dangerous() const { return active && age >= warningSeconds; }
    Color color = WHITE;
};

}  // namespace redmist
