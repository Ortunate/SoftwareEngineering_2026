// 本文件负责定义带参关卡事件及支持连续出怪的时间轴调度器接口。

#pragma once

#include "stage/enemy_spec.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace koumakyou {

/// @brief 关卡事件的稳定标识；编号只在单张事件表内有效，仅需要被引用或报告结束的事件必须设置。
using StageEventId = std::uint32_t;

/// @brief 连续请求生成相同配置的小怪；第一只在事件开始时立即生成。
/// count 至少为 1；interval 为非负有限秒数，零表示同帧生成全部；总间隔时长须有限。
/// 最后一次生成请求发出后事件结束，与敌人存活时间和池满丢弃无关。
struct SpawnEnemies {
    EnemySpec enemy;
    int count = 1;
    float interval = 0.5F;
};

/// @brief 当前关卡的 Boss 遭遇类型。
enum class BossEncounter { Midboss, Final };

/// @brief 开始 Boss 遭遇；所属 StageEvent 必须带 id，战斗结束时报告完成。
struct StartBoss {
    BossEncounter encounter = BossEncounter::Midboss;
};

/// @brief 开始当前关卡对话；所属 StageEvent 必须带 id，对话结束时报告完成。
struct StartDialogue { bool afterBattle = false; };

/// @brief 显示提示；duration 为非负有限秒数，text 的存储须覆盖事件表生命周期。
struct ShowMessage {
    std::string_view text;
    float duration = 2.4F;
};

/// @brief 带参事件动作；新增分支须同步扩展校验与执行访问器。
using StageAction = std::variant<SpawnEnemies, StartBoss, StartDialogue, ShowMessage>;

/// @brief 一条时间轴事件：调度信息独立于带参动作，事件表本身不保存执行进度。
struct StageEvent {
    /// @brief 无依赖时相对上一事件实际开始、存在依赖时相对依赖事件结束的延迟秒数。
    float delay = 0.0F;
    /// @brief 可选绝对出生帧（60 Hz）；存在时忽略 delay，不能同时设置 afterEndOf。
    std::optional<int> atFrame = std::nullopt;
    StageAction action = ShowMessage{};
    /// @brief 本事件的可选标识，用于被后续事件引用和报告结束。
    std::optional<StageEventId> id = std::nullopt;
    /// @brief 可选的前序事件标识；设置后以其实际结束时间作为延迟起点。
    std::optional<StageEventId> afterEndOf = std::nullopt;
};

/// @brief 按实际开始或结束时间调度，并保存连续出怪进度；通过回调请求动作。
class StageDirector {
public:
    /// @brief 事件回调；返回 true 表示同步结束，false 表示稍后调用 EndEvent。
    /// 出怪每次回调的 count 被规范为 1，返回值被忽略，整批结束由调度器负责。
    /// 请求只在回调期间有效，调用方不得保留指向请求对象的指针或引用。
    using EventHandler = std::function<bool(const StageEvent&)>;

    /// @brief 构造尚未绑定事件表的调度器。
    StageDirector();
    /// @brief 借用外部事件表；调用方须保证其内容不变且生命周期长于调度器。
    /// @param events 按预期触发顺序排列的事件表，依赖只能引用更早的事件。
    /// @throws std::invalid_argument 参数非法、异步动作缺少 ID、ID 重复或依赖未指向更早的 ID。
    explicit StageDirector(std::span<const StageEvent> events);

    /// @brief 将关卡时钟、事件索引及开始/结束记录归零。
    void Reset();
    /// @brief 推进关卡时钟，补发已到期的出怪请求，并启动满足延迟和依赖的事件。
    /// 连续出怪不阻塞后续事件；时间从本批实际开始起算，跨帧补发不累积间隔误差。
    /// @param deltaTime 本帧经过的非负有限秒数。
    /// @param handler 执行事件并报告是否同步结束的回调。
    /// @throws std::invalid_argument deltaTime 为负数或非有限值。
    void Update(float deltaTime, const EventHandler& handler);
    /// @brief 记录已启动的异步事件在当前关卡时钟结束。
    /// @param id 事件表中的唯一标识。
    /// @return 找到尚未结束的已启动非出怪事件时返回 true；出怪批次由调度器结束。
    bool EndEvent(StageEventId id);
    /// @brief 取消已启动批次中尚未发出的出怪请求，并以当前时刻结束这些批次。
    /// Boss 入场或对话清场时调用，停止旧批次后续出怪；不取消尚未启动的事件或移除现存敌人。
    void CancelEnemySpawns();
    /// @brief 查询开局至今的关卡时间。
    /// @return 已累计的秒数。
    [[nodiscard]] float Elapsed() const { return elapsed_; }

private:
    /// @brief 单条事件的实际时刻，以及构造时解析出的前序事件索引。
    struct EventState {
        std::optional<float> startedAt;
        std::optional<float> endedAt;
        std::optional<std::size_t> dependencyIndex;
        int spawned = 0;
    };

    /// @brief 发出一批截至当前时刻到期的单次生成请求，并记录整批完成时刻。
    void UpdateSpawns(std::size_t index, const EventHandler& handler);

    /// @brief 按稳定 ID 查询事件表索引。
    /// @param id 要查找的事件标识。
    /// @return 找到时返回索引，否则返回空值。
    [[nodiscard]] std::optional<std::size_t> FindEvent(StageEventId id) const;

    std::span<const StageEvent> events_;
    std::vector<EventState> states_;
    float elapsed_ = 0.0F;
    std::size_t nextEvent_ = 0;
};

}  // namespace koumakyou
