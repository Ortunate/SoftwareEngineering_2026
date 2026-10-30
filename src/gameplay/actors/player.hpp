// 本文件负责定义玩家输入快照、碰撞结果及封装后的玩家行为接口。

#pragma once

#include "gameplay/bullets/system.hpp"
#include "gameplay/actors/aim.hpp"
#include "gameplay/data/native_combat.hpp"

#include <span>
#include <array>

namespace koumakyou {

/// @brief 单帧玩家输入快照；mouseDelta 为鼠标像素位移，按键值表示当前按下状态，bomb 表示本帧新按下。
struct PlayerInput {
    Vector2 mouseDelta{};
    /// @brief 鼠标位移到视角的灵敏度倍率，设置层保证 0.1～3.0；默认 1.0 沿用原速度。
    float mouseSensitivityScale = 1.0F;
    bool forward = false;
    bool backward = false;
    bool moveLeft = false;
    bool moveRight = false;
    bool ascend = false;
    bool descend = false;
    bool focus = false;
    bool fire = false;
    bool bomb = false;
    bool aimAssistEnabled = false; // 正式游戏由持久化设置传入，独立调用默认保留原行为。
};

/// @brief 玩家与一颗敌弹接触后的结果；擦弹只对同一颗弹幕报告一次。
enum class PlayerContact { None, Graze, Hit };

/// @brief 本关卡的机体素材编号；各副弹变体和命中外观按原始动画顺序排列，未配置项须为 invalid。
/// 只复制编号，不拥有纹理；副弹数组供追踪弹/针、四级导弹或三级激光使用。
struct PlayerShotVisuals {
    VisualId primary=kInvalidVisualId;
    std::array<VisualId,4> secondary{kInvalidVisualId,kInvalidVisualId,kInvalidVisualId,kInvalidVisualId};
    std::array<VisualId,4> impact{kInvalidVisualId,kInvalidVisualId,kInvalidVisualId,kInvalidVisualId};
};

/// @brief 持有玩家位置、视角、判定、资源及计时状态，并执行移动、射击和受击规则。
/// 世界坐标沿用 raylib：正 Z 为初始前方，正 Y 为上方；时间单位为秒，角度单位为弧度。
class Player {
public:
    /// @brief 本项目 Power 下限及开局值；受击和调试降级后仍保留双副机的最低火力档。
    static constexpr int kMinPower = 8;
    /// @brief 原作 Power 上限；普通 P 道具增加 1，大 P 道具增加 8。
    static constexpr int kMaxPower = 128;
    /// @brief 自机弹和副机共用的不透明度（0～255），与贴图及原生动画 Alpha 相乘。
    static constexpr unsigned char kShotAlpha = 51;
    /// @brief 两角色副机未缩放的世界高度；原始直径均为 36 逻辑像素，弹束偏移和外观清单共用该换算。
    static constexpr float kOptionWorldHeight = 0.42F;
    /// @brief 恢复开局状态，包括初始位置、残机、Bomb、Power 8 和全部计时器。
    void Reset();
    /// @brief Reset 后配置本局额外残机（1～5）、Bomb（0～3）、挑战模式与机体；数值越界会钳制。
    /// 挑战模式照常计 Miss、扣 Power 和复活，但不扣残机，不会因命中结束一局。
    void ConfigureRun(int extraLives, int bombs, bool challenge, ShotType shot);
    /// @brief 重置位置、视角、副弹周期及副机收拢/动画时间，用于返回菜单或复活，旧激光下次更新回收。
    void ResetPose();
    /// @brief 指定视角并将俯仰角限制在玩家可操作范围内。
    void SetView(float yawRadians, float pitchRadians);
    /// @brief 设置剩余无敌时间；预览模式可传入较长的秒数。
    void SetInvulnerability(float seconds);

