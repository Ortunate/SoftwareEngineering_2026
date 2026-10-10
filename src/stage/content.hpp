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

namespace koumakyou {

/// @brief Stage 对游戏总控提出的生成与演出请求。
class StageEventSink {
public:
    virtual ~StageEventSink() = default;
    /// @brief 请求立即创建一个敌人；spec 为借用的出生配置，接收方须复制需保留的数据。
    virtual void SpawnStageEnemy(const EnemySpec& spec) = 0;
    /// @brief 请求开始道中或关底 Boss，并登记等待战斗结束的异步事件编号。
    virtual void BeginStageBoss(bool final, StageEventId id) = 0;
    /// @brief 请求开始战前或战后对话并登记异步事件编号；afterBattle 决定所用台词及结算路径。
    virtual void BeginStageDialogue(StageEventId id, bool afterBattle = false) = 0;
    /// @brief 请求显示一条持续指定秒数的提示；接收方须复制需要留存的文本。
    virtual void StageMessage(std::string_view message, float duration) = 0;
};

/// @brief 关卡对话中的一条台词与说话人信息；所有字符串指针须在对话结束前有效。
struct DialogueLine {
    const char* speaker;
    const char* text;
    bool bossSpeaking;
    /// @brief 非零时为本句停留秒数，否则使用关卡默认；line2Delay 为第二行出现的延迟秒数。
    float duration = 0, line2Delay = 0;
    /// @brief Rumia 在首次搭话时才出现，战后独白不显示；introduce 显示宵暗的妖怪名牌。
    bool showBoss = true, introduce = false;
    const char* secondLine = "";
    int expression = 0; // 新典原始表情编号；当前现代立绘沿用角色图，记录编号供表情资产扩展。
    float textDelay = 0;
};

/// @brief 一个关卡提供自己的素材、事件、敌人、Boss 和对话内容。
class StageContent {
public:
    virtual ~StageContent() = default;
    /// @brief 返回本关卡的图片与音频清单；返回的引用由 Stage 持有，调用方不得释放。
    [[nodiscard]] virtual const StageAssetManifest& Assets() const = 0;
    /// @brief 返回按触发顺序排列的时间轴事件；底层存储须覆盖 StageDirector 的生命周期。
    [[nodiscard]] virtual std::span<const StageEvent> Events() const = 0;
    /// @brief 根据显式出生配置立即构造敌人；实例自行持有运动状态和技能计时。
    /// @return 所有权交给调用方；无效或非本关卡的参数类型返回 nullptr。
    [[nodiscard]] virtual std::unique_ptr<Enemy> CreateEnemy(const EnemySpec& spec) const = 0;
    /// @brief 构造道中或关底 Boss 并初始化遭遇状态，所有权交给调用方。
    [[nodiscard]] virtual std::unique_ptr<Boss> CreateBoss(bool final) const = 0;
    /// @brief 执行调度器交付的一次动作；出怪请求必须已规范为 count=1。
    /// @return Boss 与对话返回 false，结束后通过 StageDirector::EndEvent 报告；其他返回 true。
    /// @throws std::invalid_argument 未规范的批次、缺少异步 ID 或动作参数非法。
    bool HandleEvent(const StageEvent& event, StageEventSink& sink) const;
    /// @brief 返回本关卡的对话台词；底层存储须在展示期间有效。
    [[nodiscard]] virtual std::span<const DialogueLine> Dialogue(bool afterBattle = false) const = 0;
    /// @brief 返回单句台词的展示秒数。
    [[nodiscard]] virtual float DialogueLineDuration() const = 0;
    /// @brief 返回玩家直射弹的外观 ID。
    [[nodiscard]] virtual VisualId PlayerBulletVisual() const = 0;
    /// @brief 返回玩家追踪弹的外观 ID。
    [[nodiscard]] virtual VisualId PlayerHomingVisual() const = 0;
    /// @brief 返回 Power 道具的外观 ID，贴图由当前 Stage 的素材清单持有。
    [[nodiscard]] virtual VisualId PowerPickupVisual() const = 0;
    /// @brief 返回分数道具的外观 ID，贴图由当前 Stage 的素材清单持有。
    [[nodiscard]] virtual VisualId ScorePickupVisual() const = 0;
    /// @brief 返回清弹星点的外观 ID，贴图由当前 Stage 的素材清单持有。
    [[nodiscard]] virtual VisualId StarPickupVisual() const = 0;
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

}  // namespace koumakyou
