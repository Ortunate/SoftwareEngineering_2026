// 本文件负责定义敌人/自机受击掉落、清弹星点及统一生成与拾取接口。

#pragma once

#include "core/fixed_pool.hpp"
#include "gameplay/items/drop_table.hpp"
#include "resources/battle_catalog.hpp"

#include "raylib.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

namespace koumakyou {

class BulletSystem;

/// @brief 道具种类；P 点增加 Power，分数点与清弹星点增加分数。
enum class PickupKind : std::uint8_t { Power, Score, Star, Bomb, Life, FullPower };

/// @brief 系统持有的道具；position 为世界坐标。
/// velocityZ 为普通抛落的世界 Z 轴速度（单位/秒），正值上抛、负值下落；追踪及 Miss 散落时忽略。
/// value 对 P 点是 Power 增量，对分数点和星点是得分增量；homing 一经触发便持续到拾取或消失。
/// active 失活后普通道具槽位可复用，星点由容器移除；复用时会重置全部运动状态。
struct Pickup {
    bool active = false;
    bool homing = false;
    PickupKind kind = PickupKind::Power;
    Vector3 position{};
    int value = 1;
    float velocityZ = 0.0F;
    float age = 0.0F; // 视觉秒数，与运动/追踪时钟独立；暂停时不推进。
    // Miss 道具先向前方散落一秒，期间不拾取/追踪；结束后从零下落速度接入普通运动。
    // 起终点均为世界坐标，保持受击高度；每个池槽独占，复用时重置。
    bool missScattering = false;
    Vector3 scatterOrigin{},scatterTarget{};
    double scatterSeconds = 0;
    /// @brief 出生后按 90 度/秒绕竖直长轴旋转；只由模拟 age 决定，暂停冻结，返回弧度。
    float RotationRadians() const { return std::fmod(age,4.F)*1.57079632679F; }
    /// @brief 当前道具对应的原始 Item 编号；大 P 按 Power 增量至少 8 区分。
    int NativeItemType() const { return kind==PickupKind::Bomb?3:kind==PickupKind::FullPower?4:kind==PickupKind::Life?5:kind==PickupKind::Star?6:kind==PickupKind::Score?1:value>=8?2:0; }
    battle::Cursor animation;
};

/// @brief 本帧拾取所得，由 Game 应用到玩家和分数状态。
struct PickupReward {
    int pointItems = 0; // 本次拾取的普通蓝色点数道具数量，不包含清弹星点。
    int power = 0;
    long long score = 0;
    int bombs=0,lives=0; // 本次拾取的 Bomb 与额外残机数量，由玩家库存接口钳制上限。
};

/// @brief 生成敌人/自机受击掉落和清弹星点，持有道具并处理运动、自动吸取与拾取。
class DropSystem {
public:
    /// @brief 普通掉落物的固定池容量；清弹星点另行存储，不占这些槽位。
    static constexpr std::size_t kCapacity = 1024;
    /// @brief 单个分数点的当前项目得分值。
    static constexpr int kScorePointValue = 1000;
    /// @brief 单个清弹星点的得分值；仅在被玩家吸收时结算。
    static constexpr int kStarPointValue = 100;

    /// @brief 使用随机种子构造生成器。
    DropSystem();
    /// @brief 使用指定种子构造生成器，供可复现的测试使用。
    explicit DropSystem(std::uint32_t seed);

