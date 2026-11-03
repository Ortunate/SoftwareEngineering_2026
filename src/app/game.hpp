// 本文件负责声明游戏总控类、运行状态及各系统的协作入口。

#pragma once

#include "resources/asset_manager.hpp"
#include "gameplay/actors/boss.hpp"
#include "gameplay/bullets/system.hpp"
#include "gameplay/actors/behavior.hpp"
#include "app/run_state.hpp"
#include "app/start_setup.hpp"
#include "app/stage_clear.hpp"
#include "app/spell_practice.hpp"
#include "config/settings.hpp"
#include "config/high_scores.hpp"
#include "narrative/ending.hpp"
#include "gameplay/actors/player.hpp"
#include "gameplay/items/drop_system.hpp"
#include "gameplay/effects/player_bomb.hpp"
#include "gameplay/arena.hpp"
#include "stage/system.hpp"
#include "stage/content.hpp"
#include "stage/catalog.hpp"
#include "render/sprite_renderer.hpp"
#include "render/combat_renderer.hpp"
#include "gameplay/collision/projectile_collision.hpp"
#include "ui/motion.hpp"
#include "ui/name_entry.hpp"
#include "ui/page_motion.hpp"
#include "render/start_setup_motion.hpp"
#include "render/dialogue_motion.hpp"
#include "render/title_entrance.hpp"

#include "raylib.h"
#include "replay/replay.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <memory>
#include <string>
#include <vector>

namespace koumakyou {

/// @brief 游戏总控，协调输入、关卡、战斗、碰撞、资源和界面。
class Game : private StageEventSink {
public:
    /// @brief 加载通用素材与设置，并进入主菜单。
    Game();
    /// @brief 释放当前关卡及通用素材。
    ~Game();

    /// @brief 重置游戏并返回主菜单。
    void Reset();
    /// @brief 从文件播放已兼容录像；失败返回 false 并保留中文错误提示，view=0/1/2。
    bool OpenReplayFile(const std::filesystem::path& path,int view=0);
    /// @brief 从第一面开头开始一局战斗，先进入可持续更新的加载页。
    /// @param captureCursor 是否为交互游玩捕获鼠标；截图时传 false。
    void Start(bool captureCursor = true,native_combat::Difficulty difficulty=native_combat::Difficulty::Normal);
    /// @brief 选择关卡并进入加载页；Campaign继续主线，Practice独立结算，Extra仅接受Extra关卡/Normal上下文。
    /// 不支持的难度或入口抛 invalid_argument；SpellPractice 必须使用独立符卡练习入口。
    void StartStage(StageId id,bool captureCursor=true,native_combat::Difficulty difficulty=native_combat::Difficulty::Normal,
                    StageEntry entry=StageEntry::Campaign);
    /// @brief 是否处于加载页（含失败提示）；截图方须等待完成，并检查 HasLoadingFailed。
    [[nodiscard]] bool IsLoading() const { return state_ == RunState::Loading; }
    /// @brief 加载失败时返回 true；交互用户可返回首页，截图调用方应结束并报告失败。
    [[nodiscard]] bool HasLoadingFailed() const { return IsLoading() && loadingFailed_; }
    /// @brief 进入不捕获鼠标、长时间无敌的关卡预览模式。
    /// 保留当前主线关卡；上一局为Extra时回到一面，不改写持久化的主线难度选择。
    void StartPreview();
    /// @brief 设置关卡预览的相机观察角度。
    /// @param yawDegrees 水平角（度）。
    /// @param pitchDegrees 仰俯角（度）。
    void SetPreviewView(float yawDegrees, float pitchDegrees);
#ifdef KOUMAKYOU_DEBUG_FEATURES
    /// @brief 设置是否绘制判定球与发弹点。
    /// @param enabled 为 true 时启用调试图层。
    void SetDebugDraw(bool enabled) { debugRender_ = enabled; }
#endif
    /// @brief 进入设置菜单并显示鼠标，保留可见指针的位置，不强制居中。
    void OpenOptions();
    /// @brief 按当前状态更新输入、关卡与战斗逻辑。
    /// @param deltaTime 本帧经过的秒数。
    /// @param playbackElapsed 播放预算使用的真实秒数；负值表示沿用 deltaTime，正常模拟仍只使用 deltaTime。
    void Update(float deltaTime, float playbackElapsed = -1.0F);
    /// @brief 根据当前状态绘制游戏画面。
    void Draw() const;
    /// @brief 查询主菜单是否要求退出程序。
    /// @return 应结束主循环时返回 true。
    [[nodiscard]] bool ShouldQuit() const { return shouldQuit_; }

private:
    // 独立离屏集成测试通过此探针检查生命周期，正式程序不暴露可修改的战斗状态。
    friend struct GameLifecycleProbe;
    /// @brief 返回当前关卡中该道具的外观，值区分大/小 P；不加载资源或推进动画。
    [[nodiscard]] VisualId PickupVisual(const Pickup& pickup) const;
    /// @brief 同时可保有的最多敌人数。
    static constexpr std::size_t kActorCapacity = 128;
    /// @brief 设置界面的普通设置页与按键设置页。
    enum class OptionsPage {
        General,
        KeyBindings,
    };