    /// @brief 推进计时器、视角和移动，并生成玩家弹或消耗 Bomb；恋札 Bomb 期间三轴移速为正常的 0.3 倍。
    /// @param deltaTime 本帧经过的非负秒数。
    /// @param input 本帧采集的输入快照。
    /// @param bullets 接收玩家弹的对象池，容量耗尽时射击请求被丢弃。
    /// @param bulletVisual 当前关卡中央主符卡的外观 ID。
    /// @param secondaryVisual 当前机体副弹外观 ID，由关卡选择追踪符卡或直射针素材。
    /// @param allowFire 对话期间为 false；Bomb 不受此参数影响。
    /// @param aimTargets 本次调用期间借用的活动敌人/Boss 判定体；移动和转向后求准星射线交点及可选辅助落点。
    /// 射线与辅助均未选中目标时在前方 20 世界单位汇聚，已发出的直射弹不再随准星改变方向。
    /// 四机体主副弹按原作 60 Hz、30 帧射击窗口分批生成；松开射击后完成当前窗口，allowFire=false 则取消。
    /// @return 成功消耗 Bomb 时返回 true，调用方据此处理清弹和范围伤害。
    bool Update(float deltaTime, const PlayerInput& input, BulletSystem& bullets,
                VisualId bulletVisual, VisualId secondaryVisual, bool allowFire,
                std::span<const PlayerAimTarget> aimTargets = {});
    /// @brief 使用当前关卡的完整机体素材推进一帧；单位与上方重载相同，素材编号仅复制到生成的弹幕。
    /// 激光以各副机独立冷却控制，不重复叠加；Focus 不修改四机体的原始射击周期。
    bool Update(float deltaTime, const PlayerInput& input, BulletSystem& bullets,
                const PlayerShotVisuals& visuals, bool allowFire,
                std::span<const PlayerAimTarget> aimTargets = {});

    /// @brief 增减 Power 并钳制在 8 到 128；收集道具、受击和 Debug 按键共用此入口。
    /// @param delta 正数增加，负数降低，单位为 Power 点。
    /// @return 仅从未满升到满级时返回 true，调用方负责提示和清弹奖励。
    bool AdjustPower(int delta);

    /// @brief 处理活动敌弹/光束的命中与擦弹；预警无害，命中只清除普通弹，光束保持到结束。
    /// @param bullet 由弹幕池持有的敌弹，玩家只修改 active 或 grazed。
    /// @return 无接触、首次擦弹或可造成伤害的命中；无敌时命中返回 None。
    PlayerContact ContactEnemyBullet(Bullet& bullet) const;
    /// @brief 查询玩家命中球与敌方本体球是否接触；position/radius 为世界坐标/世界半径，半径须正且有限。
    /// 复活或 Bomb 无敌时、半径无效时返回 false；不修改敌人，不产生擦弹，受击结算由 Game 统一合并。
    [[nodiscard]] bool ContactEnemyBody(Vector3 position,float radius) const;
    /// @brief 普通模式扣除一条残机，挑战模式保留残机；均扣 16 Power（最低 8）、补足 Bomb、复位并给予无敌。
    /// @return 仍有可用残机时返回 true；用尽时返回 false。
    bool TakeHit();
    /// @brief 根据玩家位置和视角更新相机；相机位置始终等于玩家判定中心。
    void SyncCamera(Camera3D& camera) const;

