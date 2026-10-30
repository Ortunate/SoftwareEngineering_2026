// 本文件负责定义弹幕归属与对象池中的单颗弹幕。

#pragma once

#include "raylib.h"
#include "core/projectile_pose.hpp"
#include "resources/visual_assets.hpp"
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace koumakyou {

/// @brief 区分玩家弹与敌弹，用于碰撞、清弹和计分。
enum class BulletOwner : std::uint8_t { Player, Enemy };
/// @brief 自机弹的命中特性；普通弹命中消失，导弹切换爆破，激光与爆破按经过秒数持续伤害。
enum class PlayerShotKind : std::uint8_t { Projectile, Missile, Laser, Explosion };
/// @brief 对象池中的单颗弹幕；位置和半径采用世界单位，速度为世界单位/秒，加速度为世界单位/秒²。
/// 普通弹 position 为模型原点或旧球形判定中心；光束 position 为起点，Direction() 为单位束轴。
/// IsBeam() 时采用线段胶囊判定，预警期不造成接触；active=false 后槽位可复用。
/// age 和 maxAge 为秒；rotationAxis 是旋转轴，angularVelocity 为弧度/秒。
/// homingTurnRate 为追踪转向上限（弧度/秒），零表示不追踪；damage 是原始 HP 单位的单次命中基准。
struct Bullet {
    bool active = false;
    std::uint64_t generation=0; // BulletSystem 每次复用分配新代号；仅系统写入，清池不重置系统代号序列。
    bool grazed = false; // 普通弹已消费唯一擦弹；光束仅记录是否曾擦弹，不阻止后续周期。
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
    double accelerationSeconds=1.0e9; // 加速度剩余秒数；用双精度避免2000内容帧累减后多加速一步，重启时不重置视觉年龄。
    Vector3 rotationAxis{0.0F, 1.0F, 0.0F};
    float radius = 0.16F;
    float visualScale = 1; // 原生显示的额外等比缩放；不从判定半径反推贴图大小。
    float age = 0.0F;
    double beamAge = 0; // 光束独立累计秒数，再投影到age；外部改写age时在下次更新同步，避免长寿命光束累加float提早一帧消失。
    // 碰撞在弹池更新后执行，擦弹节拍却采样更新前时钟；不能由更新后的age反推首帧。
    double beamContactAge = 0; // 本次更新前的光束年龄，秒；停时不推进。
    float beamGrazeStartSeconds = 0; // 展开转持续的年龄，秒；独立于可配置的预警时窗。
    std::int64_t lastBeamGrazeFrame = -1; // 持续阶段0起算的60Hz内容帧，槽复用时重置。
    float maxAge = 12.0F;
    float angularVelocity = 0.0F;
    float angularSeconds=1.0e9F; // 旋转速度剩余有效秒数；独立于视觉年龄和显现期。
    float speedAcceleration=0; // 极坐标速度的增量，世界单位/秒²；仅 polarMotion 使用。
    bool polarMotion=false,sampleMotionAtStart=false;
    double motionClock=0; // 显现结束后独立累计的秒数，避免以动画年龄或更新次数推导运动时序。
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
    float beamLength = 0.0F; // 当前线段长度，世界单位；伸长型光束刚生成时可以为零。
    float beamLengthLimit=0,beamTravelSpeed=0,beamEndDistance=0; // 非零上限启用伸长型光束；末端距基点的世界距离独立累计。
    /// @brief 伸长型光束的零长度初始态仍是光束，不能作为球形弹参与清弹或选择动画。
    bool IsBeam() const {return beamLength>0||beamLengthLimit>0;}
    Vector3 beamAnchor{}; // 可控光束的世界基点；position=beamAnchor+Direction()*beamStartOffset。
    float beamStartOffset=0; // 世界单位；旋转时保留基点与起始空隙，不以 position 本身作为轴心。
    int beamAnimationScript=-1; // 当前光束图集内的可选动画脚本；负值沿用素材清单，不影响自机激光。
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
    bool turnAbsolute=false;
    int turnCount=1,turnCompleted=0;
    Vector3 turnFixedDirection{}; // 绝对转向的世界单位方向；空间模式保留原高度带，不压回 X/Z 平面。
    int bouncesRemaining=0;
    float bounceSpeed=0; // 生成时保存的反弹后速率，不从之后的启动加速速度反推。
    bool reflectLowerZ=true;
    Vector3 bounceLower{},bounceUpper{},bounceHalfExtents{}; // 世界边界和逻辑半尺寸，与球形碰撞半径无关。
    Vector3 baseVelocity{};
    Vector3 launchDirection{}; // 三维发射的单位方向；零表示旧模式从 baseVelocity 推导，避免零初速弹丢失发射方向。
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
    /// @brief 在调用方确认危险时窗、未命中且位于擦弹范围后，消费一次擦弹资格。
    /// 普通弹只消费一次；光束在持续阶段每12内容帧可消费一次，重复调用和120Hz半帧不会重复结算。
    bool ConsumeGraze() {
        if (!IsBeam()) {
            if (grazed) return false;
        } else {
            // 与Update的外部改龄契约一致；出生后未更新的光束可直接检查第0帧。
            const double sample = age != static_cast<float>(beamAge) ? age : beamContactAge;
            double frame = (sample - beamGrazeStartSeconds) * 60;
            // 秒数接口及固定步长为float；仅消除整帧附近的表示误差，不把真实半帧提前到下一帧。
            const double nearest = std::round(frame);
            const double rounding = 4 * std::numeric_limits<float>::epsilon() *
                (std::abs(sample * 60) + std::abs(beamGrazeStartSeconds * 60) + 1);
            if (std::abs(frame - nearest) <= rounding) frame = nearest;
            if (!std::isfinite(frame) || frame < 0 || frame >= static_cast<double>(INT64_MAX)) return false;
            const auto contentFrame = static_cast<std::int64_t>(std::floor(frame));
            if (contentFrame % 12 != 0 || contentFrame == lastBeamGrazeFrame) return false;
            lastBeamGrazeFrame = contentFrame;
        }
        grazed = true;
        return true;
    }
    Color color = WHITE;
};

}  // namespace koumakyou