    /// @brief 进入关卡加载页；素材完成后重置本关战斗。同步入口只供预览和测试使用。
    /// @param captureCursor 是否捕获鼠标用于交互游玩。
    /// @param continuingStage 连续闯关保留分数、机体、库存和累计统计；普通开局/重开为 false。
    void StartRun(bool captureCursor = true, bool synchronous = false,bool continuingStage=false);
    /// @brief 非阻塞推进加载；失败后留在加载页，按确认或取消返回首页。
    void UpdateLoading(float deltaTime);
    /// @brief 素材就绪后初始化本局状态，避免加载耗时进入战斗计时或提前捕获鼠标。
    void CompleteStartRun();
    /// @brief 切换主菜单并清除战斗临时状态，保留离开首页时的选项。
    void EnterMainMenu();
    /// @brief 清理本局资源并返回整关练习的 Stage 页，保留御札、模式和关卡；其他模式回首页。
    void ReturnToPracticeStage();
    /// @brief 固定本关奖励及加分，允许内容在对白中提前触发；每关只生效一次，不结束对白保护。
    void AwardStageClear();
    /// @brief 战后对话实际结束后显示已固定奖励明细；未提前加分时先结算，重入不重置倒计时。
    void BeginStageClear();
    /// @brief 关闭奖励明细；本篇按难度终点衔接关卡或结局，Extra独立选存，整关练习收尾后返回选关。
    void ContinueStageClear();
    /// @brief 末关退场及旧资源释放后加载所选结局；内容和音乐沿用异步加载通道。
    void BeginEnding();
    /// @brief 只推进结局时钟；播放完毕或合法跳过后进入选存，不经过制作人员或旧胜利菜单。
    void UpdateEnding(float deltaTime);
    /// @brief 固定本轮成绩快照、保存观看标记并打开选存页，不立即插入排行榜。
    void FinishEnding();
    /// @brief 清理已结束关卡并固定本轮成绩，进入命名选存；保留调用方设置的存盘错误提示。
    void BeginScoreSave();
    /// @brief 排行榜索引为主线E/N/H/L的0～3或Extra的4；独立于Extra内部Normal脚本掩码。
    [[nodiscard]] int ScoreDifficultyIndex() const {return stageId_==StageId::Extra?4:native_combat::DifficultyIndex(runDifficulty_);}
    /// @brief 处理选存、ASCII 命名与成绩页筛选；输入、存盘与绘制互相独立。
    void UpdateScoreSave();
    void UpdateScoreName();
    /// @brief 查询先选难度，按四机体翻页并列查看标准/挑战成绩；返回逐级退回，本局保存后仍继续Demo选存。
    void UpdateScorePage();
    /// @brief 确认名字后尝试原子保存；失败保留成绩及名字并提示重试，成功后转到对应排行榜。
    void SaveNamedScore();
    /// @brief 把有效主线整关成绩写入对应模式的历史最高分；通关或失败时调用，Extra使用独立命名表。
    void RecordStageScore();
    /// @brief 保存有效整关的分模式最高分并播放退场效果；池空并卸载素材后才进入结局或结算出口。
    /// @param result 胜利或失败后的游戏状态。
    void EndStage(RunState result);
    /// @brief 在 Playing/StageClear/Finishing 与 Paused 间切换，并同步音乐和鼠标状态。
    void TogglePause();
    /// @brief 立即取消全部小怪并生成清场快照，不给予击破奖励。
    void ClearActorsWithEffects();
    /// @brief 从本地文件读取全部设置并应用音量、灵敏度。
    void LoadSettings();
    /// @brief 将当前设置、成绩及独立的开局选择记录写入本地文件。
    void SaveSettings() const;
    /// @brief 处理主菜单的移动、点击与确认输入。
    void UpdateMainMenu();
    /// @brief 处理状态输入与战斗；由 Update 包装，确保所有提前返回后仍推进 UI 展示。
    void UpdateState(float deltaTime);
    /// @brief 在输入生效后同步菜单及对话动效，deltaTime 为秒；对话过渡暂停时冻结，不推进战斗时钟。
    void UpdateUiMotion(float deltaTime);
    /// @brief 从演员/弹幕的逻辑年龄同步原 ANM，冻结暂停帧，不改变战斗对象寿命。
    void UpdateBattleAnimations();
    /// @brief 处理 Normal 开局的角色/御札/模式选择，以及成绩、音乐和制作信息页。
    void UpdateMenuPage();
    /// @brief 处理符卡练习 Boss 页及同页符卡/难度的两级焦点，鼠标可直接跨栏；返回优先于本帧确认。
    void UpdateSpellPractice();
    /// @brief 清理战场并返回符卡组选择，保留角色、关卡和难度，不重置选择页动画。
    void ReturnToSpellPractice();
    /// @brief 宣告已实现符卡时建立并保存本篇/练习尝试；本篇遇见记录只解锁本次角色/御札组合。
    /// 每阶段最多打开一条记录，预览、对话与调试靶场不写入。
    void BeginSpellRecord();
    /// @brief 结束当前尝试并存档，重复调用无副作用；capture 须排除受击、Bomb 和超时。
    void FinishSpellRecord(bool capture);
    /// @brief 当前Boss已实际宣告符卡时固定Miss/Bomb基准、切入演出和遇见记录；不在准备期提前调用。
    /// 外部Resume或新Boss可同步宣告，不能只依靠Update前后编号变化发现首张符卡。
    void OnSpellDeclared();
    /// @brief 使用现有页面底板、字体、花饰和强调绘制独立符卡练习；opacity 为流程过渡权重。
    void DrawSpellPractice(float opacity) const;
    /// @brief 处理新典顺序的开局页；角色箭头只切换，只有已实现机体的最终确认才启动加载。
    void UpdateStartSetup();
    /// @brief 处理 FPS、音量、灵敏度、开局资源与设置菜单页面的输入。
    void UpdateOptions();
    /// @brief 处理按键绑定列表与新按键录入。
    void UpdateKeyBindings();
    /// @brief 按大小切换 FPS 档位并保存；30 为下限，Unlimited 为上限，到边界不循环。
    /// @param offset 档位变化量，通常为 -1 或 1。
    void AdjustFpsLimit(int offset);
    /// @brief 调节当前数值项并立即保存；offset 正负控制方向，音量步进 5%，灵敏度步进 10%。
    void AdjustOption(int offset);
    /// @brief 消费本步 runInput_，委托 Player 更新，并处理 Bomb 对战场的影响。
    /// @param deltaTime 本帧经过的秒数。
    void UpdatePlayer(float deltaTime);
    /// @brief 每显示帧采集一次键鼠状态；固定战斗步消费缓存，鼠标和 Bomb 边沿不会重复或丢失。
    [[nodiscard]] PlayerInput ReadPlayerInput() const;
    /// @brief 使用调用方提供的输入快照推进一个战斗步；鼠标增量和 Bomb 首次按下只允许消费一次。
    void UpdatePlayer(float deltaTime,const PlayerInput& input);
    /// @brief 按 1/120 秒推进关卡、对话、演员、弹池、Bomb 与碰撞；先行为后碰撞，再即时交接阶段。
    void UpdateBattleStep(float deltaTime,const PlayerInput& input,bool dialogueConfirm,bool fastForward);
    /// @brief 推进全部活动敌人的航线及技能计时器。
    /// @param deltaTime 本帧经过的秒数。
    void UpdateActors(float deltaTime);
    /// @brief 推进 Boss 当前阶段，超时则请求阶段切换。
    /// @param deltaTime 本帧经过的秒数。
    void UpdateBoss(float deltaTime);
    /// @brief 推进普通道具与星点，将拾取结果计入玩家 Power 与分数，并处理升满奖励。
    void UpdatePickups(float deltaTime);
    /// @brief 增减玩家 Power，首次升满时显示提示并将当前敌弹转成自动吸取的星点。
    /// @param delta Power 点数，可为负；下降后再次升满可重新触发。
    void AdjustPlayerPower(int delta);
    /// @brief 合并敌弹与敌人/Boss 本体对玩家的同一战斗步命中，再处理擦弹及敌方受伤；对话不判定本体接触。
    void ResolveCollisions();
    /// @brief 将关卡事件转换为出怪、对话或 Boss 战操作。
    /// @param event 已到触发时刻的关卡事件。
    /// @return 同步完成的事件返回 true；Boss 战和对话返回 false。
    bool HandleStageEvent(const StageEvent& event);

