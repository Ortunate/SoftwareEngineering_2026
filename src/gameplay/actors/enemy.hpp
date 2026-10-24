// 本文件负责定义敌人抽象基类及持有多态敌人的固定槽位池。

#pragma once

#include "gameplay/bullets/system.hpp"
#include "gameplay/items/drop_table.hpp"
#include "gameplay/effects/actor_effects.hpp"


#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>

namespace koumakyou {

/// @brief 小怪抽象基类：处理出生年龄、独立视觉时钟与边界，移动和技能由具体敌人实现。
class Enemy {
public:
    /// @brief 通过基类指针安全销毁具体敌人。
    virtual ~Enemy() = default;
    /// @brief 更新出生年龄与视觉时钟，委托子类执行移动和技能；自然退场只设置 active=false。
    /// 调用方须在失活的当帧保存退场快照，然后停止更新；效果不承担战斗对象所有权。
    /// @param deltaTime 本帧经过的秒数。
    /// @param playerPosition 玩家判定中心，用于瞄准。
    /// @param bullets 接收技能生成的弹幕。
    virtual void Update(float deltaTime, Vector3 playerPosition,
                        BulletSystem& bullets);

    /// @brief 接收关卡自适应难度值 10…32；普通演员默认忽略，原作脚本演员用于弹速/弹数。
    virtual void SetChallengeRank(int) {}
    /// @brief 当前是否允许扣血；原生脚本演员须反映 105 号可受伤开关。
    [[nodiscard]] virtual bool CanTakeDamage() const { return true; }
    /// @brief 活动且存活的本体可接触玩家；与 CanTakeDamage 独立，敌方免伤不等于本体无害。
    /// 只做视觉淡入/淡出的演员仍按真实生命周期判定；需要无害准备/退场的子类可覆盖此接口。
    [[nodiscard]] virtual bool CanContactPlayer() const { return active && health>0; }
    /// @brief 借用本帧脚本产生的音效编号；下次更新失效，普通演员返回空表。
    [[nodiscard]] virtual std::span<const int> SoundEvents() const { return {}; }
    /// @brief 原作掉落编号；-1 为轮换掉落，0 为小 P 点，-3 表示使用 DropDistribution。
    [[nodiscard]] virtual int ReferenceDropKind() const { return -3; }
    bool active = true;
    const VisualId visual;
    Vector3 position;
    Vector3 origin;
    /// @brief 从实例出生起算的非负秒数；出怪调度不在敌人内部等待。
    float age = 0.0F;
    float health = 10.0F;
    float maxHealth = 10.0F;
    float hitRadius = 0.55F; // 世界半径，自机弹与本体接触共用，不随视觉入退场缩放。
    int scoreValue = 500;
    /// @brief 独立视觉状态；不决定活动标记、判定或技能时钟。
    ActorPresentation presentation;
    battle::Cursor animation; // 借用本关卡原 ANM，暂停/同龄绘制不推进；不控制碰撞。
    VisualId defeatVisual = kInvalidVisualId; // 可覆盖清单的击破动画，invalid 时采用清单绑定。
    int animationScript = -1; // -1 沿用清单默认；原 ECL 可覆盖本图集脚本编号。
    double animationAge = -1; // 原 ECL 的本次动画秒数；负值使用演员出生年龄。
    std::uint32_t animationRevision = 0; // 同编号重新启动时也递增，不能只比较编号。
    /// @brief 返回本类敌人的两类道具联合分布；表由具体敌人静态持有。
    /// @return 仅在敌人生命周期内借用的只读表，空表表示不掉落。
    [[nodiscard]] std::span<const DropOutcome> DropDistribution() const {
        return dropDistribution_;
    }

    /// @brief 沿玩家方向从自身判定中心向前取发弹位置。
    /// @param playerPosition 玩家判定中心。
    /// @return 世界坐标中的发弹点。
    [[nodiscard]] Vector3 MuzzlePosition(Vector3 playerPosition) const;

protected:
    /// @brief 初始化外观、出生位置与掉落表；实例立即活动。
    /// @param visual 外观编号。
    /// @param position 出生位置。
    /// @param drops 本类敌人的静态掉落结果表；存储须覆盖敌人生命周期。
    Enemy(VisualId visual, Vector3 position,
          std::span<const DropOutcome> drops);
    /// @brief Stage 子类按自身规则更新空间位置。
    virtual void Move(float deltaTime) = 0;
    /// @brief 子类自行推进任意数量的技能计时器并释放弹幕。
    virtual void UpdateSkills(float deltaTime, Vector3 playerPosition,
                              BulletSystem& bullets) = 0;

private:
    std::span<const DropOutcome> dropDistribution_;
    // 首次 Move 可能把 origin 转成轨道初始位置，须等后续采样才能估算真实速度。
    bool hasMovementSample_ = false;
};

/// @brief 用固定数量的槽位持有多态敌人并统计丢弃的生成请求。
/// @tparam Capacity 敌人池的最大槽位数。
template <std::size_t Capacity>
class EnemyPool {
public:
    /// @brief 将敌人放入空槽位或复用失活槽位。
    /// @param enemy 待转移所有权的具体敌人。
    /// @return 加入成功返回 true；池满返回 false 并记录丢弃次数。
    bool Add(std::unique_ptr<Enemy> enemy) {
        for (auto& slot : enemies_) {
            if (!slot || !slot->active) {
                slot = std::move(enemy);
                return true;
            }
        }
        ++droppedSpawns_;
        return false;
    }

    /// @brief 对活动敌人执行可修改回调。
    /// @tparam Function 接收 Enemy& 的可调用类型。
    /// @param function 接收 Enemy& 的回调。
    template <typename Function>
    void ForEachActive(Function&& function) {
        for (auto& enemy : enemies_) {
            if (enemy && enemy->active) function(*enemy);
        }
    }

    /// @brief 对活动敌人执行只读回调。
    /// @tparam Function 接收 const Enemy& 的可调用类型。
    /// @param function 接收 const Enemy& 的回调。
    template <typename Function>
    void ForEachActive(Function&& function) const {
        for (const auto& enemy : enemies_) {
            if (enemy && enemy->active)
                function(static_cast<const Enemy&>(*enemy));
        }
    }

    /// @brief 清空敌人池并重置丢弃计数。
    void Clear() {
        for (auto& enemy : enemies_) enemy.reset();
        droppedSpawns_ = 0;
    }

    /// @brief 查询因容量不足而丢弃的生成请求数。
    /// @return 最近一次 Clear 之后的丢弃次数。
    [[nodiscard]] std::size_t DroppedSpawns() const { return droppedSpawns_; }

private:
    std::array<std::unique_ptr<Enemy>, Capacity> enemies_{};
    std::size_t droppedSpawns_ = 0;
};

}  // namespace koumakyou
