#pragma once

#include "game/game_types.hpp"

#include <cstddef>
#include <functional>

namespace redmist {

/// @brief 关卡时间轴可调度的出怪与演出事件种类。
enum class StageEventKind : std::uint8_t {
    SpawnLine,
    SpawnStarFormation,
    SpawnCrossingPair,
    SpawnElite,
    StartMidboss,
    StartDialogue,
    StartFinalBoss,
    ShowMessage
};

/// @brief 一条关卡时间轴事件；不同 kind 只读取所需的生成参数。
struct StageEvent {
    /// @brief 自本局战斗开始算起的触发秒数。
    float time = 0.0F;
    /// @brief 事件类型，决定其余哪些参数生效。
    StageEventKind kind = StageEventKind::SpawnLine;
    /// @brief 普通编队事件使用的敌人类型；SpawnElite 固定生成精英。
    EnemyKind enemy = EnemyKind::Fairy;
    /// @brief 编队敌人数；交叉编队每两个敌人组成一对。
    int count = 1;
    /// @brief 直线、星形与精英事件使用的生成中心。
    Vector3 center{0.0F, 0.0F, 8.5F};
    /// @brief 直线编队的相邻间距或星形编队的半径。
    float spacing = 1.0F;
};

/// @brief 顺序读取一面事件表并回调 Game，不直接创建敌人。
class StageDirector {
public:
    /// @brief 事件触发时接收对应记录的回调类型。
    using EventHandler = std::function<void(const StageEvent&)>;

    /// @brief 将关卡时钟和下一条事件索引归零。
    void Reset();
    /// @brief 推进关卡时间并按顺序触发全部到期事件。
    /// @param deltaTime 本帧经过的秒数。
    /// @param handler 接收 StageEvent 的回调。
    void Update(float deltaTime, const EventHandler& handler);
    /// @brief 查询开局至今的关卡时间。
    /// @return 已累计的秒数。
    [[nodiscard]] float Elapsed() const { return elapsed_; }

private:
    float elapsed_ = 0.0F;
    std::size_t nextEvent_ = 0;
};

}  // namespace redmist