    /// @brief 将出生配置直接交给行为演员池；Boss 活动期间跳过该次生成请求。
    /// @param spec 按值复制的出生位置、行为配方及战斗和视觉配置。
    void SpawnStageActor(const behavior::Spawn& spec) override;
    /// @brief 开始 Boss 战并停止旧批次后续出怪，保留现存小怪、弹幕、道具及效果。
    void BeginStageBoss(const bosses::Battle& battle, StageEventId id) override;
    /// @brief 开始原文对话；战后独白结束才固定 Bonus 并进入奖励明细，关闭后进入胜利收尾。
    void BeginStageDialogue(StageEventId id, bool afterBattle = false,DialogueScene scene=DialogueScene::FinalBoss,
        DialogueClearPolicy clearPolicy=DialogueClearPolicy::ClearEnemiesAndBullets) override;
    void ResumeStageBoss() override;
    bool AwaitStageDialogueSignal(StageEventId id,int signal) override;
    /// @brief 更新单个演员及其音画/道具输出；调用方负责在池遍历结束后消费子演员请求。
    void UpdateActor(behavior::Actor& actor,float deltaTime);
    /// @brief 复制并生成行为请求，立即执行出生内容帧；仅在 ActorPool 遍历之外调用，池满仍丢弃本次请求。
    void SpawnBehaviorActors(std::span<const behavior::Spawn> requests,float deltaTime);
    void StageMessage(std::string_view message, float duration) override;
    /// @brief 消耗 Bomb 后启动独立攻击和全道具吸取，仅按初始保护实体清弹，不立即全场扣血。
    void ApplyBombEffect();
    /// @brief 在敌人和敌弹推进后、玩家碰撞前逐 60 Hz 帧推进 Bomb，所有命中均由真实空间实体决定。
    void UpdatePlayerBomb(float deltaTime);
    /// @brief 结算当前 Bomb 逻辑帧；只将真实接触交给伤害提供方，并反馈光球原始累计，同一帧重复调用无效。
    void ResolvePlayerBombFrame();
    /// @brief 仅结算当前冻结/活动 Bomb 区域的伤害，不推进效果、不清弹；调用方按 60 Hz 接触时钟交付。
    void ResolvePlayerBombDamage();
    /// @brief 只读取当前活动遭遇的世界时停，不包含暂停菜单或加载状态。
    bool WorldTimeStopped() const {return boss_&&boss_->active&&boss_->WorldTimeStopped();}
    /// @brief 玩家命中后更新场景反馈，并按剩余残机处理游戏失败。
    void HandlePlayerHit();
    /// @brief 结算当前 Boss 阶段并进入下一阶段或一面结算。
    void AdvanceBossSkill();
    /// @brief 设置屏幕提示文字及显示时间。
    /// @param message 要展示的文字。
    /// @param duration 持续秒数。
    void SetMessage(std::string message, float duration);

