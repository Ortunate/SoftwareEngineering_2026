// 本文件负责定义玩家输入快照、碰撞结果及封装后的玩家行为接口。

#pragma once

#include "gameplay/bullets/system.hpp"

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
};

/// @brief 玩家与一颗敌弹接触后的结果；擦弹只对同一颗弹幕报告一次。
enum class PlayerContact { None, Graze, Hit };
/// @brief 灵梦两种三维机体：追踪御札保留侧发追踪弹，直射针集中火力且不追踪。
enum class ShotType { HomingAmulet, Needle };

/// @brief 本帧可瞄准的球形判定体快照；position/radius 为世界坐标/世界单位，radius 须为正。
/// 仅用于确定准星射线的汇聚深度，不会将弹道吸附到偏离准星的目标中心。
struct PlayerAimTarget {
    Vector3 position{};
    float radius = 0.0F;
};

/// @brief 持有玩家位置、视角、判定、资源及计时状态，并执行移动、射击和受击规则。
/// 世界坐标沿用 raylib：正 Z 为初始前方，正 Y 为上方；时间单位为秒，角度单位为弧度。
class Player {
public:
    /// @brief 本项目 Power 下限及开局值；受击和调试降级后仍保留可显示太极圈的最低火力档。
    static constexpr int kMinPower = 8;
    /// @brief 原作 Power 上限；普通 P 道具增加 1，大 P 道具增加 8。
    static constexpr int kMaxPower = 128;
    /// @brief 自机符卡弹和太极圈共用的不透明度（0～255），与贴图及原生动画 Alpha 相乘。
    static constexpr unsigned char kShotAlpha = 51;
    /// @brief 恢复开局状态，包括初始位置、残机、Bomb、Power 8 和全部计时器。
    void Reset();
    /// @brief Reset 后配置本局额外残机（1～5）、Bomb（0～3）、挑战模式与机体；数值越界会钳制。
    /// 挑战模式照常计 Miss、扣 Power 和复活，但不扣残机，不会因命中结束一局。
    void ConfigureRun(int extraLives, int bombs, bool challenge, ShotType shot);
    /// @brief 重置位置、视角、追踪弹周期及太极圈收拢/动画时间，用于返回菜单或复活。
    void ResetPose();
    /// @brief 指定视角并将俯仰角限制在玩家可操作范围内。
    void SetView(float yawRadians, float pitchRadians);
    /// @brief 设置剩余无敌时间；预览模式可传入较长的秒数。
    void SetInvulnerability(float seconds);

    /// @brief 推进计时器、视角和移动，并根据输入生成玩家弹或消耗 Bomb。
    /// @param deltaTime 本帧经过的非负秒数。
    /// @param input 本帧采集的输入快照。
    /// @param bullets 接收玩家弹的对象池，容量耗尽时射击请求被丢弃。
    /// @param bulletVisual 当前关卡直射弹的外观 ID。
    /// @param homingVisual 当前关卡追踪弹的外观 ID。
    /// @param allowFire 对话期间为 false；Bomb 不受此参数影响。
    /// @param aimTargets 本次调用期间借用的活动敌人/Boss 判定体；移动和转向后求准星射线交点。
    /// 射线未指向目标时在前方 20 世界单位汇聚，已发出的直射弹不再随准星改变方向。
    /// 追踪弹按原作 60 Hz、30 帧射击窗口分批生成；松开射击后完成当前窗口，allowFire=false 则取消。
    /// @return 成功消耗 Bomb 时返回 true，调用方据此处理清弹和范围伤害。
    bool Update(float deltaTime, const PlayerInput& input, BulletSystem& bullets,
                VisualId bulletVisual, VisualId homingVisual, bool allowFire,
                std::span<const PlayerAimTarget> aimTargets = {});

    /// @brief 增减 Power 并钳制在 8 到 128；收集道具、受击和 Debug 按键共用此入口。
    /// @param delta 正数增加，负数降低，单位为 Power 点。
    /// @return 仅从未满升到满级时返回 true，调用方负责提示和清弹奖励。
    bool AdjustPower(int delta);

    /// @brief 处理活动敌弹/光束的命中与擦弹；预警无害，命中只清除普通弹，光束保持到结束。
    /// @param bullet 由弹幕池持有的敌弹，玩家只修改 active 或 grazed。
    /// @return 无接触、首次擦弹或可造成伤害的命中；无敌时命中返回 None。
    PlayerContact ContactEnemyBullet(Bullet& bullet) const;
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
    /// @brief 是否显示两侧太极圈；沿用原作的 Power 8 门槛，本项目最低为 8，两种御札从开局均显示。
    [[nodiscard]] bool OptionsVisible() const { return power_ >= 8; }
    /// @brief 返回左、右太极圈中心的世界坐标，也是本帧副弹的出生位置。
    /// 位置随视角和 Focus 收拢更新；近距离按当前瞄准深度缩短偏移，渲染与射击必须共用此入口。
    [[nodiscard]] std::array<Vector3, 2> OptionPositions() const;
    /// @brief 太极圈的近距离尺寸倍率，范围 (0,1]；随发射位置等比缩小以保持屏幕尺寸，避免遮挡准星。
    [[nodiscard]] float OptionScale() const;
    /// @brief 本次姿态重置后经过的游戏秒数，供太极圈原生动画同步；暂停时不推进。
    [[nodiscard]] double VisualAge() const { return visualAge_; }

private:
    /// @brief 当前视线的单位前向向量。
    [[nodiscard]] Vector3 Forward() const;
    /// @brief 与当前水平朝向垂直的单位右向量。
    [[nodiscard]] Vector3 Right() const;
    /// @brief 与当前视线正交的屏幕上方单位向量，用于将发射点保持在准星下方。
    [[nodiscard]] Vector3 Up() const;
    /// @brief gold 从前方偏下的单个模块对称散射，向准星目标平面的落点直飞。
    void Fire(BulletSystem& bullets, VisualId bulletVisual,
              std::span<const PlayerAimTarget> aimTargets);
    /// @brief 根据 homingFrame_ 与当前 Power 发射本帧到期的 cyan 通道；每个通道左右各一颗。
    /// 向准星目标平面的散射落点出射后继续追踪，近距离等比缩短模块偏移。
    void FireHomingFrame(BulletSystem& bullets, VisualId homingVisual,
                         std::span<const PlayerAimTarget> aimTargets);
    /// @brief 在库存和冷却允许时消耗 Bomb，并设置冷却与无敌时间。
    [[nodiscard]] bool TryUseBomb();

    Vector3 position_{0.0F, 0.0F, -6.5F};
    float yaw_ = 0.0F;
    float pitch_ = 0.0F;
    float fireTimer_ = 0.0F;
    float invulnerableTimer_ = 1.5F;
    float bombCooldown_ = 0.0F;
    // cyan 独立采用原作 60 Hz 计时；-1 表示空闲，0…29 为当前窗口帧，相位不能随 Focus/Power 重置。
    // 小数帧跨 Update 保留，低帧率逐帧补发；窗口结束时仅持续按住射击才启动下一轮。
    int homingFrame_ = -1;
    double homingFractionalFrame_ = 0.0;
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
