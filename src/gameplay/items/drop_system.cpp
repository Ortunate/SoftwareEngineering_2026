// 本文件负责敌人/自机受击掉落和清弹星点生成，并统一处理道具移动和拾取。

#include "gameplay/items/drop_system.hpp"
#include "gameplay/data/native_sizes.hpp"

#include "gameplay/arena.hpp"
#include "gameplay/bullets/system.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <stdexcept>

namespace koumakyou {
namespace {
constexpr int kMaxCountPerKind = 32;
// 速度单位为世界单位/秒，加速度为世界单位/秒²；0.5 秒到达比生成点高 1 单位的顶点。
constexpr float kLaunchSpeed = 4.0F;
constexpr float kFallAcceleration = 8.0F;
constexpr float kFallSpeed = 2.5F;
constexpr float kAutoCollectSpeed = 20.0F;
// 所有道具共用以自机判定中心为球心的拾取半径，单位为世界单位。
constexpr float kCollectDistance = native_sizes::kItemCollectRadius;
// 战场 Z 范围为 [-kArenaHalfExtents.z, +kArenaHalfExtents.z]，前端 2/5 从 +0.2 倍半长开始。
constexpr float kAutoCollectZ = kArenaHalfExtents.z * 0.2F;
constexpr float kSpiralAngle = 2.3999632F;
}

DropSystem::DropSystem() : DropSystem(std::random_device{}()) {}

DropSystem::DropSystem(std::uint32_t seed) : random_(seed) {}

void DropSystem::Clear() {
    pickups_.Clear();
    stars_.clear();
    referenceDropIndex_ = std::uniform_int_distribution<unsigned>(0,2)(random_);
    referenceItemIndex_ = std::uniform_int_distribution<unsigned>(0,7)(random_);
}

void DropSystem::MagnetAllItems() {
    ForEachActive([](Pickup& pickup) { pickup.homing=true; });
}

void DropSystem::Spawn(PickupKind kind, Vector3 position, int value) {
    if (value <= 0) return;
    if (Pickup* pickup = pickups_.Acquire()) {
        pickup->kind = kind;
        pickup->position = position;
        pickup->value = value;
        pickup->velocityZ = kLaunchSpeed;
    }
}

void DropSystem::SpawnEnemyDrops(Vector3 position,
                                 std::span<const DropOutcome> distribution) {
    std::uint64_t totalWeight = 0;
    for (const DropOutcome& outcome : distribution)
        totalWeight += outcome.weight;
    if (totalWeight == 0) return;

    std::uniform_int_distribution<std::uint64_t> select(0, totalWeight - 1);
    std::uint64_t choice = select(random_);
    const DropOutcome* selected = nullptr;
    for (const DropOutcome& outcome : distribution) {
        if (choice < outcome.weight) {
            selected = &outcome;
            break;
        }
        choice -= outcome.weight;
    }
    if (!selected) return;

    const int powerCount = std::clamp(selected->powerCount, 0, kMaxCountPerKind);
    const int scoreCount = std::clamp(selected->scoreCount, 0, kMaxCountPerKind);
    if (powerCount + scoreCount == 0) return;
    std::uniform_real_distribution<float> phaseDistribution(
        0.0F, 2.0F * std::numbers::pi_v<float>);
    const float phase = phaseDistribution(random_);
    // 同时掉落的道具按黄金角错开，避免贴图完全重叠；位移保持在敌人附近。
    for (int index = 0; index < powerCount + scoreCount; ++index) {
        const float angle = phase + static_cast<float>(index) * kSpiralAngle;
        const float radius = 0.22F + 0.11F * std::sqrt(static_cast<float>(index));
        Vector3 itemPosition{
            position.x + std::cos(angle) * radius,
            position.y + std::sin(angle) * radius,
            position.z,
        };
        constexpr Vector3 limit{kArenaHalfExtents.x - 0.2F,
                                kArenaHalfExtents.y - 0.2F,
                                kArenaHalfExtents.z - 0.2F};
        itemPosition.x = std::clamp(itemPosition.x, -limit.x, limit.x);
        itemPosition.y = std::clamp(itemPosition.y, -limit.y, limit.y);
        itemPosition.z = std::clamp(itemPosition.z, -limit.z, limit.z);
        if (index < powerCount)
            Spawn(PickupKind::Power, itemPosition, 1);
        else
            Spawn(PickupKind::Score, itemPosition, kScorePointValue);
    }
}

void DropSystem::SpawnFixedPower(Vector3 position, int power) {
    Spawn(PickupKind::Power, position, power);
}

void DropSystem::SpawnPlayerMiss(Vector3 position, bool hasSpareLife) {
    // 原生 drop_state=2 的目标跨场地前方随机分布；按战场比例映射 X/Z，Y 保持受击高度。
    // 连续插值替代原作逐帧采样的末帧差，避免不同更新频率改变散落终点。
    std::uniform_real_distribution<float> horizontal(-0.75F*kArenaHalfExtents.x,0.75F*kArenaHalfExtents.x);
    std::uniform_real_distribution<float> depth(kArenaHalfExtents.z*3.0F/7,kArenaHalfExtents.z*9.0F/7);
    for(int index=0;index<(hasSpareLife?6:5);++index) {
        const Vector3 target{horizontal(random_),position.y,depth(random_)};
        if(Pickup* pickup=pickups_.Acquire()) {
            pickup->kind=hasSpareLife?PickupKind::Power:PickupKind::FullPower;
            pickup->value=hasSpareLife?(index==0?8:1):128;
            pickup->position=pickup->scatterOrigin=position;
            pickup->scatterTarget=target;
            pickup->missScattering=true;
        }
    }
}

void DropSystem::SpawnReferenceDrop(Vector3 position, int item, bool fullPower) {
    // 原作随机掉落实际为共享轮换表；每三次满足随机掉落条件的击破才前进一项。
    static constexpr std::array<int,32> items{0,0,1,0,1,0,0,1,1,1,0,0,0,1,1,0,
                                            1,0,1,0,1,0,1,0,1,0,0,1,1,1,0,2};
    if(item==-1) {
        if(referenceDropIndex_++%3!=0)return;
        item=items[referenceItemIndex_++%items.size()];
    }
    // 原生演员的非负掉落字节直接交给统一道具入口；Extra尾波的5是残机，不得丢弃或在满P时转分数。
    if(item>=3&&item<=6) {SpawnNativeItem(position,item,fullPower);return;}
    if(item<0 || item>2)return;
    const bool score=item==1 || fullPower;
    Spawn(score ? PickupKind::Score : PickupKind::Power,position,
          score ? kScorePointValue : item==2 ? 8 : 1);
}

void DropSystem::SpawnReferenceBurst(Vector3 position, int count, bool fullPower) {
    constexpr float spread=72.0F*20.0F/448.0F;
    std::uniform_real_distribution<float> offset(-spread,spread);
    for(int index=0;index<std::clamp(count,0,32);++index) {
        const Vector3 scattered{position.x+offset(random_),position.y,position.z+offset(random_)};
        Spawn(fullPower ? PickupKind::Score : PickupKind::Power,scattered,
              fullPower ? kScorePointValue : index==0 ? 8 : 1);
    }
}
void DropSystem::SpawnNativeItem(Vector3 position,int item,bool fullPower) {
    if(item<0||item>6)throw std::invalid_argument("Invalid native item id");
    if(item<=2) { SpawnReferenceDrop(position,item,fullPower);return; }
    if(item==3)Spawn(PickupKind::Bomb,position,1);
    else if(item==4)Spawn(PickupKind::FullPower,position,128);
    else if(item==5)Spawn(PickupKind::Life,position,1);
    else Spawn(PickupKind::Star,position,kStarPointValue);
}

void DropSystem::ConvertEnemyBulletsToStars(BulletSystem& bullets) {
    // 先预留空间，再停用敌弹，避免生成中途扩容；连续升满时保留上一批尚未吸收的星点。
    stars_.reserve(stars_.size() + bullets.Pool().ActiveCount());
    bullets.Pool().ForEachActive([this](Bullet& bullet) {
        if (bullet.owner != BulletOwner::Enemy) return;
        Pickup star{};
        star.active = true;
        star.homing = true;
        star.kind = PickupKind::Star;
        star.position = bullet.position;
        star.value = kStarPointValue;
        stars_.push_back(star);
        bullet.active = false;
    });
}

PickupReward DropSystem::Update(float deltaTime, Vector3 playerPosition) {
    PickupReward reward{};
    const auto updatePickup = [&](Pickup& pickup) {
        pickup.age+=std::max(0.0F,deltaTime);
        float motionSeconds=deltaTime;
        if(pickup.missScattering) {
            const double remaining=1.0-pickup.scatterSeconds;
            pickup.scatterSeconds=std::min(1.0,pickup.scatterSeconds+std::max(0.0F,deltaTime));
            if(pickup.scatterSeconds>1.0-1e-6)pickup.scatterSeconds=1;
            const float ratio=static_cast<float>(pickup.scatterSeconds);
            pickup.position={pickup.scatterOrigin.x+(pickup.scatterTarget.x-pickup.scatterOrigin.x)*ratio,
                pickup.scatterOrigin.y,pickup.scatterOrigin.z+(pickup.scatterTarget.z-pickup.scatterOrigin.z)*ratio};
            if(pickup.scatterSeconds<1)return;
            pickup.missScattering=false;
            motionSeconds=std::max(0.0F,deltaTime-static_cast<float>(remaining));
        }
        // 吸取状态属于单个道具；玩家离开触发区域不会取消已有道具的追踪。
        if (playerPosition.z >= kAutoCollectZ) pickup.homing = true;
        if (pickup.homing) {
            const Vector3 toPlayer{
                playerPosition.x - pickup.position.x,
                playerPosition.y - pickup.position.y,
                playerPosition.z - pickup.position.z,
            };
            const float distance = std::sqrt(toPlayer.x * toPlayer.x +
                                             toPlayer.y * toPlayer.y +
                                             toPlayer.z * toPlayer.z);
            if (distance > 0.0001F) {
                // 限制单帧位移，避免越过玩家后反复折返。
                const float ratio = std::min(kAutoCollectSpeed * motionSeconds / distance, 1.0F);
                pickup.position.x += toPlayer.x * ratio;
                pickup.position.y += toPlayer.y * ratio;
                pickup.position.z += toPlayer.z * ratio;
            }
        } else {
            // 按达到速度上限的时刻拆分本帧，并精确积分加速段；跨越顶点或限速点时，
            // 位移仍须与分成多个小帧的结果一致，不能用帧末速度代替整帧速度。
            const float accelerationTime = std::min(
                motionSeconds, (pickup.velocityZ + kFallSpeed) / kFallAcceleration);
            pickup.position.z += pickup.velocityZ * accelerationTime -
                                 0.5F * kFallAcceleration * accelerationTime * accelerationTime -
                                 kFallSpeed * (motionSeconds - accelerationTime);
            pickup.velocityZ = std::max(
                -kFallSpeed, pickup.velocityZ - kFallAcceleration * accelerationTime);
            // 仅下落道具按后边界回收；先判断越界再拾取，避免穿出场地后仍发放奖励。
            // 追踪会中断下落，清弹星点也可能从边界外生成，须允许它们继续飞回玩家。
            if (pickup.position.z < -kArenaHalfExtents.z) {
                pickup.active = false;
                return;
            }
        }
        const float dx = playerPosition.x - pickup.position.x;
        const float dy = playerPosition.y - pickup.position.y;
        const float dz = playerPosition.z - pickup.position.z;
        if (dx * dx + dy * dy + dz * dz < kCollectDistance * kCollectDistance) {
            if (pickup.kind == PickupKind::Power || pickup.kind == PickupKind::FullPower)
                reward.power += pickup.value;
            else if(pickup.kind==PickupKind::Bomb)reward.bombs+=pickup.value;
            else if(pickup.kind==PickupKind::Life)reward.lives+=pickup.value;
            else
                reward.score += pickup.value;
            if (pickup.kind == PickupKind::Score) ++reward.pointItems;
            pickup.active = false;
        }
    };
    pickups_.ForEachActive(updatePickup);
    for (Pickup& star : stars_) updatePickup(star);
    std::erase_if(stars_, [](const Pickup& star) { return !star.active; });
    return reward;
}

}  // namespace koumakyou