    /// @brief 按设置菜单索引读取对应按键。
    /// @param index 按键绑定列表中的索引。
    /// @return 统一键盘/鼠标编码；索引无效时返回 KEY_NULL。
    [[nodiscard]] int BindingAt(int index) const;
    /// @brief 修改指定动作的键码并处理键位冲突。
    /// @param index 动作索引。
    /// @param key 新统一键盘/鼠标编码。
    void SetBinding(int index, int key);
    /// @brief 将统一键盘/鼠标编码转换为界面可读文字。
    /// @param key 统一键盘/鼠标编码。
    /// @return 中文界面键名，保留字母、数字与 Shift/Ctrl/Alt 等键帽标识；未知编码显示按键编号。
    [[nodiscard]] static std::string KeyLabel(int key);

    /// @brief 绘制天空盒、战场与空间中的敌人和弹幕。
    void DrawWorld() const;
    /// @brief 绘制战场长方体边框和六个表面的网格。
    void DrawArena() const;
    /// @brief 绘制两侧悬浮题签/状态卡、Boss 信息及窗口中央准星；不占用 3D 视口。
    void DrawHud() const;
    /// @brief 绘制状态消息、对话、符卡或结算画面的叠加层。
    void DrawOverlay() const;
    /// @brief 在战斗/对话画面上绘制固定结算快照，不推进计时或再次加分。
    void DrawStageClear() const;
    /// @brief 仅借用已加载的结局资源与播放快照，保持原始画幅和中文换行。
    void DrawEnding() const;
    /// @brief 绘制选存/命名或浏览页；布局与命中区域共用，函数不推进输入或写入存档。
    void DrawScoreEntry() const;
    /// @brief opacity为整页透明度，motion借用该页独立展示；page=0为难度札，1～4为机体页，-1采用当前身份。
    void DrawScorePage(float opacity=1,const ui::MenuMotion* motion=nullptr,int page=-1) const;
    /// @brief 在窗口右侧等比滑入完整符卡立绘，独立横幅显示名称。
    void DrawSpellCutIn() const;
    /// @brief Bomb 背景压暗在透明战斗对象之前绘制，保持原生加色光效；须在 BeginMode3D 内调用。
    void DrawPlayerBombBackdrop(float width,float height) const;
    /// @brief 使用对应自机重绘图展示短暂宣告，名称保持至演出结束；暂停只读取冻结采样。
    void DrawPlayerBombCutIn() const;
    /// @brief 播放最近一次 Bomb Start/Update 的原始音效事件，同一更新中的重复编号只播放一次。
    void PlayPlayerBombSounds();
    /// @brief 取消 Bomb 攻击、世界演出和声音，并丢弃暂停标记；卸载关卡和切换状态之前调用。
    void ClearPlayerBomb();
    /// @brief 暂停/恢复确实在播放的 Bomb 音效，避免恢复时播放从未启动的声音。
    void PausePlayerBombSounds(bool paused);
    /// @brief 根据对话计时器绘制台词及最近表情，以预先采样的透明度和强调程度绘制贴边立绘。
    void DrawDialogue() const;
    /// @brief 绘制开始菜单；motion 可借用离开首页前的展示快照，供 Start 跨页混合使用，空指针读取当前展示。
    void DrawMainMenu(const ui::MenuMotion* motion = nullptr) const;
    /// @brief 只使用通用资源绘制“少女祈祷中”；加载中的关卡纹理不可访问。
    void DrawLoading() const;
    /// @brief 在最终叠加层统一展示当前页面的功能按键；战斗键位读取本机设置，横条不接收点击。
    void DrawCommandBar() const;
    /// @brief 绘制指定音乐或制作信息页；opacity为0～1，motion可借用退场页展示，不访问已卸载的音频。
    void DrawMenuPage(RunState page,float opacity=1,const ui::MenuMotion* motion=nullptr) const;
    /// @brief 按各子页当前权重混合首页与页面；只读展示快照，不创建离屏资源或修改业务状态。
    void DrawTitlePages() const;
    /// @brief 绘制开局的难度、人物、选择及确认层；只借用通用纹理，不提前加载关卡资源。
    void DrawStartSetup() const;
    /// @brief 绘制 FPS、音量、灵敏度、开局资源和按键设置入口。
    void DrawOptions(float opacity=1,const ui::MenuMotion* motion=nullptr) const;
    /// @brief 绘制可重新绑定的战斗按键列表。
    void DrawKeyBindings(float opacity=1,const ui::MenuMotion* motion=nullptr) const;
#ifdef KOUMAKYOU_DEBUG_FEATURES
    /// @brief 进入禁用关卡时间轴、只有固定靶怪的射击调试场地。
    void StartShotTest();
#endif

