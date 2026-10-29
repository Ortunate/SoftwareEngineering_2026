// 本文件负责声明游戏总控类、运行状态及各系统的协作入口。

#pragma once

#include "resources/asset_manager.hpp"
#include "gameplay/actors/boss.hpp"
#include "gameplay/bullets/system.hpp"
#include "gameplay/actors/enemy.hpp"
#include "app/run_state.hpp"
#include "app/start_setup.hpp"
#include "app/stage_clear.hpp"
#include "app/spell_practice.hpp"
#include "config/settings.hpp"
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
#include "render/start_setup_motion.hpp"
#include "render/dialogue_motion.hpp"
#include "render/title_entrance.hpp"

#include "raylib.h"

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
    /// @brief 从第一面开头开始一局战斗，先进入可持续更新的加载页。
    /// @param captureCursor 是否为交互游玩捕获鼠标；截图时传 false。
    void Start(bool captureCursor = true);
    /// @brief 选择关卡并进入加载页；重试沿用该关卡。
    void StartStage(StageId id, bool captureCursor = true);
    /// @brief 是否处于加载页（含失败提示）；截图方须等待完成，并检查 HasLoadingFailed。
    [[nodiscard]] bool IsLoading() const { return state_ == RunState::Loading; }
    /// @brief 加载失败时返回 true；交互用户可返回首页，截图调用方应结束并报告失败。
    [[nodiscard]] bool HasLoadingFailed() const { return IsLoading() && loadingFailed_; }
    /// @brief 进入不捕获鼠标、长时间无敌的关卡预览模式。
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
    void Update(float deltaTime);
    /// @brief 根据当前状态绘制游戏画面。
    void Draw() const;
    /// @brief 查询主菜单是否要求退出程序。
    /// @return 应结束主循环时返回 true。
    [[nodiscard]] bool ShouldQuit() const { return shouldQuit_; }

