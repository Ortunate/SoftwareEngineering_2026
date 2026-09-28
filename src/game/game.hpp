#pragma once

#include "game/boss.hpp"
#include "game/bullet_system.hpp"
#include "game/enemy.hpp"
#include "game/game_types.hpp"
#include "game/stage_system.hpp"
#include "game/sprite_renderer.hpp"

#include "raylib.h"

#include <cstddef>
#include <string>

namespace redmist {

/// @brief 游戏总控，协调输入、关卡、战斗、碰撞、资源和界面。
class Game {
public:
    /// @brief 加载贴图、着色器与设置，并进入主菜单。
    Game();
    /// @brief 释放游戏持有的图形资源。
    ~Game();

    /// @brief 重置游戏并返回主菜单。
    void Reset();
    /// @brief 从第一面开头开始一局战斗。
    /// @param captureCursor 是否为交互游玩捕获鼠标；截图时传 false。
    void Start(bool captureCursor = true);
    /// @brief 进入不捕获鼠标、长时间无敌的关卡预览模式。
    void StartPreview();
    /// @brief 设置关卡预览的相机观察角度。
    /// @param yawDegrees 水平角（度）。
    /// @param pitchDegrees 仰俯角（度）。
    void SetPreviewView(float yawDegrees, float pitchDegrees);
    /// @brief 设置是否绘制判定球与发弹点。
    /// @param enabled 为 true 时启用调试图层。
    void SetDebugDraw(bool enabled) { debugRender_ = enabled; }
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
    /// @brief 从本地文件读取 FPS 上限与战斗键位。
    void LoadSettings();
    /// @brief 将当前 FPS 上限与键位写入本地文件。
    void SaveSettings() const;
    /// @brief 处理主菜单的移动、点击与确认输入。
    void UpdateMainMenu();
    /// @brief 处理 FPS 与设置菜单页面的输入。
    void UpdateOptions();
    /// @brief 处理按键绑定列表与新按键录入。
    void UpdateKeyBindings();
    /// @brief 在支持的 FPS 档位之间循环切换并保存。
    /// @param offset 档位变化量，通常为 -1 或 1。
    void AdjustFpsLimit(int offset);
    /// @brief 更新玩家视角、移动、射击与 Bomb 输入。
    /// @param deltaTime 本帧经过的秒数。
    void UpdatePlayer(float deltaTime);
    /// @brief 使摄像机位置与玩家判定中心保持一致。
    void UpdateCamera();
    /// @brief 推进全部活动敌人的航线及技能计时器。
    /// @param deltaTime 本帧经过的秒数。
    void UpdateEnemies(float deltaTime);
    /// @brief 推进 Boss 当前阶段，超时则请求阶段切换。
    /// @param deltaTime 本帧经过的秒数。
    void UpdateBoss(float deltaTime);
    /// @brief 处理玩家命中/擦弹及敌人、Boss 的受击判定。
    void ResolveCollisions();
    /// @brief 将关卡事件转换为出怪、对话或 Boss 战操作。
    /// @param event 已到触发时刻的关卡事件。
    void HandleStageEvent(const StageEvent& event);

    /// @brief 按类型构造具体敌人并加入敌人池。
    /// @param kind 敌人类型。
    /// @param path 移动航线。
    /// @param position 生成位置。
    /// @param delay 入场延迟（秒）。
    void SpawnEnemy(EnemyKind kind, PathKind path, Vector3 position,
                    float delay = 0.0F);
    /// @brief 从玩家视线前方发射一组玩家弹。
    void FirePlayerShot();
    /// @brief 消耗一个 Bomb，清弹并伤害场上敌人。
    void UseBomb();
    /// @brief 扣除残机、复位玩家并处理游戏失败。
    void DamagePlayer();
    /// @brief 清理当前场面并开启露米娅道中战。
    void StartMidboss();
    /// @brief 清理当前场面并开启露米娅关底战。
    void StartFinalBoss();
    /// @brief 结算当前 Boss 阶段并进入下一阶段或一面结算。
    void AdvanceBossPhase();
    /// @brief 设置屏幕提示文字及显示时间。
    /// @param message 要展示的文字。
    /// @param duration 持续秒数。
    void SetMessage(std::string message, float duration);

    /// @brief 将玩家 yaw/pitch 转换为视线方向。
    /// @return 单位前向向量。
    [[nodiscard]] Vector3 Forward() const;
    /// @brief 根据玩家 yaw 求水平右向量。
    /// @return 用于横移和双发射击的右向量。
    [[nodiscard]] Vector3 Right() const;
    /// @brief 按设置菜单索引读取对应按键。
    /// @param index 按键绑定列表中的索引。
    /// @return raylib 键码；索引无效时返回 KEY_NULL。
    [[nodiscard]] int BindingAt(int index) const;
    /// @brief 修改指定动作的键码并处理键位冲突。
    /// @param index 动作索引。
    /// @param key 新 raylib 键码。
    void SetBinding(int index, int key);
    /// @brief 将 raylib 键码转换为界面可读文字。
    /// @param key raylib 键码。
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
    /// @brief 绘制 FPS 和按键设置入口。
    void DrawOptions() const;
    /// @brief 绘制可重新绑定的战斗按键列表。
    void DrawKeyBindings() const;

    Camera3D camera_{};
    RunState state_ = RunState::MainMenu;
    Player player_{};
    RumiaBoss boss_{};
    BulletSystem bullets_{};
    EnemyPool<kEnemyCapacity> enemies_{};
    StageDirector stage_{};
    KeyBindings bindings_{};

    long long score_ = 0;
    int graze_ = 0;
    float messageTimer_ = 0.0F;
    float screenFlashTimer_ = 0.0F;
    float spellCutInTimer_ = 0.0F;
    float dialogueTimer_ = 0.0F;
    bool dialogueActive_ = false;
    Texture2D spellPortrait_{};
    Texture2D reimuPortrait_{};
    Model skyboxModel_{};
    TextureCubemap skyboxTexture_{};
    Shader skyboxShader_{};
    Texture2D fairyTexture_{};
    Texture2D eliteTexture_{};
    Texture2D bulletTexture_{};
    Texture2D redBulletTexture_{};
    SpriteRenderer spriteRenderer_{};
    std::string message_;
    int selectedMenuItem_ = 0;
    OptionsPage optionsPage_ = OptionsPage::General;
    int selectedOption_ = 0;
    int selectedBinding_ = 0;
    int fpsLimitIndex_ = 1;
    bool waitingForBinding_ = false;
    bool shouldQuit_ = false;
    bool debugRender_ = false;
};

}  // namespace redmist
