// 本文件保存关卡结束时的奖励快照；UI 展示不重新读取或修改战斗计数。
#pragma once

namespace koumakyou {
/// @brief Normal 关卡结算明细；计数均非负，关卡号为 1～6，分数单位为游戏分。
/// 整关奖励为 (Stage×1000 + Power×100 + Graze×10)×Point Item，Normal 倍率为 1.0。
/// 构造方须保证实际计数的乘积不超过 long long；快照只计算奖励，不负责将奖励加入总分。
struct StageClearSummary {
    int stage = 1, power = 0, graze = 0, pointItems = 0;
    [[nodiscard]] long long StagePoints() const { return stage*1000LL; }
    [[nodiscard]] long long PowerPoints() const { return power*100LL; }
    [[nodiscard]] long long GrazePoints() const { return graze*10LL; }
    [[nodiscard]] long long Total() const { return (StagePoints()+PowerPoints()+GrazePoints())*pointItems; }
};
/// @brief 结算等待的 UI 秒数；暂停时冻结，不计入关卡、弹幕或对话时钟。
inline constexpr float kStageClearSeconds = 10.0F;
} // namespace koumakyou