private:
    // 独立离屏集成测试通过此探针检查生命周期，正式程序不暴露可修改的战斗状态。
    friend struct GameLifecycleProbe;
    /// @brief 同时可保有的最多敌人数。
    static constexpr std::size_t kEnemyCapacity = 128;
    /// @brief 设置界面的普通设置页与按键设置页。
    enum class OptionsPage {
        General,
        KeyBindings,
    };

    /// @brief 进入关卡加载页；素材完成后重置战斗。同步入口只供预览和测试使用。
    /// @param captureCursor 是否捕获鼠标用于交互游玩。
    void StartRun(bool captureCursor = true, bool synchronous = false);
    /// @brief 非阻塞推进加载；失败后留在加载页，按确认或取消返回首页。
    void UpdateLoading(float deltaTime);
    /// @brief 素材就绪后初始化本局状态，避免加载耗时进入战斗计时或提前捕获鼠标。
    void CompleteStartRun();
    /// @brief 切换主菜单并清除战斗临时状态，保留离开首页时的选项。
    void EnterMainMenu();
    /// @brief 清理本局资源并返回整关练习的 Stage 页，保留御札、模式和关卡；其他模式回首页。
    void ReturnToPracticeStage();
    /// @brief 战后对话结束后固定奖励快照、计分并显示明细；每局只生效一次。
    void BeginStageClear();
    /// @brief 关闭奖励明细并进入资源收尾；整关练习随后返回关卡选择，普通开局显示通关菜单。
    void ContinueStageClear();
    /// @brief 把有效整关成绩写入对应模式的历史最高分；仅在关卡通关或失败结算时调用。
    void RecordStageScore();
    /// @brief 保存有效整关的分模式最高分并播放退场效果；池空后才显示通关/失败菜单并卸载专属素材。
    /// @param result 胜利或失败后的游戏状态。
    void EndStage(RunState result);
    /// @brief 在 Playing/StageClear/Finishing 与 Paused 间切换，并同步音乐和鼠标状态。
    void TogglePause();
    /// @brief 立即取消全部小怪并生成清场快照，不给予击破奖励。
    void ClearEnemiesWithEffects();
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
    /// @brief 处理符卡练习关卡页及同页符卡/难度的两级焦点，鼠标可直接跨栏；返回优先于本帧确认。
    void UpdateSpellPractice();
    /// @brief 清理战场并返回符卡组选择，保留角色、关卡和难度，不重置选择页动画。
    void ReturnToSpellPractice();
    /// @brief 宣告已实现符卡时建立并保存本篇/练习尝试；本篇遇见记录只解锁本次角色/御札组合。
    /// 每阶段最多打开一条记录，预览、对话与调试靶场不写入。
    void BeginSpellRecord();
    /// @brief 结束当前尝试并存档，重复调用无副作用；capture 须排除受击、Bomb 和超时。
    void FinishSpellRecord(bool capture);
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
    /// @brief 采集玩家输入，委托 Player 更新，并处理 Bomb 对战场的影响。
    /// @param deltaTime 本帧经过的秒数。
    void UpdatePlayer(float deltaTime);
    /// @brief 推进全部活动敌人的航线及技能计时器。
    /// @param deltaTime 本帧经过的秒数。
    void UpdateEnemies(float deltaTime);
    /// @brief 推进 Boss 当前阶段，超时则请求阶段切换。
    /// @param deltaTime 本帧经过的秒数。
    void UpdateBoss(float deltaTime);
    /// @brief 推进普通道具与星点，将拾取结果计入玩家 Power 与分数，并处理升满奖励。
    void UpdatePickups(float deltaTime);
    /// @brief 增减玩家 Power，首次升满时显示提示并将当前敌弹转成自动吸取的星点。
    /// @param delta Power 点数，可为负；下降后再次升满可重新触发。
    void AdjustPlayerPower(int delta);
    /// @brief 合并敌弹与敌人/Boss 本体对玩家的同帧命中，再处理擦弹及敌方受伤；对话不判定本体接触。
    void ResolveCollisions();
    /// @brief 将关卡事件转换为出怪、对话或 Boss 战操作。
    /// @param event 已到触发时刻的关卡事件。
    /// @return 同步完成的事件返回 true；Boss 战和对话返回 false。
    bool HandleStageEvent(const StageEvent& event);

    /// @brief 委托当前 Stage 工厂构造敌人并加入敌人池。
    /// @param spec 携带具体敌人类型、出生位置和显式轨迹的配置。
    void SpawnStageEnemy(const EnemySpec& spec) override;
    /// @brief 开始 Boss 战并停止旧批次后续出怪，保留现存小怪、弹幕、道具及效果。
    void BeginStageBoss(bool final, StageEventId id) override;
    /// @brief 开始原文对话；战后独白结束才固定 Bonus 并进入奖励明细，关闭后进入胜利收尾。
    void BeginStageDialogue(StageEventId id, bool afterBattle = false) override;
    void StageMessage(std::string_view message, float duration) override;
    /// @brief 消耗 Bomb 后启动独立攻击和全道具吸取，仅按初始保护实体清弹，不立即全场扣血。
    void ApplyBombEffect();
    /// @brief 在敌人和敌弹推进后、玩家碰撞前逐 60 Hz 帧推进 Bomb，所有命中均由真实空间实体决定。
    void UpdatePlayerBomb(float deltaTime);
    /// @brief 结算当前 Bomb 逻辑帧；只将真实接触交给伤害提供方，并反馈光球原始累计，同一帧重复调用无效。
    void ResolvePlayerBombFrame();
    /// @brief 玩家命中后更新场景反馈，并按剩余残机处理游戏失败。
    void HandlePlayerHit();
    /// @brief 结算当前 Boss 阶段并进入下一阶段或一面结算。
    void AdvanceBossPhase();
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
    /// @brief 绘制启动选择、音乐、成绩或制作信息页，保持通用素材生命周期。
    void DrawMenuPage() const;
    /// @brief 绘制开局的难度、人物、选择及确认层；只借用通用纹理，不提前加载关卡资源。
    void DrawStartSetup() const;
    /// @brief 绘制 FPS、音量、灵敏度、开局资源和按键设置入口。
    void DrawOptions() const;
    /// @brief 绘制可重新绑定的战斗按键列表。
    void DrawKeyBindings() const;
#ifdef KOUMAKYOU_DEBUG_FEATURES
    /// @brief 进入禁用关卡时间轴、只有固定靶怪的射击调试场地。
    void StartShotTest();
