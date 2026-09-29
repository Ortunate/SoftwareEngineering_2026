// 本文件负责定义 Boss 的抽象接口及血量、阶段等公共状态。

#pragma once

#include "gameplay/bullets/bullet_system.hpp"

namespace redmist {

/// @brief Boss 抽象基类，保存判定、血量与阶段公共状态。
class Boss {
public:
    /// @brief 通过基类指针安全销毁具体 Boss。
    virtual ~Boss() = default;
    /// @brief 推进当前阶段的移动与攻击。
    /// @param deltaTime 本帧经过的秒数。
    /// @param playerPosition 玩家判定中心。
    /// @param bullets 接收技能弹幕的系统。
    /// @return 当前阶段到达时间上限时返回 true。
    virtual bool Update(float deltaTime, Vector3 playerPosition,
                        BulletSystem& bullets) = 0;
    /// @brief 当前阶段结束后尝试进入下一阶段。
    /// @return 成功进入下一阶段返回 true；退场或战斗结束返回 false。
    virtual bool AdvancePhase() = 0;
    /// @brief 查询当前阶段需要展示的符卡名。
    /// @return 符卡名指针；非符卡阶段返回 nullptr。
    [[nodiscard]] virtual const char* SpellName() const = 0;
    /// @brief 当前遭遇是否为道中 Boss。
    [[nodiscard]] virtual bool IsMidboss() const = 0;
    /// @brief HUD 中展示的 Boss 名称。
    [[nodiscard]] virtual const char* DisplayName() const = 0;

    bool active = false;
    VisualId visual = kInvalidVisualId;
    Vector3 position{0.0F, 2.0F, 7.2F};
    float health = 0.0F;
    float maxHealth = 0.0F;
    float hitRadius = 0.95F;
    float phaseTimer = 0.0F;
    float fireTimer = 0.0F;
    float moveClock = 0.0F;
    float starRotation = 0.0F;
    int phase = 0;

    /// @brief 沿瞄准方向求 Boss 的发弹点。
    /// @param playerPosition 玩家判定中心。
    /// @return 世界坐标中的发弹点。
    [[nodiscard]] Vector3 MuzzlePosition(Vector3 playerPosition) const;
};

}  // namespace redmist
