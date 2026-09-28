#pragma once

#include "game/bullet_system.hpp"
#include "game/game_types.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <utility>

namespace redmist {

/// @brief 小怪抽象基类：按航线移动，计时到期时调用子类技能。
class Enemy {
public:
    /// @brief 通过基类指针安全销毁具体敌人。
    virtual ~Enemy() = default;
    /// @brief 更新入场延迟、航线和开火计时；age 为负时尚未入场。
    /// @param deltaTime 本帧经过的秒数。
    /// @param playerPosition 玩家判定中心，用于瞄准。
    /// @param bullets 接收技能生成的弹幕。
    virtual void Update(float deltaTime, Vector3 playerPosition,
                        BulletSystem& bullets);

    bool active = true;
    EnemyKind kind;
    PathKind path;
    Vector3 position;
    Vector3 origin;
    float age;
    float health = 10.0F;
    float maxHealth = 10.0F;
    float hitRadius = 0.55F;
    float speed = 1.0F;
    float fireTimer = 0.0F;
    float fireInterval = 1.5F;
    int scoreValue = 500;

    /// @brief 沿玩家方向从自身判定中心向前取发弹位置。
    /// @param playerPosition 玩家判定中心。
    /// @return 世界坐标中的发弹点。
    [[nodiscard]] Vector3 MuzzlePosition(Vector3 playerPosition) const;

protected:
    /// @brief 初始化敌人类型、航线、出生位置和入场延迟。
    /// @param kind 敌人类型。
    /// @param path 运动航线。
    /// @param position 出生位置。
    /// @param delay 入场延迟（秒）。
    Enemy(EnemyKind kind, PathKind path, Vector3 position, float delay);
    /// @brief 由子类生成技能弹幕；调用时机由 Update 统一控制。
    /// @param bullets 接收生成弹幕的系统。
    /// @param playerPosition 技能瞄准所用的玩家位置。
    virtual void UseSkill(BulletSystem& bullets, Vector3 playerPosition) = 0;
};

/// @brief 普通妖精，技能为单发瞄准弹。
class FairyEnemy final : public Enemy {
public:
    /// @brief 创建普通妖精。
    /// @param path 运动航线。
    /// @param position 出生位置。
    /// @param delay 入场延迟（秒），默认立即入场。
    FairyEnemy(PathKind path, Vector3 position, float delay = 0.0F);

private:
    /// @brief 向玩家发射单颗红色弹幕。
    /// @param bullets 接收弹幕的系统。
    /// @param playerPosition 玩家判定中心。
    void UseSkill(BulletSystem& bullets, Vector3 playerPosition) override;
};

/// @brief 暗色妖精，技能为较快的单发瞄准弹。
class DarkFairyEnemy final : public Enemy {
public:
    /// @brief 创建暗色妖精。
    /// @param path 运动航线。
    /// @param position 出生位置。
    /// @param delay 入场延迟（秒），默认立即入场。
    DarkFairyEnemy(PathKind path, Vector3 position, float delay = 0.0F);

private:
    /// @brief 向玩家发射较快的红色弹幕。
    /// @param bullets 接收弹幕的系统。
    /// @param playerPosition 玩家判定中心。
    void UseSkill(BulletSystem& bullets, Vector3 playerPosition) override;
};

/// @brief 光球敌人，技能为朝向玩家的锥形弹幕。
class OrbEnemy final : public Enemy {
public:
    /// @brief 创建光球敌人。
    /// @param path 运动航线。
    /// @param position 出生位置。
    /// @param delay 入场延迟（秒），默认立即入场。
    OrbEnemy(PathKind path, Vector3 position, float delay = 0.0F);

private:
    /// @brief 向玩家方向释放蓝色锥形弹幕。
    /// @param bullets 接收弹幕的系统。
    /// @param playerPosition 玩家判定中心。
    void UseSkill(BulletSystem& bullets, Vector3 playerPosition) override;
};

/// @brief 精英妖精，技能叠加球面散射与锥形瞄准弹。
class EliteEnemy final : public Enemy {
public:
    /// @brief 创建精英妖精。
    /// @param path 运动航线。
    /// @param position 出生位置。
    /// @param delay 入场延迟（秒），默认立即入场。
    EliteEnemy(PathKind path, Vector3 position, float delay = 0.0F);

private:
    /// @brief 同时释放球面与锥形两组弹幕。
    /// @param bullets 接收弹幕的系统。
    /// @param playerPosition 玩家判定中心。
    void UseSkill(BulletSystem& bullets, Vector3 playerPosition) override;
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