    /// @brief 清除场上道具；随机序列继续推进，不因重新开始而重复。
    void Clear();
    /// @brief 新局种子重置；清空道具并从种子初始化轮换表，后续 Clear 仍推进序列。
    void ResetRun(std::uint32_t seed) { random_.seed(seed); Clear(); }
    /// @brief Bomb 释放时令当前所有道具立即追踪玩家；之后新生成的道具不继承这次吸取。
    void MagnetAllItems();
    /// @brief 在敌人死亡位置抽取一次结果并生成相应数量的两类道具。
    /// @param position 敌人死亡时的世界坐标。
    /// @param distribution 由敌人持有的只读结果表；仅在调用期间借用。
    /// @note 单次生成每种道具最多 32 个，池满时多余道具被丢弃。
    void SpawnEnemyDrops(Vector3 position,
                         std::span<const DropOutcome> distribution);
    /// @brief 为调试或自定义关卡生成单个给定 Power 值的 P 点。
    /// @param position 退场位置，使用世界坐标。
    /// @param power 增加的 Power 点数；非正值不生成。
    void SpawnFixedPower(Vector3 position, int power);
    /// @brief 在受击世界位置生成死亡掉落：有额外残机为 1 大 P（8）和 5 小 P（1），末命为 5 满 P。
    /// hasSpareLife 使用扣命前库存判断；与当前 Power 无关，不转换为分数，池满丢弃请求。
    /// 三维散落保持高度、向前方 X/Z 区域作一秒连续插值，然后下落；暂停时不推进。
    void SpawnPlayerMiss(Vector3 position, bool hasSpareLife);
    /// @brief 原作敌人掉落；-1每三次击破按32项表轮换，0…6与SpawnNativeItem编号相同；其他值不生成。
    /// 仅小/大P在满Power时转分数；固定道具不推进整关轮换计数，Clear随机初始化两个表偏移。
    void SpawnReferenceDrop(Vector3 position, int item, bool fullPower);
    /// @brief 执行 ins_119；count 限制在 0…32，未满 Power 时首个大 P（8），其余小 P（1）。
    /// 散落范围为 X/Z 各 ±72 原作像素，Y 保持敌人高度，满 Power 时全部转为点数。
    void SpawnReferenceBurst(Vector3 position, int count, bool fullPower);
    /// @brief 生成脚本指定的一件原生道具：0/1/2 为小P/分数/大P，3/4/5/6 为 Bomb/满P/残机/星点。
    /// 仅小/大 P 在 fullPower 时转分数；非法编号抛 invalid_argument，池满丢弃该件。
    void SpawnNativeItem(Vector3 position,int item,bool fullPower);
    /// @brief 将当前全部活动敌弹在原位置转成立即追踪玩家的星点，保留玩家弹。
    /// @param bullets 借用的弹幕系统；转换的敌弹失活，不再参与碰撞。
    /// @note 不受普通道具池容量限制，已有星点也会继续追踪。
    void ConvertEnemyBulletsToStars(BulletSystem& bullets);
    /// @brief 推进道具并拾取；普通掉落物先上抛再下落，Miss 道具先散落一秒再从零速度下落。
    /// @note 玩家进入 +Z 端 2/5 区域时，所有活动道具立即持续追踪玩家，优先于上抛和下落。
    /// 已触发的道具在玩家离开该区域后仍追踪；之后生成的道具重新从上抛开始。
    /// 非追踪道具沿 -Z 下落至 z < -kArenaHalfExtents.z 时消失；刚好位于边界时仍保留。
    /// 星点生成时已触发追踪，不受玩家所处区域限制；追踪中的道具持续到拾取或清场。
    /// @param deltaTime 本帧非负秒数。
    /// @param playerPosition 玩家判定中心的世界坐标。
    /// @return 本帧距离玩家中心小于 1.2 世界单位的道具奖励，调用方负责应用上限与满火力效果。
    PickupReward Update(float deltaTime, Vector3 playerPosition);

    /// @brief 遍历普通道具和星点，只借用对象；回调不得保留引用或修改道具容器。
    template <typename Function>
    void ForEachActive(Function&& function) const {
        pickups_.ForEachActive(function);
        for (const Pickup& star : stars_)
            if (star.active) function(star);
    }
    /// @brief 可修改遍历仅供统一推进视觉状态，调用期间不得生成、删除或保留引用。
    template <typename Function>
    void ForEachActive(Function&& function) {
        pickups_.ForEachActive(function);
        for(Pickup& star:stars_)if(star.active)function(star);
    }

private:
    void Spawn(PickupKind kind, Vector3 position, int value);

    FixedPool<Pickup, kCapacity> pickups_{};
    // 星点使用可增长容器，确保弹幕池满或普通道具池满时也能逐弹转换；拾取/清场后回收。
    std::vector<Pickup> stars_;
    std::mt19937 random_;
    unsigned referenceDropIndex_ = 0, referenceItemIndex_ = 0;
};

}  // namespace koumakyou
