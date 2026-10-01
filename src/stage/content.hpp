// 本文件负责定义关卡素材、事件、敌人工厂和演出请求的抽象接口。

#pragma once

#include "gameplay/actors/boss.hpp"
#include "gameplay/actors/enemy.hpp"
#include "stage/system.hpp"
#include "resources/visual_assets.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace redmist {

/// @brief 关卡编队提供的出生上下文；position 为世界坐标，delay 为入场延迟秒数。
/// formation 是关卡局部编号，slot 是本次编队内编号；具体运动轨迹由敌人决定。
struct EnemySpawn {
    Vector3 position{};
    float delay = 0.0F;
    std::uint16_t formation = 0;
    int slot = 0;
};

/// @brief Stage 对游戏总控提出的生成与演出请求。
class StageEventSink {
public:
    virtual ~StageEventSink() = default;
    /// @brief 请求游戏总控生成一个当前关卡的敌人；原型编号由当前 Stage 解释。
    virtual void SpawnStageEnemy(std::uint16_t archetype,
                                 const EnemySpawn& spawn) = 0;
    /// @brief 请求开始道中或关底 Boss，并登记等待战斗结束的异步事件编号。
    virtual void BeginStageBoss(bool final, StageEventId id) = 0;
    /// @brief 请求开始对话并登记异步事件编号。
    virtual void BeginStageDialogue(StageEventId id) = 0;
    /// @brief 请求显示一条持续指定秒数的提示；接收方须复制需要留存的文本。
    virtual void StageMessage(std::string_view message, float duration) = 0;
};

/// @brief 关卡对话中的一条台词与说话人信息；两个字符串指针须在对话结束前有效。
struct DialogueLine {
    const char* speaker;
    const char* text;
    bool bossSpeaking;
};

/// @brief 一个关卡提供自己的素材、事件、敌人、Boss 和对话内容。
class StageContent {
public:
    virtual ~StageContent() = default;
    /// @brief 返回本关卡的图片与音频清单；返回的引用由 Stage 持有，调用方不得释放。
    [[nodiscard]] virtual const StageAssetManifest& Assets() const = 0;
    /// @brief 返回按触发顺序排列的时间轴事件；底层存储须覆盖 StageDirector 的生命周期。
    [[nodiscard]] virtual std::span<const StageEvent> Events() const = 0;
    /// @brief 根据关卡局部原型与编队上下文构造敌人；运动由具体类选择。
    /// @return 所有权交给调用方；不支持的原型或编队返回 nullptr。
    [[nodiscard]] virtual std::unique_ptr<Enemy> CreateEnemy(
        std::uint16_t archetype, const EnemySpawn& spawn) const = 0;
    /// @brief 构造道中或关底 Boss 并初始化遭遇状态，所有权交给调用方。
    [[nodiscard]] virtual std::unique_ptr<Boss> CreateBoss(bool final) const = 0;
    /// @brief 解释一个关卡事件；异步事件返回 false，结束后须通过 StageDirector::EndEvent 报告。
    virtual bool HandleEvent(const StageEvent& event, StageEventSink& sink) const = 0;
    /// @brief 返回本关卡的对话台词；底层存储须在展示期间有效。
    [[nodiscard]] virtual std::span<const DialogueLine> Dialogue() const = 0;
    /// @brief 返回单句台词的展示秒数。
    [[nodiscard]] virtual float DialogueLineDuration() const = 0;
    /// @brief 返回玩家弹幕的外观 ID。
    [[nodiscard]] virtual VisualId PlayerBulletVisual() const = 0;
    /// @brief 返回对话中玩家侧立绘的外观 ID。
    [[nodiscard]] virtual VisualId HeroPortraitVisual() const = 0;
    /// @brief 返回对话与符卡中 Boss 立绘的外观 ID。
    [[nodiscard]] virtual VisualId BossPortraitVisual() const = 0;
    /// @brief 返回关卡开始提示。
    [[nodiscard]] virtual const char* StartTitle() const = 0;
    /// @brief 返回胜利结算标题。
    [[nodiscard]] virtual const char* ClearTitle() const = 0;
    /// @brief 返回 Boss 出场提示。
    [[nodiscard]] virtual const char* BossStartMessage(bool final) const = 0;
    /// @brief 返回道中 Boss 退场提示。
    [[nodiscard]] virtual const char* MidbossEndMessage() const = 0;
};

}  // namespace redmist
