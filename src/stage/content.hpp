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

class AssetManager;

/// @brief Stage 对游戏总控提出的生成与演出请求。
class StageEventSink {
public:
    virtual ~StageEventSink() = default;
    /// @brief 请求立即创建一个敌人；spec 为借用的出生配置，接收方须复制需保留的数据。
    virtual void SpawnStageActor(const behavior::Spawn& spec) = 0;
    /// @brief 请求按 battle 启动 Boss，并登记等待完整退场结束的异步事件编号；battle 借用静态内容。
    virtual void BeginStageBoss(const bosses::Battle& battle, StageEventId id) = 0;
    /// @brief 请求对话并登记异步事件编号；内容决定条件短路、加分帧和实际结束后的结算出口。
    virtual void BeginStageDialogue(StageEventId id, bool afterBattle = false,DialogueScene scene=DialogueScene::FinalBoss,
        DialogueClearPolicy clearPolicy=DialogueClearPolicy::ClearEnemiesAndBullets) = 0;
    /// @brief 请求现存、已入场且等待对话的 Boss 开战；没有可恢复遭遇时由接收方报告错误。
    virtual void ResumeStageBoss() = 0;
    /// @brief 登记当前对白的时间轴等待，signal>=0；已到达返回 true，否则实际到达后通知事件结束。
    virtual bool AwaitStageDialogueSignal(StageEventId id,int signal) = 0;
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
    /// @brief 控制对手立绘与本句名牌；名牌称号/姓名由 StageContent 提供，introduceDelay 为句内延迟秒数。
    bool showBoss = true, introduce = false;
    const char* secondLine = "";
    int expression = 0; // 当前说话人的关卡局部表情编号；另一方保留最近一句的表情。
    float textDelay = 0;
    float expressionDelay = 0; // 本句表情相对句首的生效秒数；在此之前保留上一句的差分。
    float confirmDelay = 8.0F/60; // 原 MSG 等待计数至少达到 8 帧后才接受单次确认。
    float advanceAfter = -1; // 确认后跳至本句内此秒数；负值跳到下一句，末句保留原始退场/结算等待。
    bool secondLineReplacesFirst = false; // 原脚本写入同一文字槽时保留覆盖行为，原文仍分别保存。
    float introduceDelay=0;
};

/// @brief 返回截至 lineIndex 的玩家/Boss 表情编号，数组下标分别为 0/1。
/// elapsed 为当前句内秒数，默认取完整句末；尚未发言的一方使用 0，不持有台词。
[[nodiscard]] std::array<int, 2> DialogueExpressionsAt(
    std::span<const DialogueLine> lines, std::size_t lineIndex, float elapsed = 1.0e9F);

/// @brief 原 MSG 展开后的演出指令；frame 是包含 wait 停顿的 60 Hz 实际播放帧，按时间和原指令顺序排列。
/// Portrait/Expression 的 slot=0/1 表示自机/对手；前者 value 是 ANM 中断标签，后者为关卡局部表情号。
/// 表情可提前于台词切换，不能依赖当前说话人推断；其余指令不使用 slot。
/// StageSignal 的 value 是从 1 起算、单调递增的 MSG 放行序号，供异步时间轴等待，不表示对白结束。
/// StageMusic 的 value 为关卡音乐槽 0/1/2，-1 停止；同帧指令仍按表序逐条执行。
/// BossMusic 保留既有关卡的布尔约定（true 选择 Boss 曲）；新对白优先使用 StageMusic。
/// PortraitStart明确创建slot的立绘；当前只支持value=0的普通对话脚本，创建本身即启动30帧入场。
enum class DialogueCueKind { Portrait, InputEnabled, BossMusic, Expression, StageSignal, StageMusic, PortraitStart };
struct DialogueCue { int frame; DialogueCueKind kind; int slot, value; };
/// @brief 查询已展开指令的布尔状态，未出现指令时返回 initial；seconds 是整段对话已播放秒数。
[[nodiscard]] bool DialogueFlagAt(std::span<const DialogueCue> cues, DialogueCueKind kind,
                                float seconds, bool initial = false);
/// @brief 按完整对话秒数覆盖精确表情指令；没有对应指令的槽保留 fallback，兼容旧逐句表。
[[nodiscard]] std::array<int,2> DialogueCueExpressionsAt(std::span<const DialogueCue> cues,float seconds,std::array<int,2> fallback);

/// @brief 悬浮题签的关卡名称；字符串借用内容模块，须覆盖整个关卡展示期，不包含游玩模式。
struct StageDisplayInfo {
    std::string_view label, title, subtitle;
};

