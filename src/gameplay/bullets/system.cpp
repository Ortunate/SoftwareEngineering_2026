// 本文件负责实现单颗弹幕生成、运动更新与敌弹清除。

#include "gameplay/arena.hpp"
#include "gameplay/bullets/system.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace koumakyou {
namespace {

/// @brief 对两个三维向量逐分量相加。
/// @param left 左操作数。
/// @param right 右操作数。
/// @return 相加后的向量。
Vector3 Add(Vector3 left, Vector3 right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

/// @brief 将三维向量乘以标量。
/// @param vector 原向量。
/// @param scale 缩放系数。
/// @return 缩放后的向量。
Vector3 Scale(Vector3 vector, float scale) {
    return {vector.x * scale, vector.y * scale, vector.z * scale};
}

/// @brief 计算两个向量的点积。
/// @param left 第一个向量。
/// @param right 第二个向量。
/// @return 点积标量。
float Dot(Vector3 left, Vector3 right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

/// @brief 计算两个向量的叉积。
/// @param left 第一个向量。
/// @param right 第二个向量。
/// @return 右手系叉积向量。
Vector3 Cross(Vector3 left, Vector3 right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

/// @brief 将向量归一化，零向量退化为正 Z 方向。
/// @param vector 原向量。
/// @return 单位方向向量。
Vector3 Normalize(Vector3 vector) {
    const float length = BulletSystem::Length(vector);
    if (length < 0.00001F) {
        return {0.0F, 0.0F, 1.0F};
    }
    return Scale(vector, 1.0F / length);
}

/// @brief 以显现结束的零时刻更新新弹道；运动参数的有效期包含启动加速及取消该选项的等待帧。
/// 延迟转向独立于启动加速执行，重复转向以最初时钟的整数倍触发，不能每次累加浮点截止时刻。
void UpdateStartSampledMotion(Bullet& bullet,float delta,float age,Vector3 target) {
    constexpr float epsilon=1.0e-6F;
    const bool boosted=bullet.launchBoostSeconds>0&&age<=bullet.launchBoostSeconds+epsilon;
    const bool removingBoost=bullet.launchBoostSeconds>0&&age<bullet.launchBoostSeconds+2.0F/60-epsilon;
    if(boosted) {
        const float speed=BulletSystem::Length(bullet.baseVelocity)+bullet.launchBoost*
            std::max(0.0F,1-age/bullet.launchBoostSeconds);
        bullet.SetVelocity(Scale(Normalize(BulletSystem::Length(bullet.launchDirection)>0 ? bullet.launchDirection : bullet.baseVelocity),speed));
    } else if(!removingBoost) {
        if(bullet.polarMotion) {
            const float elapsed=std::min(delta,std::max(0.0F,bullet.angularSeconds));
            const float speed=BulletSystem::Length(bullet.baseVelocity)+bullet.speedAcceleration*elapsed;
            const auto direction=BulletSystem::RotateAroundAxis(Normalize(bullet.baseVelocity),bullet.rotationAxis,bullet.angularVelocity*elapsed);
            bullet.baseVelocity=Scale(direction,speed);bullet.SetVelocity(bullet.baseVelocity);
        } else {
            const float elapsed=std::min(delta,static_cast<float>(std::max(0.0,bullet.accelerationSeconds)));
            bullet.SetVelocity(Add(bullet.Velocity(),Scale(bullet.acceleration,elapsed)));
        }
    }
    bullet.angularSeconds=std::max(0.0F,bullet.angularSeconds-delta);
    bullet.accelerationSeconds=std::max(0.0,bullet.accelerationSeconds-delta);
    if(bullet.turnDelaySeconds>0&&!bullet.turnApplied) {
        bool turned=false;
        while(bullet.turnCompleted<bullet.turnCount&&age+epsilon>=bullet.turnDelaySeconds*(bullet.turnCompleted+1)) {
            const auto direction=bullet.turnAbsolute ? bullet.turnFixedDirection :
                BulletSystem::RotateAroundAxis(bullet.turnAimed ? BulletSystem::DirectionTo(bullet.position,target) : Normalize(bullet.baseVelocity),
                    bullet.rotationAxis,bullet.turnAngle);
            bullet.baseVelocity=Scale(direction,bullet.turnSpeed);++bullet.turnCompleted;turned=true;
        }
        bullet.turnApplied=bullet.turnCompleted==bullet.turnCount;
        const float progress=age-bullet.turnDelaySeconds*bullet.turnCompleted;
        bullet.SetVelocity(Scale(bullet.baseVelocity,turned||bullet.turnApplied ? 1 : std::max(0.0F,1-progress/bullet.turnDelaySeconds)));
    }
}
/// @brief 完全越出可见边界后才反弹；同一角落反射多轴仍只消费一次次数，末端离场也消费次数。
void Bounce(Bullet& bullet) {
    const auto p=bullet.position,lo=bullet.bounceLower,hi=bullet.bounceUpper,h=bullet.bounceHalfExtents;
    if(p.x>=lo.x-h.x&&p.x<=hi.x+h.x&&p.y>=lo.y-h.y&&p.y<=hi.y+h.y&&p.z>=lo.z-h.z&&p.z<=hi.z+h.z)return;
    auto direction=Normalize(BulletSystem::Length(bullet.Velocity())>0 ? bullet.Velocity() : bullet.launchDirection);
    if(p.x<lo.x||p.x>=hi.x)direction.x=-direction.x;
    if(p.y<lo.y||p.y>=hi.y)direction.y=-direction.y;
    if(p.z>hi.z||(bullet.reflectLowerZ&&p.z<=lo.z))direction.z=-direction.z;
    bullet.SetVelocity(Scale(direction,bullet.bounceSpeed));bullet.baseVelocity=bullet.Velocity();bullet.launchDirection=direction;
    --bullet.bouncesRemaining;
}

}  // namespace

void BulletSystem::Reset() { bullets_->Clear();enemyClearSeconds_=0; }
BulletSystem::Handle BulletSystem::MakeHandle(Bullet* bullet) const {
    Handle handle;
    if(bullet) {handle.owner_=this;handle.slot_=bullet;handle.generation_=bullet->generation;}
    return handle;
}
Bullet* BulletSystem::Resolve(Handle handle) {
    return handle.owner_==this&&handle.slot_&&handle.slot_->active&&handle.slot_->generation==handle.generation_ ? handle.slot_ : nullptr;
}

void BulletSystem::Update(float deltaTime, std::span<const Vector3> targets, Vector3 enemyAimTarget,bool timeStopped) {
    // 在演员交付本步新弹后清理，且覆盖窗口的最后一个固定步；不能先减时钟再漏掉尾步。
    if(!timeStopped&&enemyClearSeconds_>0) {
        ClearEnemyBullets();
        enemyClearSeconds_=std::max(0.0,enemyClearSeconds_-std::max(0.0F,deltaTime));
        if(enemyClearSeconds_<1e-6)enemyClearSeconds_=0;
    }
    bullets_->ForEachActive([deltaTime, targets, enemyAimTarget,timeStopped](Bullet& bullet) {
        if(bullet.pendingExpiration) {bullet.active=false;return;}
        if(timeStopped) {
            bullet.damageSeconds=bullet.owner==BulletOwner::Player&&bullet.damagePerSecond>0&&bullet.Dangerous() ? deltaTime : 0;
            return;
        }
        const float oldAge=bullet.age;
        if(bullet.IsBeam()) {
            if(bullet.age!=static_cast<float>(bullet.beamAge))bullet.beamAge=bullet.age;
            bullet.beamContactAge=bullet.beamAge;
            bullet.beamAge+=deltaTime;bullet.age=static_cast<float>(bullet.beamAge);
        } else bullet.age += deltaTime;
        // 光束截止值可能由两个float秒数相加得到；只包容该次表示的一ULP，真实跨过下一逻辑步仍立即回收。
        // 原生淡出末帧在演员逻辑中仍可见，之后的弹幕更新才释放，不能以累计float误差吞掉这帧。
        const bool expired=bullet.age>(bullet.IsBeam()?std::nextafter(bullet.maxAge,std::numeric_limits<float>::infinity()):bullet.maxAge);
        // 在本帧内裁剪有效区间，避免两个累计年龄相减产生精度损失，进而漏算伤害及整数得分。
        bullet.damageSeconds=std::max(0.0F,std::min({deltaTime,bullet.maxAge-oldAge,bullet.dangerEndSeconds-oldAge})-
            std::max({0.0F,bullet.warningSeconds-oldAge,bullet.spawnVisualSeconds-oldAge}));
        if(bullet.playerShotKind==PlayerShotKind::Missile) {
            // 原作每帧先增加 0.3 的前向速度再移动；此积分在 60 Hz 与原序列一致，其他帧率保持相同轨迹。
            bullet.position=Add(bullet.position,Add(Scale(bullet.Velocity(),deltaTime),
                Scale(bullet.acceleration,0.5F*deltaTime*(deltaTime+1.0F/60))));
            bullet.SetVelocity(Add(bullet.Velocity(),Scale(bullet.acceleration,deltaTime)));
            if(expired || std::abs(bullet.position.x)>kArenaHalfExtents.x+3 ||
               std::abs(bullet.position.y)>kArenaHalfExtents.y+3 || std::abs(bullet.position.z)>kArenaHalfExtents.z+3)
                bullet.active=false;
            return;
        }
        // age的float累计在整帧边界可能留下数个ULP；仅消除表示残余，不吞掉真实跨界时间。
        const auto remainingStep=[&](float end) {
            float value=std::clamp(end-oldAge,0.0F,deltaTime);
            const float rounding=4*std::numeric_limits<float>::epsilon()*(std::abs(end)+oldAge+deltaTime);
            if(value<=rounding)return 0.0F;
            if(deltaTime-value<=rounding)return deltaTime;
            return value;
        };
        const float spawnDelta=remainingStep(bullet.spawnVisualSeconds);
        bullet.position=Add(bullet.position,Scale(bullet.baseVelocity,spawnDelta*bullet.spawnMoveScale));
        // 原生ANM在创建时执行第0帧；结束更新先做显现位移，再落入零时钟运动。
        // 步首采样路径保留最后一个60Hz内容帧的重叠，120Hz拆成两个半步；旧路径保持既有采样契约。
        const float motionStart=bullet.sampleMotionAtStart&&bullet.spawnVisualSeconds>0 ?
            bullet.spawnVisualSeconds-1.0F/60 : bullet.spawnVisualSeconds;
        const float motionDelta=deltaTime-remainingStep(motionStart);
        const float motionAge=std::max(0.0F,bullet.age-bullet.spawnVisualSeconds);
        const float startAge=static_cast<float>(bullet.motionClock);bullet.motionClock+=motionDelta;
        const bool turning=bullet.turnDelaySeconds>0&&!bullet.turnApplied;
        // 先更新速度，再按新速度推进位置；角速度使弹道在三维空间转弯。
        const bool extendedMotion=bullet.sampleMotionAtStart||bullet.turnCount>1||bullet.turnAbsolute||bullet.speedAcceleration!=0;
        if(extendedMotion&&motionDelta>0)UpdateStartSampledMotion(bullet,motionDelta,bullet.sampleMotionAtStart ? startAge : motionAge,enemyAimTarget);
        else if (!extendedMotion) {
        // 旧关卡保留已验证的步末采样和一次转向路径。
            if (bullet.launchBoostSeconds > 0 && motionAge <= bullet.launchBoostSeconds) {
                const float speed = Length(bullet.baseVelocity) + bullet.launchBoost *
                    (1 - motionAge / bullet.launchBoostSeconds);
                bullet.SetVelocity(Scale(Normalize(Length(bullet.launchDirection)>0 ? bullet.launchDirection : bullet.baseVelocity), speed));
            } else {
                const float accelerationDelta=std::min(motionDelta,static_cast<float>(std::max(0.0,bullet.accelerationSeconds)));
                bullet.SetVelocity(Add(bullet.Velocity(),Scale(bullet.acceleration,accelerationDelta)));
                bullet.accelerationSeconds=std::max(0.0,bullet.accelerationSeconds-accelerationDelta);
            }
            if (bullet.turnDelaySeconds > 0 && !bullet.turnApplied) {
                if (motionAge >= bullet.turnDelaySeconds) {
                    const Vector3 direction = bullet.turnAimed
                        ? DirectionTo(bullet.position, enemyAimTarget) : Normalize(bullet.baseVelocity);
                    bullet.SetVelocity(Scale(RotateAroundAxis(direction, bullet.rotationAxis,
                        bullet.turnAngle), bullet.turnSpeed));
                    bullet.turnApplied = true;
                } else bullet.SetVelocity(Scale(bullet.baseVelocity, 1 - motionAge / bullet.turnDelaySeconds));
            }
            if (bullet.angularVelocity != 0.0F) {
                const float angularDelta=std::min(motionDelta,std::max(0.0F,bullet.angularSeconds));
                bullet.SetVelocity(RotateAroundAxis(
                    bullet.Velocity(), bullet.rotationAxis,
                    bullet.angularVelocity * angularDelta));
                bullet.angularSeconds=std::max(0.0F,bullet.angularSeconds-angularDelta);
            }
        }
        if(bullet.bouncesRemaining>0&&!turning&&motionDelta>0)Bounce(bullet);
        if (bullet.homingTurnRate > 0.0F && !targets.empty()) {
            const Vector3* nearest = nullptr;
            float nearestDistanceSquared = 0.0F;
            for (const Vector3& target : targets) {
                const Vector3 difference{target.x - bullet.position.x,
                                          target.y - bullet.position.y,
                                          target.z - bullet.position.z};
                const float distanceSquared = Dot(difference, difference);
                if (!nearest || distanceSquared < nearestDistanceSquared) {
                    nearest = &target;
                    nearestDistanceSquared = distanceSquared;
                }
            }
            if (nearest && nearestDistanceSquared > 0.0001F) {
                const float speed = Length(bullet.Velocity());
                if (speed > 0.0001F) {
                    const Vector3 current = Scale(bullet.Velocity(), 1.0F / speed);
                    const Vector3 desired = DirectionTo(bullet.position, *nearest);
                    const float angle = std::acos(std::clamp(Dot(current, desired),
                                                              -1.0F, 1.0F));
                    if (angle > 0.0001F) {
                        Vector3 axis = Cross(current, desired);
                        // 反向目标的叉积接近零，改用固定垂直轴避免转向停滞。
                        if (Length(axis) < 0.0001F) {
                            axis = Cross(current, {0.0F, 1.0F, 0.0F});
                            if (Length(axis) < 0.0001F)
                                axis = Cross(current, {1.0F, 0.0F, 0.0F});
                        }
                        const float turn = std::min(angle,
                            bullet.homingTurnRate * motionDelta);
                        bullet.SetVelocity(Scale(RotateAroundAxis(current, axis,
                                                                   turn), speed));
                    }
                }
            }
        }
        bullet.position = Add(bullet.position, Scale(bullet.Velocity(), motionDelta));
        if(bullet.IsBeam())bullet.beamAnchor=Add(bullet.beamAnchor,Scale(bullet.Velocity(),motionDelta));
        if(bullet.beamLengthLimit>0) {
            // 原作先推进末端，超出长度上限后才推进起点；基点不随光束移动。
            bullet.beamEndDistance+=bullet.beamTravelSpeed*motionDelta;
            bullet.beamStartOffset=std::max(bullet.beamStartOffset,bullet.beamEndDistance-bullet.beamLengthLimit);
            bullet.beamLength=bullet.beamEndDistance-bullet.beamStartOffset;
            bullet.position=Add(bullet.beamAnchor,Scale(bullet.Direction(),bullet.beamStartOffset));
        }

        constexpr float margin = 3.0F;
        if (expired && bullet.damagePerSecond>0 && bullet.damageSeconds>0) {
            bullet.pendingExpiration=true;
        } else if (expired ||
            std::abs(bullet.position.x) > kArenaHalfExtents.x + margin ||
            std::abs(bullet.position.y) > kArenaHalfExtents.y + margin ||
            std::abs(bullet.position.z) > kArenaHalfExtents.z + margin) {
            bullet.active = false;
        }
    });
}

void BulletSystem::ExplodePlayerMissile(Bullet& bullet) {
    if(!bullet.active || bullet.owner!=BulletOwner::Player || bullet.playerShotKind!=PlayerShotKind::Missile || bullet.impactVisual==kInvalidVisualId) {
        bullet.active=false;return;
    }
    bullet.playerShotKind=PlayerShotKind::Explosion;
    bullet.visual=bullet.impactVisual;bullet.animation={};
    bullet.radius=bullet.explosionRadius;
    bullet.SetVelocity(Scale(bullet.Velocity(),0.5F));bullet.acceleration={};
    // 原作按整数取四分之一且至少 1 点；生命、单发及持续伤害均采用原始点数。
    bullet.damagePerSecond=std::max(1.0F,std::floor(bullet.damage/4))*60;
    bullet.damage=0;bullet.damageSeconds=0;
    bullet.age=0;bullet.maxAge=20.0F/60;
}

Bullet* BulletSystem::Spawn(BulletOwner owner, Vector3 position,
                            Vector3 velocity, float radius, Color color,
                            VisualId visual, Vector3 acceleration) {
    if(!std::isfinite(velocity.x)||!std::isfinite(velocity.y)||!std::isfinite(velocity.z))return nullptr;
    Bullet* bullet = bullets_->Acquire();
    if (bullet == nullptr) {
        return nullptr;
    }
    bullet->generation=nextGeneration_++;
    bullet->owner = owner;
    bullet->visual = visual;
    bullet->position = position;
    bullet->SetVelocity(velocity);
    bullet->acceleration = acceleration;
    bullet->radius = radius;
    bullet->color = color;
    // 原生普通敌弹没有统一寿命；场内慢弹须保留到离场、命中或清弹，显式限时攻击另设 maxAge。
    bullet->maxAge = owner == BulletOwner::Player ? 3.0F : std::numeric_limits<float>::infinity();
    return bullet;
}

Bullet* BulletSystem::SpawnBeam(Vector3 origin, Vector3 direction, float length,
                                float radius, float warning, float duration, Color color) {
    if (!std::isfinite(length) || !std::isfinite(radius) || !std::isfinite(warning) ||
        !std::isfinite(duration) || !std::isfinite(warning + duration) ||
        !std::isfinite(origin.x + origin.y + origin.z) ||
        !std::isfinite(direction.x + direction.y + direction.z) ||
        std::hypot(double(direction.x),double(direction.y),double(direction.z))==0 ||
        length <= 0 || radius <= 0 || warning < 0 || duration <= 0) return nullptr;
    Bullet* beam = Spawn(BulletOwner::Enemy, origin, {}, radius, color, kInvalidVisualId);
    if (!beam) return nullptr;
    beam->SetBeamDirection(Normalize(direction));
    beam->beamAnchor=origin;
    beam->beamLength = length;
    beam->warningSeconds = warning;
    beam->beamGrazeStartSeconds = warning;
    beam->maxAge = warning + duration;
    return beam;
}

void BulletSystem::ClearEnemyBullets(float durationSeconds) {
    if(!std::isfinite(durationSeconds)||durationSeconds<0)throw std::invalid_argument("Invalid enemy bullet clear duration");
    enemyClearSeconds_=std::max(enemyClearSeconds_,static_cast<double>(durationSeconds));
    bullets_->ForEachActive([](Bullet& bullet) {
        if (bullet.owner == BulletOwner::Enemy) {
            bullet.active = false;
        }
    });
}

Vector3 BulletSystem::DirectionFromYawPitch(float yaw, float pitch) {
    const float cosinePitch = std::cos(pitch);
    return Normalize({
        std::sin(yaw) * cosinePitch,
        std::sin(pitch),
        std::cos(yaw) * cosinePitch,
    });
}

Vector3 BulletSystem::DirectionTo(Vector3 from, Vector3 to) {
    return Normalize({to.x - from.x, to.y - from.y, to.z - from.z});
}

Vector3 BulletSystem::RotateAroundAxis(Vector3 vector, Vector3 axis, float angle) {
    axis = Normalize(axis);
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const Vector3 first = Scale(vector, cosine);
    const Vector3 second = Scale(Cross(axis, vector), sine);
    const Vector3 third = Scale(axis, Dot(axis, vector) * (1.0F - cosine));
    return Add(Add(first, second), third);
}

float BulletSystem::Length(Vector3 vector) {
    return std::sqrt(vector.x * vector.x + vector.y * vector.y +
                     vector.z * vector.z);
}

}  // namespace koumakyou
