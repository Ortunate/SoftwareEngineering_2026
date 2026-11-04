// 本文件统一战斗与录像校验使用的玩家入口资源；不修改持久设置或连续切关的库存。
#pragma once
#include "app/spell_practice.hpp"
#include "gameplay/actors/player.hpp"
#include "stage/catalog.hpp"
#include <stdexcept>

namespace koumakyou::player_start {
/// @brief extraLives 不含当前机体，bombs 为库存，power 使用原生0～128点数；本项目游玩下限为8。
struct Resources { int extraLives,bombs,power; };
/// @brief 根据已校验的入口计算新局资源；配置值范围为残机1～5、Bomb 0～3。
/// 单符必须提供属于当前关卡的Card；非法配置或缺失/错配的Card抛invalid_argument。
inline Resources ForEntry(StageId stage,StageEntry entry,int extraLives,int bombs,const spell::Card* card=nullptr) {
    if(extraLives<1||extraLives>5||bombs<0||bombs>3||StageNumber(stage)==0)
        throw std::invalid_argument("Invalid player entry resources");
    if(entry==StageEntry::SpellPractice) {
        if(!card||card->stage!=StageNumber(stage)-1)throw std::invalid_argument("Invalid spell player entry");
        return {0,0,card->power};
    }
    // 原生整关练习一面不追加Power；保持项目最低8点，二面64，三面及以后128。
    const int power=entry!=StageEntry::Practice||stage==StageId::Stage1?Player::kMinPower:
        stage==StageId::Stage2?64:Player::kMaxPower;
    return {extraLives,bombs,power};
}
}
