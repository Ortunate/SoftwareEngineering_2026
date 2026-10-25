// 本文件定义关卡素材、事件、Boss 工厂及向行为组件提交演出/生成请求的接口。

#pragma once

#include "gameplay/actors/boss.hpp"
#include "gameplay/actors/behavior.hpp"
#include "gameplay/actors/player.hpp"
#include "gameplay/combat/damage_rules.hpp"
#include "stage/system.hpp"
#include "resources/visual_assets.hpp"

#include <array>
#include <cstddef>
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
    virtual void SpawnStageActor(const behavior::Spawn& spec) = 0;
    /// @brief 请求开始道中或关底 Boss，并登记等待战斗结束的异步事件编号。
    virtual void BeginStageBoss(bool final, StageEventId id) = 0;
    /// @brief 请求对话并登记异步事件编号；afterBattle 的全部台词结束后再展示关卡结算。
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
    int expression = 0; // 当前说话人的关卡局部表情编号；另一方保留最近一句的表情。
    float textDelay = 0;
    float expressionDelay = 0; // 本句表情相对句首的生效秒数；在此之前保留上一句的差分。
    float confirmDelay = 8.0F/60; // 原 MSG 等待计数至少达到 8 帧后才接受单次确认。
    float advanceAfter = -1; // 确认后跳至本句内此秒数；负值跳到下一句，末句保留原始退场/结算等待。
    bool secondLineReplacesFirst = false; // 原脚本写入同一文字槽时保留覆盖行为，原文仍分别保存。
};

/// @brief 返回截至 lineIndex 的玩家/Boss 表情编号，数组下标分别为 0/1。
/// elapsed 为当前句内秒数，默认取完整句末；尚未发言的一方使用 0，不持有台词。
[[nodiscard]] std::array<int, 2> DialogueExpressionsAt(
    std::span<const DialogueLine> lines, std::size_t lineIndex, float elapsed = 1.0e9F);

/// @brief 原 MSG 展开后的演出指令；frame 是包含 wait 停顿的 60 Hz 实际播放帧，按时间和原指令顺序排列。
/// Portrait 的 slot=0/1 表示自机/对手，value 是 ANM 中断标签 1/3/4/5；其他指令不使用 slot。
enum class DialogueCueKind { Portrait, InputEnabled, BossMusic };
struct DialogueCue { int frame; DialogueCueKind kind; int slot, value; };
/// @brief 查询已展开指令的布尔状态，未出现指令时返回 initial；seconds 是整段对话已播放秒数。
[[nodiscard]] bool DialogueFlagAt(std::span<const DialogueCue> cues, DialogueCueKind kind,
                                float seconds, bool initial = false);

/// @brief 悬浮题签的关卡名称；字符串借用内容模块，须覆盖整个关卡展示期，不包含游玩模式。
struct StageDisplayInfo {
    std::string_view label, title, subtitle;
};

