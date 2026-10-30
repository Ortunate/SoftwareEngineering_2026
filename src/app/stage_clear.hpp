// 本文件保存关卡结束时的奖励快照；UI 展示不重新读取或修改战斗计数。
#pragma once
#include "gameplay/data/native_combat.hpp"
#include <stdexcept>

namespace koumakyou {
/// @brief 关卡结算明细；计数均非负，关卡号为 1～7，7为独立Extra，分数单位为游戏分。
/// 基础分为 (Stage×1000 + Power×100 + Graze×10)×Point Item；倍率按难度计算后向下取整到 10 分。
/// 第六关及Extra先追加剩余残机×300万和 Bomb×100万；Extra使用Normal内部上下文，原生倍率固定2倍。
/// 初始残机为 3/4 时分别再乘 0.5/0.2 并截断到 10 分；不模拟原生32位结算中间值溢出。
/// startingLives、extraLives 为不含当前机体的额外残机数；所有中间乘积须可由 long long 表示。
struct StageClearSummary {
    int stage = 1, power = 0, graze = 0, pointItems = 0;
    native_combat::Difficulty difficulty=native_combat::Difficulty::Normal;
    int extraLives=0,bombs=0,startingLives=2;
    [[nodiscard]] long long StagePoints() const { return stage*1000LL; }
    [[nodiscard]] long long PowerPoints() const { return power*100LL; }
    [[nodiscard]] long long GrazePoints() const { return graze*10LL; }
    [[nodiscard]] long long Total() const {
        const long long base=(StagePoints()+PowerPoints()+GrazePoints())*pointItems+
            (stage>=6 ? extraLives*3000000LL+bombs*1000000LL : 0);
        long long result=0;
        if(stage==7) {
            if(difficulty!=native_combat::Difficulty::Normal)throw std::invalid_argument("Invalid Extra clear context");
            result=(base*2/10)*10;
        } else switch(difficulty) {
            case native_combat::Difficulty::Easy:result=(base/20)*10;break;
            case native_combat::Difficulty::Normal:result=base;break;
            case native_combat::Difficulty::Hard:result=(base*12/100)*10;break;
            case native_combat::Difficulty::Lunatic:result=(base*15/100)*10;break;
            default:throw std::invalid_argument("Invalid stage clear difficulty");
        }
        if(startingLives==3)return (result/20)*10;
        if(startingLives==4)return (result/50)*10;
        return result;
    }
};
/// @brief 结算等待的 UI 秒数；暂停时冻结，不计入关卡、弹幕或对话时钟。
inline constexpr float kStageClearSeconds = 10.0F;
} // namespace koumakyou
