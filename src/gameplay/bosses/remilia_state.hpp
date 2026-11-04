// 本文件定义蕾米莉亚实例独占的跨阶段攻击记忆，关卡配方只借用而不持有它。
#pragma once
namespace koumakyou::bosses {
/// @brief 蓄力剩余脉冲数，非负；星阵被打断时保留，后继非符继续消费，不用固定等待伪造。
struct RemiliaPhaseMemory {int chargingPulsesLeft=0;};
}
