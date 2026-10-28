// 本文件声明六面普通演员的命名行为；路线/攻击配方共享，实例初始动画独立选择红蓝外观。
#pragma once
#include "gameplay/actors/behavior.hpp"
namespace koumakyou::stage6 {
// 以下协程由 Actor::Start/Call 持有；等待采用 60 Hz 内容帧，空间量使用世界单位。
// actor 与 PhaseMemory 的借用期覆盖协程执行期；阶段回调由 Boss 包装层推进，不在协程内销毁演员。
/// @brief 隐藏环境演出保留周期位置随机读取和寿命，不参与碰撞或索敌。
behavior::Routine AmbientPositions(behavior::Actor& actor);
/// @brief 两类弯曲晶体保留各自转速、停火帧和随机初相，不由外观脚本反推路线。
behavior::Routine BlueHook(behavior::Actor& actor);
behavior::Routine RedHook(behavior::Actor& actor);
/// @brief 横穿蓝环在第 40 内容帧发射并选择随机退场方向；击破掉落由配方的独立回调负责。
behavior::Routine CrossingRing(behavior::Actor& actor);
/// @brief 下行蓝环保留与横穿款相同的发射/随机读取契约，出生方向由配方决定。
behavior::Routine DescendingRing(behavior::Actor& actor);
/// @brief 击破时请求散落八个普通道具并结束演员；调用方仍负责一次性的分数及源掉落。
behavior::Routine DropEight(behavior::Actor& actor);
inline constexpr behavior::Pattern kAmbientPositions{.movement={.speed=0},.initialAnimation=12,.routine=AmbientPositions};
inline constexpr behavior::Pattern kBlueHook{.movement={.angle=0.523598790F,.speed=12.0535717F},.initialAnimation=8,.deathEffect=0,.spatialShots=true,.routine=BlueHook};
inline constexpr behavior::Pattern kRedHook{.movement={.angle=-1.04719758F,.speed=10.7142859F},.initialAnimation=8,.deathEffect=0,.spatialShots=true,.routine=RedHook};
inline constexpr behavior::Pattern kCrossingRing{.movement={.angle=0,.speed=5.35714293F},.initialAnimation=11,.deathEffect=0,.spatialShots=true,.routine=CrossingRing,.onDefeated=DropEight};
inline constexpr behavior::Pattern kDescendingRing{.movement={.speed=5.35714293F},.initialAnimation=11,.deathEffect=0,.spatialShots=true,.routine=DescendingRing,.onDefeated=DropEight};
/// @brief 安装六面弹幕资源并选择真实三维/中性平面模式；不重置实例随机流。
void ConfigureBehavior(behavior::Actor& actor,bool spatial);

}