    /// @brief 采集一份逻辑输入；播放时由文件提供，避免多步播放重复消费实体按键。
    replay::Frame ReadRunInput(float dt) const;
    /// @brief 一次实际模拟更新，包含影响退场和碰撞的动画同步。
    void StepRun(float dt);
    /// @brief 初始化录制；仅交互正式关卡启用，随机种子在初始化掉落前设置。
    void PrepareReplay();
    /// @brief 当前开局配置和终局摘要，不拥有任何关卡资源。
    replay::Header ReplayHeader() const;
    replay::Result ReplayResult() const;
    /// @brief 打开录像列表并释放旧关卡；保留状态文字，失败文件也可查看原因。
    void OpenReplayList();
    /// @brief 处理列表分页选择、播放及返回；坏文件不允许启动。
    void UpdateReplayList();
    /// @brief 根据缓存头部说明所选槽的版本错误；选择不加载逐步输入，启动前才完整读取。
    void RefreshReplaySelection();
    /// @brief 扫描60个固定槽并追加旧命名文件；列表只缓存有界元数据，启动仍须完整校验。
    void ReadReplayDirectory();
    /// @brief 保存选槽与播放列表共享键鼠导航；返回本帧是否确认，不处理外层取消。
    bool UpdateReplayTable(const ui::MenuInput& input,bool saving);
    /// @brief 绘制槽号、名称、日期、机体、难度、分数及进度；只读缓存，不访问文件。
    void DrawReplayTable(bool saving,float opacity=1,const ui::MenuMotion* motion=nullptr) const;
    /// @brief 已成功写入Demo后记住共享名称；记忆文件失败仍保留会话内初值并返回false。
    bool RememberName(const std::string& name);
    /// @brief 绘制缓存的录像元数据，不在绘制路径访问文件系统。
    void DrawReplayList() const;
    /// @brief 从已校验文件启动关卡；退出时恢复原本的开局设置。
    void PlayReplay(replay::Data data);
    /// @brief 退出播放或加载失败时还原开局配置；未播放时无副作用。
    void RestoreReplaySettings();
    /// @brief 从录像首片段恢复入口，倒带时必须先恢复再重新加载，不能从当前关卡重开。
    void RestoreReplayEntry();
    std::size_t replaySegment_=0; // 播放中的片段索引；加载时间不占输入步，跨关后递增。
    replay::Mode replayMode_=replay::Mode::Idle;
    replay::Data replayData_{};
    replay::Frame runInput_{};
    std::optional<replay::Header> replayOriginal_;
    StageId replayOriginalStage_=StageId::Stage1;
    int replayOriginalDifficulty_=1;
    spell::Selection replayOriginalSpell_{};
    // 四个并行数组下标一致：前60项为固定槽，后续为旧文件；选槽只允许前60项。
    std::vector<std::filesystem::path> replayFiles_;
    std::vector<std::optional<replay::Summary>> replayRows_;
    std::vector<std::string> replayRowErrors_; // 占用而无法解析的槽也不能未经确认覆盖。
    std::vector<bool> replayOccupied_;
    int selectedReplay_=0;
    bool replayOriginalDebug_=false;
    std::size_t replayCursor_=0;
    double replayBudget_=0; // 秒；只累计未暂停的真实播放时间，不能跳过录像步骤。
    bool replayPaused_=false, replayEnded_=false;
    std::string replayStatus_;
    /// @brief 观战状态只驱动渲染，秒数索引指向原始步骤边界；定位不修改录像。
    Camera3D observer_{};
    int replayView_=0,replaySpeed_=2;
    float orbitYaw_=0,orbitPitch_=0.2F,orbitDistance_=3;
    std::vector<double> replayTimes_;
    std::size_t replayTarget_=0;
    bool replaySeeking_=false,replayFinished_=false;
    // Ready 只在最终步骤封存后设置；Declined 可手动重开询问，Saved 不得重复保存。
    enum class ReplaySaveState { None, Ready, Ask, Slots, Overwrite, Naming, Failed, Declined, Saved };
    enum class ReplayAction { Retry, Menu, StageSelection, SpellSelection, ContinueStage, Stay };
    ReplaySaveState replaySaveState_=ReplaySaveState::None;
    ReplayAction replayAfterSave_=ReplayAction::Stay;
    int replayChoice_=0;
    int replaySaveSlot_=0; // 1～60，0表示尚未选择；只有Overwrite确认后才允许替换已有文件。
    bool replayOverwrite_=false,replayNamePrepared_=false;
    ui::NameEntryState replayNameEditor_{};
    ui::MenuMotion replaySaveMotion_{};
    bool replayAvailable_=false,replayMenuFocused_=false;
    float replayScanSeconds_=1;
    /// @brief 仅检查录像文件存在性，启动、返回、聚焦和主菜单每秒刷新，不在 Draw 中访问文件系统。
    void RefreshReplayAvailability(float seconds=0,bool force=false);
    [[nodiscard]] bool MainMenuItemAvailable(int index) const;
    /// @brief 保存资格同时供菜单和请求使用；拒绝后仍允许手动保存。
    [[nodiscard]] bool CanSaveReplay() const;
    [[nodiscard]] bool ReplaySaveModalOpen() const;