/// @brief 一个关卡提供自己的素材、事件、敌人、Boss 和对话内容。
class StageContent {
public:
    virtual ~StageContent() = default;
    /// @brief 可选最终 HP 规则，不定义接触几何；无敌目标及 Bomb 零接触不进入提供方。
    /// 默认空指针使用明确标记的基准/回退及已核对的 Bomb 规则。
    [[nodiscard]] virtual const DamageRuleProvider* DamageRules() const { return nullptr; }
    /// @brief 关联原始数值目录；默认空表示尚未绑定。DataOnly 记录仍需实现演员和时间轴才可游玩。
    [[nodiscard]] virtual const native_combat::StageData* CombatData() const { return nullptr; }
    /// @brief 返回本关卡的图片与音频清单；返回的引用由 Stage 持有，调用方不得释放。
    [[nodiscard]] virtual const StageAssetManifest& Assets() const = 0;
    /// @brief 返回按触发顺序排列的时间轴事件；底层存储须覆盖 StageDirector 的生命周期。
    [[nodiscard]] virtual std::span<const StageEvent> Events() const = 0;
    /// @brief 构造道中或关底 Boss 并初始化遭遇状态，所有权交给调用方。
    [[nodiscard]] virtual std::unique_ptr<Boss> CreateBoss(bool final) const = 0;
    /// @brief 执行调度器交付的一次动作；出怪请求必须已规范为 count=1。
    /// @return Boss 与对话返回 false，结束后通过 StageDirector::EndEvent 报告；其他返回 true。
    /// @throws std::invalid_argument 未规范的批次、缺少异步 ID 或动作参数非法。
    bool HandleEvent(const StageEvent& event, StageEventSink& sink) const;
    /// @brief 返回实际机体所属角色的对话台词；shot 使用四组合编号，底层存储须在展示期间有效。
    [[nodiscard]] virtual std::span<const DialogueLine> Dialogue(bool afterBattle = false, ShotType shot=ShotType::HomingAmulet) const = 0;
    /// @brief 返回当前对话的原生演出时间轴；缺省空表沿用通用的逐句强调。
    [[nodiscard]] virtual std::span<const DialogueCue> DialogueCues(bool, ShotType) const { return {}; }
    /// @brief 返回单句台词的展示秒数。
    [[nodiscard]] virtual float DialogueLineDuration() const = 0;
    /// @brief 返回玩家直射弹的外观 ID。
    [[nodiscard]] virtual VisualId PlayerBulletVisual() const = 0;
    /// @brief 返回玩家追踪弹的外观 ID。
    [[nodiscard]] virtual VisualId PlayerHomingVisual() const = 0;
    /// @brief 返回梦之御札侧针的外观 ID；未配置独立针素材的关卡沿用主弹外观。
    [[nodiscard]] virtual VisualId PlayerNeedleVisual() const { return PlayerBulletVisual(); }
    /// @brief 返回实际机体左/右副机外观；side 为 0/1，未配置时返回 invalid，不绘制该物件。
    [[nodiscard]] virtual VisualId PlayerOptionVisual(int side, ShotType shot=ShotType::HomingAmulet) const {
        (void)side; (void)shot; return kInvalidVisualId;
    }
    /// @brief 返回实际机体的主副弹及命中外观；默认实现兼容只配置灵梦素材的关卡。
    [[nodiscard]] virtual PlayerShotVisuals PlayerVisuals(ShotType shot) const {
        PlayerShotVisuals result;result.primary=PlayerBulletVisual();
        result.secondary.fill(shot==ShotType::Needle ? PlayerNeedleVisual() : PlayerHomingVisual());
        return result;
    }
    /// @brief 返回对应 Bomb 的四层原生动画外观；未实现项为 invalid，不构造替代贴图。
    [[nodiscard]] virtual std::array<VisualId,4> PlayerBombVisuals(ShotType) const {
        return {kInvalidVisualId,kInvalidVisualId,kInvalidVisualId,kInvalidVisualId};
    }
    /// @brief 返回 Power 道具的外观 ID，贴图由当前 Stage 的素材清单持有。
    [[nodiscard]] virtual VisualId PowerPickupVisual() const = 0;
    /// @brief 返回大 P 道具外观；未区分大小的关卡默认复用普通 P 图。
    [[nodiscard]] virtual VisualId LargePowerPickupVisual() const { return PowerPickupVisual(); }
    /// @brief 返回分数道具的外观 ID，贴图由当前 Stage 的素材清单持有。
    [[nodiscard]] virtual VisualId ScorePickupVisual() const = 0;
    /// @brief 返回清弹星点的外观 ID，贴图由当前 Stage 的素材清单持有。
    [[nodiscard]] virtual VisualId StarPickupVisual() const = 0;
    /// @brief 按实际机体及关卡局部表情编号返回玩家立绘；未知表情回退到该角色默认表情 0。
    [[nodiscard]] virtual VisualId HeroPortraitVisual(int expression = 0, ShotType shot=ShotType::HomingAmulet) const = 0;
    /// @brief 按关卡局部表情编号返回 Boss 立绘；未知编号回退到默认表情 0。
    [[nodiscard]] virtual VisualId BossPortraitVisual(int expression = 0) const = 0;
    /// @brief 返回符卡演出的完整立绘；默认沿用 Boss 默认表情，关卡可独立选择差分。
    [[nodiscard]] virtual VisualId SpellPortraitVisual() const { return BossPortraitVisual(); }
    /// @brief 返回关卡开始提示。
    [[nodiscard]] virtual const char* StartTitle() const = 0;
    /// @brief 常驻题签的编号、中文名与副标题；未配置时沿用开场标题。
    [[nodiscard]] virtual StageDisplayInfo DisplayInfo() const { return {"STAGE", StartTitle(), {}}; }
    /// @brief 返回胜利结算标题。
    [[nodiscard]] virtual const char* ClearTitle() const = 0;
    /// @brief 返回 Boss 出场提示。
    [[nodiscard]] virtual const char* BossStartMessage(bool final) const = 0;
    /// @brief 返回道中 Boss 退场提示。
    [[nodiscard]] virtual const char* MidbossEndMessage() const = 0;
};

}  // namespace koumakyou
