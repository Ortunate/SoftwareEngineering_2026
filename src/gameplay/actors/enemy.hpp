// 本文件负责定义敌人抽象基类及持有多态敌人的固定槽位池。

#pragma once

#include "gameplay/bullets/system.hpp"
#include "gameplay/items/drop_table.hpp"


#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>

namespace redmist {

/// @brief 小怪抽象基类：仅处理入场与边界，移动和技能计时由具体敌人实现。
class Enemy {
public:
    /// @brief 通过基类指针安全销毁具体敌人。
    virtual ~Enemy() = default;
    /// @brief 更新入场延迟，并委托子类执行移动和各技能计时。
    /// @param deltaTime 本帧经过的秒数。
    /// @param playerPosition 玩家判定中心，用于瞄准。
    /// @param bullets 接收技能生成的弹幕。
    virtual void Update(float deltaTime, Vector3 playerPosition,
                        BulletSystem& bullets);

    bool active = true;
    const VisualId visual;
    Vector3 position;
    Vector3 origin;
    /// @brief 从实际入场起算的秒数；延迟期间为负，达到零后才移动和发弹。
    float age;
    float health = 10.0F;
    float maxHealth = 10.0F;
    float hitRadius = 0.55F;
    int scoreValue = 500;
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
    /// @brief 初始化外观、出生位置和入场延迟。
    /// @param visual 外观编号。
    /// @param position 出生位置。
    /// @param delay 非负入场延迟（秒）。
    /// @param drops 本类敌人的静态掉落结果表；存储须覆盖敌人生命周期。
    Enemy(VisualId visual, Vector3 position, float delay,
          std::span<const DropOutcome> drops);
    /// @brief Stage 子类按自身规则更新空间位置。
    virtual void Move(float deltaTime) = 0;
    /// @brief 子类自行推进任意数量的技能计时器并释放弹幕。
    virtual void UpdateSkills(float deltaTime, Vector3 playerPosition,
                              BulletSystem& bullets) = 0;

private:
    std::span<const DropOutcome> dropDistribution_;
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

}  // namespace redmist