    /// @brief 返回玩家判定中心的世界坐标。
    [[nodiscard]] Vector3 Position() const { return position_; }
    /// @brief 返回 HUD 展示的剩余残机数；包含当前正在使用的一条命。
    [[nodiscard]] int RemainingLives() const;
    /// @brief 返回当前可用 Bomb 数量。
    [[nodiscard]] int Bombs() const { return bombs_; }
    /// @brief 返回当前 Power 点数，范围为 8 到 128。
    [[nodiscard]] int Power() const { return power_; }
    /// @brief 返回本局实际机体；关卡据此绑定玩家弹和副机外观，不能使用菜单浏览状态代替。
    [[nodiscard]] ShotType Shot() const { return shot_; }
    /// @brief 是否使用魔理沙的移动速度、主弹、副机及持续射击规则。
    [[nodiscard]] bool IsMarisa() const { return shot_==ShotType::MagicMissile || shot_==ShotType::IllusionLaser; }
    /// @brief 是否显示两侧副机；沿用原作 Power 8 门槛，本项目四种御札从开局均显示。
    [[nodiscard]] bool OptionsVisible() const { return power_ >= 8; }
    /// @brief 返回左、右副机中心的世界坐标；追踪弹及激光从中心出生，其他弹束叠加原始模块偏移。
    /// 位置随视角和 Focus 收拢更新；近距离按当前瞄准深度缩短偏移，渲染与副弹模块必须共用此入口。
    [[nodiscard]] std::array<Vector3, 2> OptionPositions() const;
    /// @brief 副机的近距离尺寸倍率，范围 (0,1]；随发射位置等比缩小以保持屏幕尺寸，避免遮挡准星。
    [[nodiscard]] float OptionScale() const;
    /// @brief 本次姿态重置后经过的游戏秒数，供副机原生动画同步；暂停时不推进。
    [[nodiscard]] double VisualAge() const { return visualAge_; }

    /// @brief 借用本帧解析结果，仅用于 HUD，不拥有目标对象。
    [[nodiscard]] const PlayerAimResult& Aim() const { return aim_; }

private:
    PlayerAimResult aim_{};
    /// @brief 当前视线的单位前向向量。
    [[nodiscard]] Vector3 Forward() const;
    /// @brief 与当前水平朝向垂直的单位右向量。
    [[nodiscard]] Vector3 Right() const;
    /// @brief 与当前视线正交的屏幕上方单位向量，用于将发射点保持在准星下方。
    [[nodiscard]] Vector3 Up() const;
    /// @brief 从四机体统一原始火力表发射到期通道，使用原始点数；激光的 period/phase 表示槽冷却和槽号。
    void FireNativeFrame(BulletSystem& bullets, const PlayerShotVisuals& visuals,
                         std::span<const PlayerAimTarget> aimTargets);
    /// @brief 维护两个激光槽及其跟随位置和当前视线；禁火或复活取消旧激光，冷却使用游戏秒数。
    void UpdateLasers(bool allowFire, BulletSystem& bullets,
                      std::span<const PlayerAimTarget> aimTargets);
    /// @brief 在库存和冷却允许时消耗 Bomb，按当前御札设置完整演出的冷却与原生保护时间。
    [[nodiscard]] bool TryUseBomb();

    Vector3 position_{0.0F, 0.0F, -6.5F};
    float yaw_ = 0.0F;
    float pitch_ = 0.0F;
    float invulnerableTimer_ = 1.5F;
    float bombCooldown_ = 0.0F;
    // 四机体主副弹采用原作 60 Hz 计时；-1 表示空闲，0…29 为窗口帧，Focus/Power 不重置相位。
    // 小数帧跨 Update 保留，低帧率逐帧补发；窗口结束时仅持续按住射击才启动下一轮。
    int secondaryFrame_ = -1;
    double secondaryFractionalFrame_ = 0.0;
    // 槽号只属于本玩家；epoch 隔离复活前的旧光束，不保存可能被弹池复用的指针。
    std::array<double,2> laserReadyAt_{}; // 与 visualAge_ 同一游戏秒数基准；补发逻辑帧按其真实时刻检查冷却。
    std::uint32_t weaponEpoch_=1;
    // Focus 在 8/60 秒内收拢到三分之一侧距；进度跨帧保留，反向切换不跳变。
    float optionFocus_ = 0.0F;
    float optionAimDepth_ = 20.0F; // 最近一次 Update 的瞄准深度（世界单位），只保存值，不借用目标。
    double visualAge_ = 0.0;
    // lives_ 是额外残机数；未配置的默认值 3 供独立测试使用，Game 按设置覆盖；-1 时结束。
    int lives_ = 3;
    int bombs_ = 3;
    int power_ = kMinPower;
    bool challenge_ = false;
    ShotType shot_ = ShotType::HomingAmulet;
};

}  // namespace koumakyou
