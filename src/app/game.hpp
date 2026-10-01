// 本文件负责声明游戏总控类、运行状态及各系统的协作入口。

#pragma once

#include "resources/asset_manager.hpp"
#include "gameplay/actors/boss.hpp"
#include "gameplay/bullets/system.hpp"
#include "gameplay/actors/enemy.hpp"
#include "app/run_state.hpp"
#include "config/key_bindings.hpp"
#include "gameplay/actors/player.hpp"
#include "gameplay/items/drop_system.hpp"
#include "gameplay/arena.hpp"
#include "stage/system.hpp"
#include "stage/content.hpp"
#include "stage/catalog.hpp"
#include "render/sprite_renderer.hpp"

#include "raylib.h"

#include <cstddef>
#include <optional>
#include <memory>
#include <string>

namespace redmist {

/// @brief 游戏总控，协调输入、关卡、战斗、碰撞、资源和界面。
class Game : private StageEventSink {
public:
    /// @brief 加载通用素材与设置，并进入主菜单。
    Game();
    /// @brief 释放当前关卡及通用素材。
    ~Game();

    /// @brief 重置游戏并返回主菜单。
    void Reset();
    /// @brief 从第一面开头开始一局战斗。
    /// @param captureCursor 是否为交互游玩捕获鼠标；截图时传 false。
    void Start(bool captureCursor = true);
    /// @brief 选择并启动已注册的关卡；重试沿用该关卡。
    void StartStage(StageId id, bool captureCursor = true);
    /// @brief 进入不捕获鼠标、长时间无敌的关卡预览模式。
    void StartPreview();
    /// @brief 设置关卡预览的相机观察角度。
    /// @param yawDegrees 水平角（度）。
    /// @param pitchDegrees 仰俯角（度）。
    void SetPreviewView(float yawDegrees, float pitchDegrees);
#ifdef RED_MIST_DEBUG_FEATURES
    /// @brief 设置是否绘制判定球与发弹点。
    /// @param enabled 为 true 时启用调试图层。
    void SetDebugDraw(bool enabled) { debugRender_ = enabled; }
#endif
    /// @brief 进入设置菜单并显示鼠标。
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
    /// @brief 同时可保有的最多敌人数。
    static constexpr std::size_t kEnemyCapacity = 128;
    /// @brief 设置界面的普通设置页与按键设置页。
    enum class OptionsPage {
        General,
        KeyBindings,
    };

    /// @brief 重置玩家、敌人、弹幕、Boss 与关卡时钟。
    /// @param captureCursor 是否捕获鼠标用于交互游玩。
    void StartRun(bool captureCursor = true);
    /// @brief 切换主菜单并清除战斗中的临时状态。
    void EnterMainMenu();
    /// @brief 结算关卡并卸载专属素材。
    /// @param result 胜利或失败后的游戏状态。
    void EndStage(RunState result);
    /// @brief 从本地文件读取全部设置并应用音量、灵敏度。
    void LoadSettings();
    /// @brief 将当前 FPS、音量、灵敏度和绑定写入本地文件。
    void SaveSettings() const;
    /// @brief 处理主菜单的移动、点击与确认输入。
    void UpdateMainMenu();
    /// @brief 处理 FPS、音量、灵敏度与设置菜单页面的输入。
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
    /// @brief 推进两类掉落物并将拾取结果计入玩家 Power 与分数。
    void UpdatePickups(float deltaTime);
    /// @brief 处理玩家命中/擦弹及敌人、Boss 的受击判定。
    void ResolveCollisions();
    /// @brief 将关卡事件转换为出怪、对话或 Boss 战操作。
    /// @param event 已到触发时刻的关卡事件。
    /// @return 同步完成的事件返回 true；Boss 战和对话返回 false。
    bool HandleStageEvent(const StageEvent& event);

    /// @brief 委托当前 Stage 工厂构造敌人并加入敌人池。
    /// @param archetype Stage 局部敌人原型。
    /// @param spawn 出生位置、延迟与编队上下文。
    void SpawnStageEnemy(std::uint16_t archetype,
                         const EnemySpawn& spawn) override;
    void BeginStageBoss(bool final, StageEventId id) override;
    void BeginStageDialogue(StageEventId id) override;
    void StageMessage(std::string_view message, float duration) override;
    /// @brief 玩家已消耗 Bomb 后清弹、伤害敌人并展示反馈。
    void ApplyBombEffect();
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
    /// @return 用于设置界面的键名。
    [[nodiscard]] static std::string KeyLabel(int key);

    /// @brief 绘制天空盒、战场与空间中的敌人和弹幕。
    void DrawWorld() const;
    /// @brief 绘制战场立方体边框、网格和坐标轴。
    void DrawArena() const;
    /// @brief 绘制分数、残机、Bomb 与坐标等战斗信息。
    void DrawHud() const;
    /// @brief 绘制状态消息、对话、符卡或结算画面的叠加层。
    void DrawOverlay() const;
    /// @brief 绘制符卡开始时的立绘与名称展示。
    void DrawSpellCutIn() const;
    /// @brief 根据对话计时器绘制当前说话人和台词。
    void DrawDialogue() const;
    /// @brief 绘制开始菜单。
    void DrawMainMenu() const;
    /// @brief 绘制 FPS、音量、灵敏度和按键设置入口。
    void DrawOptions() const;
    /// @brief 绘制可重新绑定的战斗按键列表。
    void DrawKeyBindings() const;
#ifdef RED_MIST_DEBUG_FEATURES
    /// @brief 进入禁用关卡时间轴、只有固定靶怪的射击调试场地。
    void StartShotTest();
#endif

    Camera3D camera_{};
    RunState state_ = RunState::MainMenu;
    Player player_{};
    std::unique_ptr<Boss> boss_{};
    BulletSystem bullets_{};
    EnemyPool<kEnemyCapacity> enemies_{};
    DropSystem drops_{};
    StageDirector stage_{};
    std::unique_ptr<StageContent> stageContent_{};
    StageId stageId_ = StageId::Stage1;
    KeyBindings bindings_{};
    AssetManager assets_;

    long long score_ = 0;
    int graze_ = 0;
    float messageTimer_ = 0.0F;
    float screenFlashTimer_ = 0.0F;
    float spellCutInTimer_ = 0.0F;
    float dialogueTimer_ = 0.0F;
    bool dialogueActive_ = false;
    std::optional<StageEventId> bossEventId_;
    std::optional<StageEventId> dialogueEventId_;
    SpriteRenderer spriteRenderer_{};
    std::string message_;
    int selectedMenuItem_ = 0;
    OptionsPage optionsPage_ = OptionsPage::General;
    int selectedOption_ = 0;
    int selectedBinding_ = 0;
    int fpsLimitIndex_ = 1;
    int musicVolume_ = 50;
    int mouseSensitivity_ = 100;
    bool waitingForBinding_ = false;
    /// @brief 激活录入后须先释放确认键/鼠标按钮，避免把打开弹窗的点击录入为绑定。
    bool bindingCaptureReady_ = false;
    bool shouldQuit_ = false;
#ifdef RED_MIST_DEBUG_FEATURES
    bool shotTestActive_ = false;
    bool debugRender_ = false;
    /// @brief Debug 构建默认启用，F4 可切换；重新开始时保留当前值。
    bool debugInvincible_ = false;
#endif
};

}  // namespace redmist
