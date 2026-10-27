// 本文件定义各关共用的原生战斗素材槽；演员和 Boss/立绘槽由具体关卡填入。

#pragma once

#include "resources/visual_assets.hpp"
#include <array>

namespace koumakyou::stage_assets {

// 编号属于当前关卡清单；公共槽只统一玩家、弹幕、道具的绑定，不跨关卡借用纹理。
constexpr VisualId kBlueBullet = 0;
constexpr VisualId kRedBullet = 1;
constexpr VisualId kGreenSphere = 2;
constexpr VisualId kFairy = 3;
constexpr VisualId kDarkFairy = 4;
constexpr VisualId kOrb = 5;
constexpr VisualId kBoss = 6;
constexpr VisualId kReimuPortrait = 7;
/// @brief 玩家直射符卡与追踪符卡使用独立的关卡外观编号。
constexpr VisualId kPlayerGoldCard = 8;
constexpr VisualId kPlayerCyanCard = 9;
/// @brief Power 道具的公告板贴图，由各关素材清单填充。
constexpr VisualId kPowerPoint = 10;
/// @brief 分数道具的公告板贴图编号。
constexpr VisualId kScorePoint = 11;
/// @brief 满火力清弹产生的星点公告板贴图编号。
constexpr VisualId kStarPoint = 12;

/// @brief 原版表情按 a0、a1、b0、b1、c0、c1 排列；默认图沿用原有 ID，不移动玩法编号。
constexpr std::array<VisualId, 6> kReimuPortraits{kReimuPortrait, 13, 14, 15, 16, 17};
/// @brief Boss 的四个预留立绘槽，不与战斗精灵共用编号或纹理。
constexpr std::array<VisualId, 4> kBossPortraits{21, 18, 19, 20};
// 战斗 Boss kBoss 与立绘 21 分离；22 起为十种弹幕的 16 个颜色槽，后接大 P 与激光。
constexpr VisualId kNativeBulletBase = 22;
constexpr VisualId kLargePowerPoint = kNativeBulletBase + 160;
constexpr VisualId kNativeLaserBase = kLargePowerPoint + 1;
constexpr VisualId kNativeDefeat = kNativeLaserBase + 16;
/// @brief 灵梦左右太极圈，追加在既有外观之后，保持弹种和对话编号不变。
constexpr VisualId kPlayerOptionLeft = kNativeDefeat + 3;
constexpr VisualId kPlayerOptionRight = kPlayerOptionLeft + 1;
/// @brief 梦之御札侧针，使用 player00 的独立针弹动画，不复用中央符卡外观。
constexpr VisualId kPlayerNeedle = kPlayerOptionRight + 1;
/// @brief 魔理沙 player01 独立主弹、副弹、爆破及副机物件，全部追加，保持既有素材编号稳定。
constexpr VisualId kMarisaPrimary = kPlayerNeedle + 1;
constexpr VisualId kMarisaMissile = kMarisaPrimary + 1; // 连续四项，对应脚本 65～68。
constexpr VisualId kMarisaLaser = kMarisaMissile + 4; // 连续三项，对应脚本 69～71。
constexpr VisualId kMarisaExplosion = kMarisaLaser + 3; // 连续四项，对应脚本 97～100。
constexpr VisualId kMarisaOptionLeft = kMarisaExplosion + 4;
constexpr VisualId kMarisaOptionRight = kMarisaOptionLeft + 1;
constexpr std::array<VisualId,6> kMarisaPortraits{kMarisaOptionRight+1,kMarisaOptionRight+2,kMarisaOptionRight+3,
                                              kMarisaOptionRight+4,kMarisaOptionRight+5,kMarisaOptionRight+6};
/// @brief 四 Bomb 的原生层；每组最多四项，星群只有前三项有效，保持之前全部编号不变。
constexpr VisualId kReimuBombSeal = kMarisaPortraits.back()+1;
constexpr VisualId kReimuBombBarrier = kReimuBombSeal+4;
constexpr VisualId kMarisaBombStars = kReimuBombBarrier+4;
constexpr VisualId kMarisaBombSpark = kMarisaBombStars+3;
/// @brief 原生 Bomb、满 Power、残机道具追加到既有槽后，不改变旧弹幕编号。
constexpr VisualId kBombPoint=kMarisaBombSpark+4;
constexpr VisualId kFullPowerPoint=kBombPoint+1;
constexpr VisualId kLifePoint=kFullPowerPoint+1;
constexpr std::size_t kCommonVisualCount=kLifePoint+1;
/// @brief 保留 ECL 弹幕类型与颜色，非法输入回退普通蓝弹；类型内部颜色转换由战斗目录负责。
constexpr VisualId NativeBulletVisual(int type,int color) {
    return type>=0 && type<10 && color>=0 && color<16
        ? static_cast<VisualId>(kNativeBulletBase+type*16+color) : kBlueBullet;
}

}  // namespace koumakyou::stage_assets
