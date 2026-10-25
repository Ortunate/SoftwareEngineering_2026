// 本文件定义可复用的扇形/环形速度层弹幕、延迟转向及固定瞄准光束技能。
#pragma once
#include "gameplay/actors/movement.hpp"
#include "gameplay/bullets/system.hpp"
#include <array>
#include <optional>
namespace koumakyou::skill {
enum class Formation { AimedFan, FixedFan, AimedRing, FixedRing };
/// @brief 可组合的显现与运动选项；显现期结束后才开始计算加速/转向延迟。
enum ShotFlag { LaunchBoost=1, SpawnSmall=2, SpawnMedium=4, SpawnLarge=8, Accelerate=16, DelayTurn=64, DelayAim=128 };
/// @brief 一轮弹幕的参数；速度为世界单位/秒，角度从世界 +X 朝 -Z 为正、单位弧度，count/layers 至少为 1。
/// type 为 0…9，color 为 0…15；所有浮点参数须有限。firstSpeed/lastSpeed 沿速度层递减，
/// 末层采用开区间插值；flags 组合 ShotFlag，各显现位同时存在时优先 Large、Medium、Small。
struct Shot {
    Formation formation = Formation::AimedFan;
    int type=1, color=2, count=1, layers=1;
    float firstSpeed=2.67857143F, lastSpeed=0, angle=0, spacing=0;
    int flags=0;
};
/// @brief 出生局部帧上的一次技能，frame 非负；为空表示该敌人不发弹。
struct TimedShot { int frame=0; Shot shot; };
/// @brief 固定瞄准光束；length/width 为世界单位长度/全宽，其他时间均为 60 Hz 帧。
/// color 为 0…15，长度与宽度须为正，时间须有限且非负，sustain 须为正。
struct Laser {
    int color=6;
    float length=22.3214286F, width=0.714285714F, grow=120, sustain=60, fade=16, warning=120, harmlessTail=14;
};
/// @brief 延迟转向与加速的独立带参配置；时间为逻辑帧，速度/加速度为世界单位每秒/每秒平方。
/// 已发射的弹保存参数副本，之后更改配置只影响后续发射。
struct ProjectileMotion {
    float delayFrames=0, turnAngle=0, turnSpeed=0, acceleration=0;
};
/// @brief 默认 rank 修正；前两项为世界单位/秒，后四项为弹数与速度层数的整数端点。
inline constexpr std::array<float,6> kDefaultRankInfluence{-1.33928571F,1.33928571F,0,0,0,0};
/// @brief 一名演员持续保留的发弹上下文；offset 是世界 XYZ 偏移，rank 须在 10…32。
/// projectile 跨技能、阶段保留；清弹时只重置 rankInfluence。
class State {
public:
    Vector3 offset{};
    ProjectileMotion projectile;
    std::array<float,6> rankInfluence=kDefaultRankInfluence; // 弹速、弹数、速度层的 rank=0/32 两端修正。
    int rank=16;
    VisualId bulletBase=0, laserBase=0, fallback=0; // 每种弹连续 16 色，光束连续 16 色；编号属于调用关卡素材表。
    /// @brief 从世界位置加 offset 发射一轮，锁定本轮玩家高度坡度；池满时丢弃对应弹，不改变时序。
    void Fire(const Shot& shot,Vector3 position,Vector3 player,BulletSystem& bullets) const;
    /// @brief 生成时锁定瞄准方向，预警与消退期间沿用原光束碰撞时窗。
    void Fire(const Laser& laser,Vector3 position,Vector3 player,BulletSystem& bullets) const;
};
/// @brief 校验可配置单次技能的有限数值、颜色/弹种及正弹数/层数。
[[nodiscard]] bool Valid(const TimedShot& skill);
} // namespace koumakyou::skill
