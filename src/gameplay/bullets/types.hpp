// 本文件负责定义弹幕归属与对象池中的单颗弹幕。

#pragma once

#include "raylib.h"
#include "core/projectile_pose.hpp"
#include "resources/visual_assets.hpp"
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace koumakyou {

/// @brief 区分玩家弹与敌弹，用于碰撞、清弹和计分。
enum class BulletOwner : std::uint8_t { Player, Enemy };
/// @brief 自机弹的命中特性；普通弹命中消失，导弹切换爆破，激光与爆破按经过秒数持续伤害。
enum class PlayerShotKind : std::uint8_t { Projectile, Missile, Laser, Explosion };
/// @brief 对象池中的单颗弹幕；位置和半径采用世界单位，速度为世界单位/秒，加速度为世界单位/秒²。
/// 普通弹 position 为模型原点或旧球形判定中心；光束 position 为起点，Direction() 为单位束轴。
/// beamLength > 0 时采用线段胶囊判定，预警期不造成接触；active=false 后槽位可复用。
/// age 和 maxAge 为秒；rotationAxis 是旋转轴，angularVelocity 为弧度/秒。
/// homingTurnRate 为追踪转向上限（弧度/秒），零表示不追踪；damage 是原始 HP 单位的单次命中基准。
struct Bullet {
    bool active = false;
    bool grazed = false;
    BulletOwner owner = BulletOwner::Enemy;
    VisualId visual = kInvalidVisualId;
    battle::Cursor animation; // 对象池复用时重置，借用当前关卡 CPU 档案。
    int nativeType = -1, nativeColor = 0; // 原 ECL 弹种 0–9、色号 0–15；负值沿用外观绑定。
    Vector3 position{};
    /// @brief 世界速度（单位/秒）与单位朝向只读访问；零速度保留最后朝向。
    Vector3 Velocity() const { return velocity_; }
    Vector3 Direction() const { return direction_; }
    /// @brief 正常危险几何的连续姿态：规范本地 +Y 对齐 Direction，+Z 为参考上方向。
    Quaternion Orientation() const { return orientation_; }
    /// @brief 非零有限速度同步方向；非法输入抛 invalid_argument，不改变原状态。
    void SetVelocity(Vector3 value) {
        const double length=std::hypot(double(value.x),double(value.y),double(value.z));
        if(!std::isfinite(length))throw std::invalid_argument("Nonfinite bullet velocity");
        velocity_=value;
        if(length>0){direction_={float(value.x/length),float(value.y/length),float(value.z/length)};
            orientation_=ProjectilePoseBuilder::BuildPose(ModelDirection{},direction_,{},1,orientationInitialized_?&orientation_:nullptr).rotation;orientationInitialized_=true;}
    }
    /// @brief 仅静止对象可指定初始朝向或束轴；非零有限向量自动归一化，否则抛异常。
    void SetInitialDirection(Vector3 value) {
        const double length=std::hypot(double(value.x),double(value.y),double(value.z));
        if(!std::isfinite(length)||length==0||velocity_.x!=0||velocity_.y!=0||velocity_.z!=0)
            throw std::invalid_argument("Invalid stationary bullet direction");
        direction_={float(value.x/length),float(value.y/length),float(value.z/length)};
        orientation_=ProjectilePoseBuilder::BuildPose(ModelDirection{},direction_,{},1,orientationInitialized_?&orientation_:nullptr).rotation;orientationInitialized_=true;
    }
    /// @brief 固定激光束轴入口；与普通弹共用方向，不保存第二份 beamDirection。
    void SetBeamDirection(Vector3 value) { SetInitialDirection(value); }
private:
    Quaternion orientation_{0,.7071067812F,.7071067812F,0}; // 初始 +Y→世界 +Z，图案参考上方向朝世界 +Y。
    bool orientationInitialized_=false;
    Vector3 velocity_{};
    Vector3 direction_{0,0,1}; // 静止出生默认 +Z；对象池重置时恢复，不继承旧槽方向。
public:
    Vector3 acceleration{};
    Vector3 rotationAxis{0.0F, 1.0F, 0.0F};
    float radius = 0.16F; // 世界参考半径：光束为实际胶囊半径；普通模型按外观/碰撞参考尺寸换算缩放。
    float age = 0.0F;
    float maxAge = 12.0F;
    float angularVelocity = 0.0F;
    float homingTurnRate = 0.0F;
    float damage = 2.0F;
    std::uint32_t sourceShotRva = 0; // 原生火力通道的来源 RVA，仅在对应 EXE 哈希下有效；手工生成弹为零。
    PlayerShotKind playerShotKind = PlayerShotKind::Projectile;
    // 持续伤害以原始 HP/秒计，damageSeconds 是本次 Update 的有效判定秒数，碰撞总控处理完全部目标后清零。
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
    float warningSeconds = 0.0F; // 光束预警秒数；maxAge 包含预警和生效两段。
    /// @brief 光束的世界终点；普通弹返回其位置。
    Vector3 BeamEnd() const {
        return {position.x + direction_.x * beamLength,
                position.y + direction_.y * beamLength,
                position.z + direction_.z * beamLength};
    }
    /// @brief 点到球心或光束轴线段的距离平方（世界单位²），不扣除判定半径。
    /// 旧球/胶囊与粗筛共用该几何约定，端点参与判定；模型窄相由 ProjectileCollision 处理。
    float DistanceSquaredTo(Vector3 point) const {
        const Vector3 relative{point.x - position.x, point.y - position.y, point.z - position.z};
        const float along = beamLength > 0 ? std::clamp(relative.x * direction_.x +
            relative.y * direction_.y + relative.z * direction_.z, 0.0F, beamLength) : 0;
        const Vector3 offset{relative.x - direction_.x * along,
                             relative.y - direction_.y * along,
                             relative.z - direction_.z * along};
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
