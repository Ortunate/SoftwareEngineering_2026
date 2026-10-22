// 本文件负责定义弹幕归属与对象池中的单颗弹幕。

#pragma once

#include "raylib.h"
#include "resources/visual_assets.hpp"
#include <cstdint>
#include <algorithm>

namespace koumakyou {

/// @brief 区分玩家弹与敌弹，用于碰撞、清弹和计分。
enum class BulletOwner : std::uint8_t { Player, Enemy };
/// @brief 自机弹的命中特性；普通弹命中消失，导弹切换爆破，激光与爆破按经过秒数持续伤害。
enum class PlayerShotKind : std::uint8_t { Projectile, Missile, Laser, Explosion };
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
    battle::Cursor animation; // 对象池复用时重置，借用当前关卡 CPU 档案。
    int nativeType = -1, nativeColor = 0; // 原 ECL 弹种 0–9、色号 0–15；负值沿用外观绑定。
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
    PlayerShotKind playerShotKind = PlayerShotKind::Projectile;
    // 持续伤害以点/秒计，damageSeconds 是本次 Update 的有效判定秒数，碰撞总控处理完全部目标后清零。
    float damagePerSecond = 0, damageSeconds = 0;
    double hitScoreRemainder = 0; // 持续弹命中 Boss 的不足 1 分余数；按原始 60 Hz 的每帧 10 分换算，避免 FPS 改变得分。
    bool pendingExpiration = false; // 持续弹跨过寿命终点时保留最后一段有效秒数，碰撞结算后再回收。
    VisualId impactVisual = kInvalidVisualId;
    float explosionRadius = 0; // 导弹命中后的球形爆破半径（世界单位）。
    int playerSlot = -1; // 自机激光所属左右副机槽 0/1；其他弹为 -1。
    std::uint32_t weaponEpoch = 0;
    float laserFadeAt = 0; // 激光启动原生淡出中断的年龄（秒）；由动画同步器分段推进。
    bool laserFadeStarted = false;
    /// @brief 返回本次碰撞结算的伤害；持续伤害需由 BulletSystem::Update 先写入有效秒数。
    float ContactDamage() const { return damagePerSecond>0 ? damagePerSecond*damageSeconds : damage; }
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
    bool Dangerous() const { return active && age >= std::max(warningSeconds,spawnVisualSeconds) && age < dangerEndSeconds; }
    // 新典延迟转向采用先减速、到点后瞬时旋转/重新瞄准；秒和世界单位/秒。
    float turnDelaySeconds = 0, turnAngle = 0, turnSpeed = 0;
    bool turnAimed = false, turnApplied = false;
    Vector3 baseVelocity{};
    float launchBoost = 0, launchBoostSeconds = 0, spawnVisualSeconds = 0;
    float spawnMoveScale = 1; // 10/16/32 帧显现期按原作 1/2、1/2.5、1/3 的速度移动，不参与接触。
    // turnDelaySeconds 与 launchBoostSeconds 从显现结束起算；age 仍从生成起算。
    // 光束以 grow 秒展开、末尾 fade 秒收束；dangerEndSeconds 之前才有碰撞。
    float beamGrowSeconds = 0, beamFadeSeconds = 0, dangerEndSeconds = 1.0e9F;
    /// @brief 原作光束宽度比例；先保持细线，入射前 30 帧展开，退场按 fade 秒收束；不改变判定时窗。
    float BeamVisualScale() const {
        if(beamGrowSeconds<=0)return Dangerous() ? 1 : 0.3F;
        if(age<beamGrowSeconds-0.5F)return 0.075F;
        if(age<beamGrowSeconds)return std::clamp(age/beamGrowSeconds,0.075F,1.0F);
        if(beamFadeSeconds>0 && age>maxAge-beamFadeSeconds)
            return std::clamp((maxAge-age)/beamFadeSeconds,0.0F,1.0F);
        return 1;
    }
    Color color = WHITE;
};

}  // namespace koumakyou