    std::string replayName_;
    battle::Cursor playerBodyAnimation_{};
    int playerBodyAction_=0;
    double playerBodySince_=0,replayMusicSince_=0;
    int replayMusicSlot_=0;
    bool replayAudioMuted_=true;
    /// @brief 请求离开结果页；未保存时先询问，动作表示重试、返回首页/选择页或留在结果。
    void RequestReplayAction(ReplayAction action);
    void FinishReplayAction();
    void UpdateReplaySave();
    void DrawReplaySave() const;
    void UpdateObserver(float seconds);
    void SeekReplay(double seconds);
    void DrawReplayBar() const;
    Camera3D camera_{};
    RunState state_ = RunState::MainMenu;
    float loadingSeconds_ = 0; // 秒；独立 UI 时钟，不能参与关卡/弹幕/符卡计时。
    bool loadingStarted_ = false;
    bool loadingFailed_ = false;
    bool continuingStage_=false; // 仅属于当前加载任务；进入主菜单或重新开局时清除，不借用旧关卡对象。
    // 只有本局依次完成一至六面才形成 0x3f；直接启动末关、练习、预览不能伪造完整主线通关。
    unsigned clearedMainlineStages_=0;
    int runContinues_=0; // 当前尚无续关入口；以后接入续关时须递增，不能把挑战模式直接当作续关。
    std::optional<ending::Id> pendingEnding_;
    ending::Player endingPlayer_;
#ifdef KOUMAKYOU_DEBUG_FEATURES
    bool loadingShotTest_ = false; // 射击测试需在异步素材完成后再创建固定靶。
#endif
    // 暂停记住战斗、奖励明细或收尾，不能一律恢复 Playing；finishResult_ 仅在收尾结束时应用。
    RunState pausedState_ = RunState::Playing;
    RunState finishResult_ = RunState::Won;
    // 当前一局是否捕获鼠标，预览和离屏测试在恢复暂停时仍不捕获。
    bool captureCursor_ = true;
    Player player_{};
    // 只借用本关卡图集；卸载前重置，动画年龄来自 Player，暂停与复活均沿用玩家时间基准。
    std::array<battle::Cursor, 2> playerOptionAnimations_{};
    std::unique_ptr<Boss> boss_{};
    BulletSystem bullets_{};
    behavior::ActorPool<kActorCapacity> actors_{};
    ActorEffectSystem actorEffects_{};
    effects::ExternalEffectSystem externalEffects_{}; // 独立512槽，StartRun/退出/卸载前必须解除ANM借用。
    PlayerBombEffect playerBomb_{}; // 仅借用关卡 ANM；退出、重开、结算及卸载资源前清空。
    bool playerBombStartedThisFrame_=false;
    int playerBombResolvedFrame_=-1; // 只属于当前 Bomb，Start/Clear 重置，杜绝重复碰撞入口再次扣血。
    double frozenBombFrames_=0; // 时停中的 Bomb 区域接触仍按 60 Hz 结算，效果本身的帧号不前进。
    std::array<bool,4> playerBombSoundPaused_{}; // 与四种音效名字对应，记录本次暂停实际停止的缓冲。
    DropSystem drops_{};
    double battleTicks_=0; // 未消费的 120 Hz 战斗步；暂停不累积，重开清零。
    PlayerInput pendingPlayerInput_{}; // 持续键采用最新快照，鼠标增量累加，Bomb 边沿保留至下个战斗步。
    bool pendingDialogueConfirm_=false;
    StageDirector stage_{};
    double scenerySeconds_=0; // 背景世界秒数；时停时冻结，独立于仍前进的关卡事件时钟。
    std::unique_ptr<StageContent> stageContent_{};
    StageId stageId_ = StageId::Stage1;
    KeyBindings bindings_{};
    AssetManager assets_;

