// 本文件声明Extra的普通演员命名配方；关卡入口须待完整移植后另行注册。
#pragma once
#include "gameplay/actors/behavior.hpp"

namespace koumakyou::stage_extra {
/// @brief 周期瞄准单弹与两种三弹曲线；使用实例rank修正周期，初相只消费一次实例随机流。
/// 协程由Actor独占，空间量为世界单位，等待为60 Hz内容帧；不依赖主线难度选择。
behavior::Routine SingleCurve(behavior::Actor& actor);
behavior::Routine TightCurve(behavior::Actor& actor);
behavior::Routine WideCurve(behavior::Actor& actor);
/// @brief 第60内容帧停止、第70帧发射、第130帧加速转弯；横穿款初始朝+X，其余朝-Z。
/// 三种配方分别保留单轮多层扇、双轮窄扇和双轮宽扇，已发弹不借用后续演员技能状态。
behavior::Routine LayeredFanTurn(behavior::Actor& actor);
behavior::Routine TwinFanTurn(behavior::Actor& actor);
behavior::Routine CrossingFanTurn(behavior::Actor& actor);
/// @brief 先减速，再按不同角步长/速度序列交替发环；等待为60 Hz内容帧，攻击期间保留实例随机流。
/// 最后一种在整段攻击结束前保护本体；其余在第70帧恢复受伤。击破后均交付8个源掉落请求。
behavior::Routine BlueSpiralRings(behavior::Actor& actor);
behavior::Routine PairedBlueSpiralRings(behavior::Actor& actor);
behavior::Routine PairedRedSpiralRings(behavior::Actor& actor);
behavior::Routine ExpandingCrystalRings(behavior::Actor& actor);
/// @brief 击破后继请求8个源掉落并结束；由演员池消费掉落事件，不拥有道具资源。
behavior::Routine DropEight(behavior::Actor& actor);
/// @brief 固定发射体第30帧开放交互，按各自轮数/间隔发射后中断本体ANM并等待30帧退场。
/// 发射不清除已生弹；击破后继另请求8个源掉落，本体与辅助光圈分别中断并保留淡出等待。
behavior::Routine RedRingEmitter(behavior::Actor& actor);
behavior::Routine RandomRedEmitter(behavior::Actor& actor);
behavior::Routine BlueFanEmitter(behavior::Actor& actor);
behavior::Routine BlueLayerEmitter(behavior::Actor& actor);
behavior::Routine LayeredRedEmitter(behavior::Actor& actor);
behavior::Routine EmitterDefeated(behavior::Actor& actor);

inline constexpr behavior::Pattern kSingleCurve{.movement={.speed=5.35714286F},.initialAnimation=0,.spatialShots=true,.routine=SingleCurve};
inline constexpr behavior::Pattern kTightCurve{.movement={.speed=5.35714286F},.initialAnimation=0,.spatialShots=true,.routine=TightCurve};
inline constexpr behavior::Pattern kWideCurve{.movement={.speed=5.35714286F},.initialAnimation=0,.spatialShots=true,.routine=WideCurve};
inline constexpr behavior::Pattern kLayeredFanTurn{.movement={.speed=5.35714286F},.initialAnimation=3,.spatialShots=true,.routine=LayeredFanTurn};
inline constexpr behavior::Pattern kTwinFanTurn{.movement={.speed=5.35714286F},.initialAnimation=3,.spatialShots=true,.routine=TwinFanTurn};
inline constexpr behavior::Pattern kCrossingFanTurn{.movement={.angle=0,.speed=5.35714286F},.initialAnimation=3,.spatialShots=true,.routine=CrossingFanTurn};
inline constexpr behavior::Pattern kBlueSpiralRings{.movement={.speed=5.35714286F},.initialAnimation=12,.spatialShots=true,.routine=BlueSpiralRings,.onDefeated=DropEight};
inline constexpr behavior::Pattern kPairedBlueSpiralRings{.movement={.speed=5.35714286F},.initialAnimation=12,.spatialShots=true,.routine=PairedBlueSpiralRings,.onDefeated=DropEight};
inline constexpr behavior::Pattern kPairedRedSpiralRings{.movement={.speed=5.35714286F},.initialAnimation=10,.spatialShots=true,.routine=PairedRedSpiralRings,.onDefeated=DropEight};
inline constexpr behavior::Pattern kExpandingCrystalRings{.movement={.speed=5.35714286F},.initialAnimation=10,.spatialShots=true,.routine=ExpandingCrystalRings,.onDefeated=DropEight};
inline constexpr behavior::Pattern kRedRingEmitter{.movement={.speed=0},.initialAnimation=14,.spatialShots=true,.routine=RedRingEmitter,.onDefeated=EmitterDefeated};
inline constexpr behavior::Pattern kRandomRedEmitter{.movement={.speed=0},.initialAnimation=14,.spatialShots=true,.routine=RandomRedEmitter,.onDefeated=EmitterDefeated};
inline constexpr behavior::Pattern kBlueFanEmitter{.movement={.speed=0},.initialAnimation=14,.spatialShots=true,.routine=BlueFanEmitter,.onDefeated=EmitterDefeated};
inline constexpr behavior::Pattern kBlueLayerEmitter{.movement={.speed=0},.initialAnimation=14,.spatialShots=true,.routine=BlueLayerEmitter,.onDefeated=EmitterDefeated};
inline constexpr behavior::Pattern kLayeredRedEmitter{.movement={.speed=0},.initialAnimation=14,.spatialShots=true,.routine=LayeredRedEmitter,.onDefeated=EmitterDefeated};

/// @brief 绑定调用方当前关卡的公共弹幕槽；spatial=false用于独立平面差分，保留实例随机流及rank。
/// 这些素材编号只借用当前关卡清单，不能跨卸载保留；本函数不开放Extra的正式入口。
void ConfigureBehavior(behavior::Actor& actor,bool spatial);
}