#endif

    Camera3D camera_{};
    RunState state_ = RunState::MainMenu;
    float loadingSeconds_ = 0; // 秒；独立 UI 时钟，不能参与关卡/弹幕/符卡计时。
    bool loadingStarted_ = false;
    bool loadingFailed_ = false;
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
    EnemyPool<kEnemyCapacity> enemies_{};
    ActorEffectSystem actorEffects_{};
    PlayerBombEffect playerBomb_{}; // 仅借用关卡 ANM；退出、重开、结算及卸载资源前清空。
    bool playerBombStartedThisFrame_=false;
    int playerBombResolvedFrame_=-1; // 只属于当前 Bomb，Start/Clear 重置，杜绝重复碰撞入口再次扣血。
    std::array<bool,4> playerBombSoundPaused_{}; // 与四种音效名字对应，记录本次暂停实际停止的缓冲。
    DropSystem drops_{};
    StageDirector stage_{};
    std::unique_ptr<StageContent> stageContent_{};
    StageId stageId_ = StageId::Stage1;
    KeyBindings bindings_{};
    AssetManager assets_;

    long long score_ = 0;
    /// @brief 按原作每 100 subrank 提升一个 rank；Normal 限制 10…32，初始 16。
    void AdjustReferenceRank(int subrank);
    int referenceRank_ = 16, referenceSubrank_ = 0;
    float rankTime_ = 0;
    int graze_ = 0;
    int pointItems_ = 0; // 本局普通蓝色道具累计数量，供原作点数栏显示。
    float messageTimer_ = 0.0F;
    float screenFlashTimer_ = 0.0F;
    float spellCutInTimer_ = 0.0F;
    float spellBonusTimer_ = 0; // 秒；暂停时冻结，捕获反馈最多显示 280 原作帧。
    long long spellBonus_ = 0, stageBonus_ = 0;
    StageClearSummary stageClearSummary_{}; // 关卡结束时固定的 Normal 奖励明细，整个展示期间不变。
    float stageClearSeconds_ = 0;
    bool stageClearShown_ = false; // 每局至多计算并展示一次；重开时重置，防止事件重入重复加分。
    /// @brief 借用当前战前/战后台词；Stage 内容存储须覆盖整个展示期。
    [[nodiscard]] std::span<const DialogueLine> CurrentDialogue() const;
    /// @brief 当前句索引及起始秒数；每句以自身 duration 计时，确认不越过原生退场或结算保护。
    [[nodiscard]] int DialogueIndex(float* lineStart = nullptr) const;
    [[nodiscard]] float DialogueDuration() const;
    bool dialogueAfterBattle_ = false;
    bool dialogueBossMusic_ = false;
    double dialogueTimer_ = 0.0; // 整段对话的游戏秒数；双精度累积，避免长对白在高 FPS 下偏离原生指令帧。
    bool dialogueActive_ = false;
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
    ui::MenuMotion titleMenuMotion_{}; // 首页离开前的选择与展开状态，供 Start 过渡期间继续绘制首页。
    ui::StartSetupMotion startSetupMotion_{}; // 首页及 Start 页面连续混合；只推进展示，不阻塞选择输入。
    std::array<ui::MenuMotion,static_cast<int>(StartSetupStep::Count)> setupChoiceMotion_{}; // 各步骤独立保留行强调，快速确认/返回不把正在变化的亮度重置。
    ui::TitleEntrance titleEntrance_{}; // 首次首页的五段顺序展示，完成前屏蔽首页输入。
    OptionsPage optionsPage_ = OptionsPage::General;
    int selectedOption_ = 0;
    int selectedBinding_ = 0;
    int fpsLimitIndex_ = 1;
    int musicVolume_ = 50;
    int mouseSensitivity_ = 100;
    int soundVolume_ = 60;
    int initialLives_ = 2;
    int initialBombs_ = 3;
    int bestScore_ = 0;
    StageHighScores stageHighScores_{}; // 标准/挑战分开记录整关通关及失败分数，符卡练习和预览不写入。
    StartSelection lastSelection_{}; // 仅由开局页选择更新，不随本局 challengeMode_ 等运行状态重置。
    StartSetupStep setupStep_ = StartSetupStep::Difficulty; // 当前支持两位角色的四种御札、Normal 与第一面。
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
    bool spellAttemptOpen_=false,spellAttemptPractice_=false,spellCaptured_=false; // 已打开记录只能结算一次；仅成功无伤无 Bomb 击破为取得。
    int practicePhase_ = 2; // 符卡练习只允许露米娅第 2 或第 4 阶段。
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
