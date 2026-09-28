#pragma once

#include "game/bullet_system.hpp"

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

    bool active = false;
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

/// @brief 实现露米娅的道中战及关底四阶段战斗。
class RumiaBoss final : public Boss {
public:
    /// @brief 区分道中遭遇与关底遭遇。
    enum class Encounter { Midboss, Final };

    /// @brief 重置为道中 Boss 的初始状态。
    void StartMidboss();
    /// @brief 重置为关底 Boss 的初始状态。
    void StartFinalBoss();
    /// @brief 按遭遇类型和阶段更新移动、技能及超时判定。
    /// @param deltaTime 本帧经过的秒数。
    /// @param playerPosition 玩家判定中心。
    /// @param bullets 接收生成弹幕的系统。
    /// @return 当前阶段超时返回 true。
    bool Update(float deltaTime, Vector3 playerPosition,
                BulletSystem& bullets) override;
    /// @brief 结束当前阶段并重置下一阶段的血量与计时器。
    /// @return 仍有下一阶段返回 true，否则返回 false。
    bool AdvancePhase() override;
    /// @brief 获取当前符卡展示名。
    /// @return 符卡阶段返回名称，其他阶段返回 nullptr。
    [[nodiscard]] const char* SpellName() const override;
    /// @brief 获取当前是道中战还是关底战。
    /// @return 当前遭遇类型。
    [[nodiscard]] Encounter CurrentEncounter() const { return encounter_; }

private:
    Encounter encounter_ = Encounter::Midboss;
    /// @brief 道中阶段的瞄准散射弹。
    /// @param bullets 接收弹幕的系统。
    /// @param playerPosition 玩家判定中心。
    void FireAimedVolley(BulletSystem& bullets, Vector3 playerPosition);
    /// @brief 关底第一阶段的连续瞄准弹。
    /// @param bullets 接收弹幕的系统。
    /// @param playerPosition 玩家判定中心。
    void FireAimedStream(BulletSystem& bullets, Vector3 playerPosition);
    /// @brief 第一张符卡的双层旋转锥形弹。
    /// @param bullets 接收弹幕的系统。
    /// @param playerPosition 玩家判定中心。
    void CastNightBird(BulletSystem& bullets, Vector3 playerPosition);
    /// @brief 关底第三阶段的直线弹列与扩散弹。
    /// @param bullets 接收弹幕的系统。
    /// @param playerPosition 玩家判定中心。
    void FireMoonlightBeam(BulletSystem& bullets, Vector3 playerPosition);
    /// @brief 第二张符卡的多层环状散射弹。
    /// @param bullets 接收弹幕的系统。
    /// @param playerPosition 玩家判定中心。
    void CastDemarcation(BulletSystem& bullets, Vector3 playerPosition);
};

}  // namespace redmist
