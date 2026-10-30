// 本文件定义可复用的扇形/环形速度层弹幕、延迟转向及固定瞄准光束技能。
#pragma once
#include "gameplay/actors/movement.hpp"
#include "gameplay/bullets/system.hpp"
#include <array>
#include <optional>
#include <functional>
namespace koumakyou::skill {
/// @brief 环形偏移款在每层增加半个方向间隔；随机款的 angle/spacing 分别为角度区间两端。
/// RandomSpeedRing 保留规则环，随机速率；RandomAngleSpeed 对每发依次读取角度、速率两个随机数。
enum class Formation { AimedFan, FixedFan, AimedRing, FixedRing, AimedOffsetRing, FixedOffsetRing,
                       RandomAngle, RandomSpeedRing, RandomAngleSpeed };
/// @brief 可组合的显现与运动选项；显现末帧的交接按ProjectileMotion::sampleAtStepStart约定计算运动时钟。
/// PlaySound 由 Actor 按已配置的音效身份生成事件，纯 State 发射不访问音频资源。
enum ShotFlag { LaunchBoost=1, SpawnSmall=2, SpawnMedium=4, SpawnLarge=8, Accelerate=16, Velocity=32, DelayTurn=64, DelayAim=128, AbsoluteTurn=256, PlaySound=512, BounceAll=1024, BounceExceptLowerZ=2048 };
/// @brief 一轮弹幕的参数；速度为世界单位/秒，角度从世界 +X 朝 -Z 为正、单位弧度，count/layers 至少为 1。
/// type 为 0…9，color 为 0…15；所有浮点参数须有限。firstSpeed/lastSpeed 沿速度层递减，
/// 末层采用开区间插值；flags 组合 ShotFlag，各显现位同时存在时优先 Large、Medium、Small。
struct Shot {
    Formation formation = Formation::AimedFan;
    int type=1, color=2, count=1, layers=1;
    float firstSpeed=2.67857143F, lastSpeed=0, angle=0, spacing=0;
    int flags=0;
    int spatialCount=0; // 三维模式每速度层的基础方向总数；0 沿用 count，正值直接采样该数量，再加本轮 rank 弹数修正。
    float coneHalfAngle=0.75F; // 扇形延拓为球冠时的半张角，弧度，范围 (0,π]；环形始终覆盖整球。
    int sampleIndex=-1; // 单弹扫描/随机角图案的低差异序号；随机款非负时跨轮累积，避免单发总落在同一球带。
    // 单发扇形 -1 表示中心瞄准，非负时在球冠内取等立体角路点；普通多发环形忽略此字段。
    float aimAxisOffset=0; // 三维瞄准扇形的轴线绕世界 +Y 旋转弧度；angle 仍控制球冠内方位，默认保留既有排列。
    std::optional<Vector3> spatialAxis=std::nullopt; // 三维模式显式锁定非零世界方向；用于多轮固定瞄准，后续玩家移动不改变轴线。
};
/// @brief 出生局部帧上的一次技能，frame 非负；为空表示该敌人不发弹。
struct TimedShot { int frame=0; Shot shot; };
/// @brief 子发射体按次推进的极坐标扫描参数；角步长为弧度/次，emissions至少为1。
/// 配方只保存初值；实际偏移角、弹向角与剩余次数由演员的独占协程保存，不共享可变状态。
struct PolarSweep {float offsetAngleStep=0,shotAngleStep=0;int emissions=1;};
/// @brief 分层旋转发射体的只读快照；acceleration为世界单位/秒²，两个时间为正整数60Hz内容帧，角为弧度。
/// aimOffset用于控制器锁定自机轴，axisAngle/angleStep用于后继逐次扫描；状态和随机流由每名演员独占。
struct SpiralTrail {
    float acceleration=0;
    int accelerationFrames=1,emitterInterval=1;
    float aimOffset=0,axisAngle=0,angleStep=0;
};
/// @brief 固定瞄准光束；length/width 为世界单位长度/全宽，其他时间均为 60 Hz 帧。
/// color 为 0…15，长度与宽度须为正，时间须有限且非负，sustain 须为正。
struct Laser {
    int color=6;
    float length=22.3214286F, width=0.714285714F, grow=120, sustain=60, fade=16, warning=120, harmlessTail=14;
    float angle=0,speed=0; // angle 为相对瞄准/固定方向的弧度偏移；speed 为光束整体前移的世界单位/秒。
    bool aimed=true;
    float startOffset=0; // 基点到光束起点的世界距离；length 仍为实际线段长度，不包含此空隙。
    std::optional<Vector3> direction; // 非空时使用明确世界方向，不再根据 angle/aimed 推导；必须为有限非零向量。
    VisualId visual=kInvalidVisualId; // 可选的当前关卡光束外观；缺省使用 laserBase+color。
    int animationScript=-1; // 光束图集内脚本号；负值沿用清单，不从光束颜色推导。
    float initialLength=-1; // 非负时先从此世界长度伸长至 length，再沿 direction 前移；speed 须为正，基点不动。
};
/// @brief 延迟转向与加速的独立带参配置；时间为逻辑帧，速度/加速度为世界单位每秒/每秒平方。
/// 已发射的弹保存参数副本，之后更改配置只影响后续发射。
struct ProjectileMotion {
    float delayFrames=0, turnAngle=0, turnSpeed=0, acceleration=0;
    std::optional<Vector3> accelerationVector; // 非空时为固定世界加速度，不随发射角旋转；用于雨状弹的定向下落。
    float angularVelocity=0,angularFrames=99999; // 旋转速率为弧度/秒，时长为内容帧；Velocity 选项启用。
    float speedAcceleration=0,accelerationFrames=-1; // 极坐标速度增量为世界单位/秒²；加速度时长为内容帧，-1 不限。
    int turnCount=1; // 延迟转向的总次数，至少一次；每次都重新减速，后继弹保留自己的次数/速度快照。
    bool sampleAtStepStart=false; // 显现末内容帧同时启动零运动时钟，按步首采样；false 保留既有步末采样约定。
    int bounceCount=0; // 反弹选项启用时须为正；同次越界可反射多个轴，但只消耗一次次数。
    float bounceSpeed=-1; // 负值沿用生成时速率，非负为反弹后世界单位/秒速率。
    Vector3 bounceLower{-10,-10,-11.5F},bounceUpper{10,10,11.5F};
    Vector3 bounceHalfExtents{}; // 反弹边界外扩的逻辑半尺寸，世界单位，独立于碰撞半径；完全离开后才反弹，不夹回位置。
    bool preserveTurnSpeed=false; // 延迟转向保留该弹生成时的实际速率（含 rank），与允许负速率的 turnSpeed 分开。
    // BounceExceptLowerZ 保留 X/Y 两侧及 +Z 反射，-Z 方向离场时只消费次数；BounceAll 包含 -Z。
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
    bool spatial=false; // false 保留旧高度坡度；true 采用单位方向、真实速率和等面积球面/球冠采样。
    VisualId bulletBase=0, laserBase=0, fallback=0; // 每种弹连续 16 色，光束连续 16 色；编号属于调用关卡素材表。
    /// @brief 从世界位置加 offset 发射一轮，锁定本轮瞄准；池满只丢弃对应弹，不改变时序。
    /// 三维方向直接按面积采样，每轮只计算一次 rank；速度层共享角分布，不旋转复制平面图案。
    /// random 返回 [0,1) 比例乘以有限 range（可负），仅本次调用借用；随机图案必须提供它并共享演员随机流。
    /// 池满仍消费该发所需随机数。非法配置或随机图案缺少随机流抛 invalid_argument。
    void Fire(const Shot& shot,Vector3 position,Vector3 player,BulletSystem& bullets,
              const std::function<float(float)>& random={}) const;
    /// @brief 生成时锁定瞄准方向，预警与消退期间沿用原光束碰撞时窗。
    /// @brief 返回本次光束的池借用指针，池满为 nullptr；可立即建立句柄，不得跨槽位复用保留指针。
    Bullet* Fire(const Laser& laser,Vector3 position,Vector3 player,BulletSystem& bullets) const;
};
/// @brief 校验可配置单次技能的有限数值、颜色/弹种及正弹数/层数。
[[nodiscard]] bool Valid(const TimedShot& skill);
} // namespace koumakyou::skill