/// @brief 当前关卡的取得进度与本局模式快照；captures由内容规则计分，netMisses为有符号确认Miss减Challenge残机奖励。
/// 不包含历史存档或练习成绩；只在开始对白时取值，后续演出保持同一分支。
struct StagePerformance { int captures=0; bool challenge=false; std::int64_t netMisses=0; };
/// @brief 对话的流程决定；skip立即完成事件，finishStage在实际结束后进入结算页。
/// awardFrame>=0表示先在该60 Hz内容帧固定奖励，再保持对白保护直到实际结束；-1沿用结束时加分。
/// endFrame>=0使用独立核对的整段60 Hz终点，避免长对白逐句float秒数相加改变退出半帧；-1沿用时长接口。
struct DialogueFlow { bool skip=false,finishStage=false; int awardFrame=-1,endFrame=-1; };

/// @brief 一个关卡提供自己的素材、事件、敌人、Boss 和对话内容。
class StageContent {
public:
    /// @brief 按值保存四种主线难度；非法枚举抛 invalid_argument，默认 Normal。
    explicit StageContent(native_combat::Difficulty difficulty=native_combat::Difficulty::Normal):difficulty_(difficulty) {
        if(native_combat::DifficultyIndex(difficulty)<0)throw std::invalid_argument("Invalid stage difficulty");
    }
    virtual ~StageContent() = default;
    /// @brief 本关实例的运行难度，按值保存；自适应 rank 不改变此身份。
    [[nodiscard]] native_combat::Difficulty Difficulty() const { return difficulty_; }
    /// @brief 可选最终 HP 规则，不定义接触几何；无敌目标及 Bomb 零接触不进入提供方。
    /// 默认空指针使用明确标记的基准/回退及已核对的 Bomb 规则。
    [[nodiscard]] virtual const DamageRuleProvider* DamageRules() const { return nullptr; }
    /// @brief 关联原始数值目录；默认空表示尚未绑定。DataOnly 记录仍需实现演员和时间轴才可游玩。
    [[nodiscard]] virtual const native_combat::StageData* CombatData() const { return nullptr; }
    /// @brief 返回本关卡的图片与音频清单；返回的引用由 Stage 持有，调用方不得释放。
    [[nodiscard]] virtual const StageAssetManifest& Assets() const = 0;
    /// @brief 返回按触发顺序排列的时间轴事件；底层存储须覆盖 StageDirector 的生命周期。
    [[nodiscard]] virtual std::span<const StageEvent> Events() const = 0;
    /// @brief 按事件携带的命名战斗定义创建并启动 Boss；shot 在配置解析前提供，所有权交给调用方。
    [[nodiscard]] std::unique_ptr<Boss> CreateBoss(const bosses::Battle& battle,ShotType shot=ShotType::HomingAmulet) const {
        if(!battle.create)throw std::invalid_argument("Missing Boss factory");
        return battle.create(shot,difficulty_);
    }
    /// @brief 符卡练习使用的完整战斗入口；配置存储由关卡静态持有。
    [[nodiscard]] virtual const bosses::Battle& PracticeBoss() const = 0;
    /// @brief 按原典符卡编号选择练习遭遇；返回静态定义，未实现或与当前难度不符须抛 invalid_argument。
    /// 调用方仍须按目录的 attack 进入具体技能，不能把道中符卡当作关底技能。
    [[nodiscard]] virtual const bosses::Battle& PracticeBoss(int) const {
        throw std::invalid_argument("Spell practice encounter not implemented");
    }
    /// @brief 按当前运行难度复制出生配置；关卡可覆盖以选择已验证的难度配方。
    [[nodiscard]] virtual behavior::Spawn ConfigureSpawn(const behavior::Spawn& spawn) const {
        auto result=spawn;result.difficulty=difficulty_;
        if(result.pattern.configureDifficulty)result.pattern.configureDifficulty(result.pattern,difficulty_);
        return result;
    }
    /// @brief 执行调度器交付的一次动作；出怪请求必须已规范为 count=1。
    /// @return Boss 与对话返回 false，结束后通过 StageDirector::EndEvent 报告；其他返回 true。
    /// @throws std::invalid_argument 未规范的批次、缺少异步 ID 或动作参数非法。
    bool HandleEvent(const StageEvent& event, StageEventSink& sink) const;
    /// @brief 返回实际机体所属角色的对话台词；shot 使用四组合编号，底层存储须在展示期间有效。
    [[nodiscard]] virtual std::span<const DialogueLine> Dialogue(bool afterBattle = false, ShotType shot=ShotType::HomingAmulet) const = 0;
    /// @brief 返回当前对话的原生演出时间轴；缺省空表沿用通用的逐句强调。
    [[nodiscard]] virtual std::span<const DialogueCue> DialogueCues(bool, ShotType) const { return {}; }
    /// @brief 按遭遇选择对话；默认仅提供既有的关底对白，道中须由内容明确覆盖，不能误播关底台词。
    [[nodiscard]] virtual std::span<const DialogueLine> SceneDialogue(DialogueScene scene,bool afterBattle,ShotType shot) const {
        return scene==DialogueScene::FinalBoss ? Dialogue(afterBattle,shot) : std::span<const DialogueLine>{};
    }
    [[nodiscard]] virtual std::span<const DialogueCue> SceneDialogueCues(DialogueScene scene,bool afterBattle,ShotType shot) const {
        return scene==DialogueScene::FinalBoss ? DialogueCues(afterBattle,shot) : std::span<const DialogueCue>{};
    }
    /// @brief 无台词演出也可保持等待；返回非负有限秒数，整段时长取此值与台词总长的较大者。
    [[nodiscard]] virtual float DialogueMinimumDuration(DialogueScene,bool,ShotType) const {return 0;}
    /// @brief 成功取得一张符卡时更新本关进度；previous>=0，card为1起算原典全局编号。
    /// Game只为非练习的实际成功结算调用一次；默认有效编号每张加一，未绑定的0不计，关卡可定义特殊计分。
    [[nodiscard]] virtual int CaptureProgress(int previous,int card) const {return card>0?previous+1:previous;}
    /// @brief 按开始对白时的本局成绩选择流程；默认仅关底战后结束关卡，返回值不持有快照。
    [[nodiscard]] virtual DialogueFlow ResolveDialogueFlow(DialogueScene scene,bool after,ShotType,const StagePerformance&) const {
        return {.finishStage=scene==DialogueScene::FinalBoss&&after};
    }
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
    /// @brief 脚本显式道具的外观；item 为原生 0～6，默认只配置既有 P/分数/星点，其余返回 invalid。
    [[nodiscard]] virtual VisualId ItemVisual(int item) const {
        return item==0 ? PowerPickupVisual() : item==1 ? ScorePickupVisual() : item==2 ? LargePowerPickupVisual() :
               item==6 ? StarPickupVisual() : kInvalidVisualId;
    }
    /// @brief 在3D通道绘制本关装饰；seconds为冻结暂停后的秒数，assets只在本次调用中借用。
    /// 不得推进逻辑、加载/释放素材或保留纹理引用；卸载后的空素材也须安全处理。
    virtual void DrawScenery(float seconds,const AssetManager& assets) const { (void)seconds; (void)assets; }
    /// @brief 按实际机体及关卡局部表情编号返回玩家立绘；未知表情回退到该角色默认表情 0。
    [[nodiscard]] virtual VisualId HeroPortraitVisual(int expression = 0, ShotType shot=ShotType::HomingAmulet) const = 0;
    /// @brief 按关卡局部表情编号返回 Boss 立绘；未知编号回退到默认表情 0。
    [[nodiscard]] virtual VisualId BossPortraitVisual(int expression = 0) const = 0;
    /// @brief 道中与关底对手不同时分别选择关卡立绘；默认沿用当前 Boss 立绘接口。
    [[nodiscard]] virtual VisualId SceneBossPortraitVisual(DialogueScene,int expression=0) const {return BossPortraitVisual(expression);}
    /// @brief 返回符卡演出的完整立绘；默认沿用 Boss 默认表情，关卡可独立选择差分。
    [[nodiscard]] virtual VisualId SpellPortraitVisual() const { return BossPortraitVisual(); }
    /// @brief 按实际宣告的全局符卡编号选择演出角色；默认沿用关底立绘，支持道中与关底不同角色。
    [[nodiscard]] virtual VisualId SpellPortraitVisual(int spellCard) const { (void)spellCard; return SpellPortraitVisual(); }
    /// @brief Boss 对话名牌的称号与姓名；字符串由内容持有，空项不绘制对应文字。
    [[nodiscard]] virtual std::array<std::string_view,2> BossIntroduction() const { return {}; }
    /// @brief 返回关卡开始提示。
    [[nodiscard]] virtual const char* StartTitle() const = 0;
    /// @brief 常驻题签的编号、中文名与副标题；未配置时沿用开场标题。
    [[nodiscard]] virtual StageDisplayInfo DisplayInfo() const { return {"STAGE", StartTitle(), {}}; }
    /// @brief 返回胜利结算标题。
    [[nodiscard]] virtual const char* ClearTitle() const = 0;
private:
    native_combat::Difficulty difficulty_;
};

}  // namespace koumakyou