    long long score_ = 0;
    // 加载完成后按本局难度/内容初始化，连续切关保留；已审阅事件和管理回调不提供动态调整规则。
    // 其他原生间接写入仍待核验，不由此字段或初始化表推导全局恒定性及上下界。
    int referenceRank_ = 0;
    int graze_ = 0; // 本局累计擦弹，跨关保留，饱和于999999；饱和不停止擦弹加分。
    int pointItems_ = 0; // 本局普通蓝色道具累计数量，供原作点数栏显示。
    int stageGraze_=0,stagePointItems_=0; // 本关奖励计数，每关归零；stageGraze_饱和于99999，不能用累计值重复计算奖励。
    float messageTimer_ = 0.0F;
    float screenFlashTimer_ = 0.0F;
    float spellCutInTimer_ = 0.0F;
    float spellBonusTimer_ = 0; // 秒；暂停时冻结，捕获反馈最多显示 280 原作帧。
    long long spellBonus_ = 0, stageBonus_ = 0;
    StageClearSummary stageClearSummary_{}; // 关卡结束时固定的奖励明细，整个展示期间不变。
    float stageClearSeconds_ = 0;
    bool stageClearShown_ = false; // 每关奖励至多固定一次；可早于结算页，在对白保护等待期间仍保持Playing。
    /// @brief 借用当前战前/战后台词；Stage 内容存储须覆盖整个展示期。
    [[nodiscard]] std::span<const DialogueLine> CurrentDialogue() const;
    /// @brief 当前句索引及起始秒数；每句以自身 duration 计时，确认不越过原生退场或结算保护。
    [[nodiscard]] int DialogueIndex(float* lineStart = nullptr) const;
    [[nodiscard]] double DialogueDuration() const;
    /// @brief 顺序交付当前对白已到期的音乐指令；跳句也消费跨过的每条指令，同帧停止/重播不可合并。
    /// 游标随 BeginStageDialogue 重置，暂停不调用；cues 借用当前内容且必须按实际内容帧排序。
    void ApplyDialogueMusic(std::span<const DialogueCue> cues);
    bool dialogueAfterBattle_ = false;
    DialogueScene dialogueScene_=DialogueScene::FinalBoss;
    bool dialogueBossMusic_ = false;
    std::size_t dialogueMusicCursor_=0; // 当前对白已消费的演出表前缀；保证同曲后继与停止只执行一次。
    double dialogueTimer_ = 0.0; // 整段对话的游戏秒数；双精度累积，避免长对白在高 FPS 下偏离原生指令帧。
    bool dialogueActive_ = false;
    int dialogueSignalsSeen_=0; // 当前整段对白累计放行序号；跳句只前进，BeginStageDialogue 时归零。
    DialogueFlow dialogueFlow_{}; // 开始对白时固定的分支及加分边界；跳句不重新求值成绩门槛。
    int stageSpellCaptures_=0; // 本关独立取得进度，按内容规则更新；新局、重开和进入下一关均清零。
    std::optional<std::pair<StageEventId,int>> dialogueSignalWait_; // 时间轴按序只登记一个未完成等待，不拥有对白。
    ui::DialogueMotion dialogueMotion_{}; // 只在 Playing 的对话期间推进；暂停保持当前透明度、缩放与位移。
    std::optional<StageEventId> bossEventId_;
    std::optional<StageEventId> dialogueEventId_;
    SpriteRenderer spriteRenderer_{};
    mutable CombatRenderer combatRenderer_{};
    ProjectileCollision projectileCollision_{};
    // 道具与自机太极圈共用公告板缓冲；星点可超过普通道具池容量，不能改成固定长度数组。
    mutable std::vector<PickupSprite> pickupSprites_;
    mutable std::vector<BombVolumeSprite> playerBombVolumes_; // 本帧借用攻击几何快照，下一帧重建。
    std::string message_;
    int selectedMenuItem_ = 0; // 首页选项在本次运行中保留；首次启动默认 Start，返回首页不重置。
    ui::MenuMotion menuMotion_{}; // 当前页面的局部选择强度和进场时钟；Draw 只读取。
    ui::MenuMotion titleMenuMotion_{}; // 首页离开前的选择与展开状态，供首页各子界面过渡时继续绘制首页。
    // 编号仅属于首页展示，不是RunState；四机体各有一层，难度/设置子页换页也保持旧层退场。
    enum class TitlePage { Replay, ScoreDifficulty, ScoreFirst, ScoreSecond, ScoreThird, ScoreFourth, Music, Credit, Options, Bindings, Count };
    static constexpr int kTitlePages=static_cast<int>(TitlePage::Count);
    ui::PageMotion<kTitlePages> titlePageMotion_{};
    std::array<ui::MenuMotion,kTitlePages> titlePageLayers_{}; // 各页独立的选择与进场时钟，退出后不被首页选择覆盖。
    ui::StartSetupMotion startSetupMotion_{}; // 首页及 Start 页面连续混合；只推进展示，不阻塞选择输入。
    std::array<ui::MenuMotion,static_cast<int>(StartSetupStep::Count)> setupChoiceMotion_{}; // 各步骤独立保留行强调，快速确认/返回不把正在变化的亮度重置。
    ui::TitleEntrance titleEntrance_{}; // 首次首页的五段顺序展示，完成前屏蔽首页输入。
    OptionsPage optionsPage_ = OptionsPage::General;
    int selectedOption_ = 0;
    int selectedBinding_ = 0;
    int fpsLimitIndex_ = 1;
    int musicVolume_ = 50;
    int mouseSensitivity_ = 100;
    bool aimAssistEnabled_ = true;
    int soundVolume_ = 60;
    int initialLives_ = 2;
    int initialBombs_ = 3;
    int bestScore_ = 0;
    StageHighScores stageHighScores_{}; // 标准/挑战分开记录整关通关及失败分数，符卡练习和预览不写入。
    score::Profile scoreProfile_{};
    score::Entry pendingScore_{}; // 结局结束时固定，命名/选存期间不得再次加分或改变统计。
    int scoreShot_=0,scoreMode_=0,scoreDifficulty_=1,selectedScoreSave_=0;
    enum class ScoreViewStep { Difficulty, Records };
    ScoreViewStep scoreViewStep_=ScoreViewStep::Difficulty; // 仅控制查询层级；本局成绩保存后直接进入Records。
    native_combat::Difficulty runDifficulty_=native_combat::Difficulty::Normal; // 本局身份，独立于自适应 rank。
    int runInitialLives_=2; // 固定本局开局额外残机，跨关保留，供结算倍率使用。
    int scoreHighlight_=-1; // 最近一次成功保存的排名，浏览其他分组后清除。
    std::string scoreName_,scoreSaveError_;
    ui::NameEntryState scoreNameEditor_{};
    bool scorePageFromRun_=false;
    StartSelection lastSelection_{}; // 仅由开局页选择更新，不随本局 challengeMode_ 等运行状态重置。
    StartSetupStep setupStep_ = StartSetupStep::Difficulty; // 只接受目录中已开放的难度和入口。
    int selectedDifficulty_=1; // UI 0～3 索引；开始战斗时转换为独立运行身份。
    int selectedCharacter_ = 0; // 实际角色候选，0 灵梦/1 魔理沙；普通开局和符卡练习均沿用该编号。
    ui::MenuMotion startCharacterMotion_{}; // 角色切换时前景、背景及人物摘要共用的渐变权重。
    int selectedPracticeStage_ = 0; // kPracticeStages 的局部索引，始终指向已注册关卡。
    int selectedShot_ = 0; // 始终为角色内御札 0/1；成绩四组合编号由实际 Player::Shot() 给出。
    bool challengeMode_ = false;
    bool practiceMode_ = false;
    bool previewMode_ = false; // 静默截图不写入本机通关成绩，StartRun 会恢复普通记录规则。
    spell::Selection spellSelection_{}; // 浏览身份与当前战斗御札独立，确认时拆分为角色及角色内御札。
    spell::Records spellRecords_{};
    ui::MenuMotion spellCharacterMotion_{}; // 四组合展示权重，切换人物与文字共用，不影响鼠标命中。
    ui::MenuMotion spellGroupMotion_{},spellDifficultyMotion_{}; // 同页两列分别保存强调，焦点递进不清空另列或重播页面动画。
    int spellAttemptNumber_=0,spellAttemptCharacter_=0;
    bool spellAttemptOpen_=false,spellAttemptPractice_=false,spellCaptured_=false; // 已打开记录只能结算一次；无Miss/Bomb的击破或生存符实际超时可取得。
    int practiceAttack_ = 2; // 选定符卡工厂内的1起算展开技能编号，由目录Card::phase给出。
    bool spellPractice_ = false;
    int selectedTrack_ = 0; // 音乐页局部编号为 0～2；返回使用固定返回键，不占曲目槽。
    int selectedPauseItem_ = 0; // 暂停项：继续、主菜单、重开。
    int selectedResultItem_ = 0; // 结算项：重开、主菜单；每次结算重置为重开。
    int missCount_ = 0;
    int bombCount_ = 0;
    // 每张符卡开始时保存计数；发生 Miss、Bomb 或超时则不能获得捕获奖励。
    int spellStartMisses_ = 0, spellStartBombs_ = 0;
    float shotSoundTimer_ = 0.0F; // 秒；射击声音最多每 0.10 秒触发一次。
    bool waitingForBinding_ = false;
    /// @brief 激活行内录入后须先释放确认键/鼠标按钮，避免把确认点击录入为绑定。
    bool bindingCaptureReady_ = false;
    bool shouldQuit_ = false;
#ifdef KOUMAKYOU_DEBUG_FEATURES
    bool shotTestActive_ = false;
    bool debugRender_ = false;
    /// @brief Debug 构建默认启用，F4 可切换；重新开始时保留当前值。
    bool debugInvincible_ = false;
#endif
};

}  // namespace koumakyou
