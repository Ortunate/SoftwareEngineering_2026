// 本文件负责定义关卡事件数据与通用时间轴调度器接口。

#pragma once

#include "raylib.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace redmist {

/// @brief 关卡事件的稳定标识；编号只在单张事件表内有效，仅需要被引用或报告结束的事件必须设置。
using StageEventId = std::uint32_t;

/// @brief 一条通用时间轴事件；位置使用 raylib 世界坐标，action 和 archetype 由 Stage 自行解释。
struct StageEvent {
    /// @brief 无依赖时相对上一事件实际开始、存在依赖时相对依赖事件结束的延迟秒数。
    float time = 0.0F;
    /// @brief Stage 局部的动作编号。
    std::uint16_t action = 0;
    /// @brief Stage 局部的敌人原型编号。
    std::uint16_t archetype = 0;
    /// @brief 编队敌人数；交叉编队每两个敌人组成一对。
    int count = 1;
    /// @brief 编队事件使用的生成中心。
    Vector3 center{0.0F, 0.0F, 8.5F};
    /// @brief 直线编队的相邻间距或星形编队的半径。
    float spacing = 1.0F;
    /// @brief 本事件的可选标识，用于被后续事件引用和报告结束。
    std::optional<StageEventId> id = std::nullopt;
    /// @brief 可选的前序事件标识；设置后以其实际结束时间作为延迟起点。
    std::optional<StageEventId> afterEndOf = std::nullopt;
};

/// @brief 顺序读取事件表，按实际开始或结束时间调度；不直接创建敌人。
class StageDirector {
public:
    /// @brief 事件回调；返回 true 表示同步结束，false 表示稍后调用 EndEvent。
    using EventHandler = std::function<bool(const StageEvent&)>;

    /// @brief 构造尚未绑定事件表的调度器。
    StageDirector();
    /// @brief 借用外部事件表；调用方须保证其内容不变且生命周期长于调度器。
    /// @param events 按预期触发顺序排列的事件表，依赖只能引用更早的事件。
    /// @throws std::invalid_argument 延迟非法、ID 重复或依赖未指向更早的 ID。
    explicit StageDirector(std::span<const StageEvent> events);

    /// @brief 将关卡时钟、事件索引及开始/结束记录归零。
    void Reset();
    /// @brief 推进关卡时钟，并按顺序触发已满足延迟和依赖的事件。
    /// @param deltaTime 本帧经过的非负有限秒数。
    /// @param handler 执行事件并报告是否同步结束的回调。
    /// @throws std::invalid_argument deltaTime 为负数或非有限值。
    void Update(float deltaTime, const EventHandler& handler);
    /// @brief 记录已启动的异步事件在当前关卡时钟结束。
    /// @param id 事件表中的唯一标识。
    /// @return 找到尚未结束的已启动事件时返回 true。
    bool EndEvent(StageEventId id);
    /// @brief 查询开局至今的关卡时间。
    /// @return 已累计的秒数。
    [[nodiscard]] float Elapsed() const { return elapsed_; }

private:
    /// @brief 单条事件的实际时刻，以及构造时解析出的前序事件索引。
    struct EventState {
        std::optional<float> startedAt;
        std::optional<float> endedAt;
        std::optional<std::size_t> dependencyIndex;
    };

    /// @brief 按稳定 ID 查询事件表索引。
    /// @param id 要查找的事件标识。
    /// @return 找到时返回索引，否则返回空值。
    [[nodiscard]] std::optional<std::size_t> FindEvent(StageEventId id) const;

    std::span<const StageEvent> events_;
    std::vector<EventState> states_;
    float elapsed_ = 0.0F;
    std::size_t nextEvent_ = 0;
};

}  // namespace redmist
