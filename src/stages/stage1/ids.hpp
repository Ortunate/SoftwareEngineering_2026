// 本文件负责定义第一面局部使用的外观编号。

#pragma once

#include "resources/visual_assets.hpp"
#include <array>

namespace koumakyou::stage1 {

// 编号仅在 Stage 1 内有效；更换 Stage 不要求保留相同含义。
constexpr VisualId kBlueBullet = 0;
constexpr VisualId kRedBullet = 1;
constexpr VisualId kGreenSphere = 2;
constexpr VisualId kFairy = 3;
constexpr VisualId kDarkFairy = 4;
constexpr VisualId kOrb = 5;
constexpr VisualId kRumia = 6;
constexpr VisualId kReimuPortrait = 7;
/// @brief 玩家直射符卡与追踪符卡使用独立的关卡外观编号。
constexpr VisualId kPlayerGoldCard = 8;
constexpr VisualId kPlayerCyanCard = 9;
/// @brief Power 道具的公告板贴图，仅供第一面素材清单和场景绘制使用。
constexpr VisualId kPowerPoint = 10;
/// @brief 分数道具的公告板贴图编号。
constexpr VisualId kScorePoint = 11;
/// @brief 满火力清弹产生的星点公告板贴图编号。
constexpr VisualId kStarPoint = 12;

/// @brief 原版表情按 a0、a1、b0、b1、c0、c1 排列；默认图沿用原有 ID，不移动玩法编号。
constexpr std::array<VisualId, 6> kReimuPortraits{kReimuPortrait, 13, 14, 15, 16, 17};
/// @brief 露米娅的 a0、a1、b0、b1 立绘差分，不与战斗精灵共用编号或纹理。
constexpr std::array<VisualId, 4> kRumiaPortraits{21, 18, 19, 20};
// 战斗露米娅 kRumia 与立绘 21 分离；22 起为十种弹幕的 16 个颜色槽，后接大 P 与激光。
constexpr VisualId kNativeBulletBase = 22;
constexpr VisualId kLargePowerPoint = kNativeBulletBase + 160;
constexpr VisualId kNativeLaserBase = kLargePowerPoint + 1;
constexpr VisualId kNativeDefeat = kNativeLaserBase + 16;
/// @brief 灵梦左右太极圈，追加在既有外观之后，保持弹种和对话编号不变。
constexpr VisualId kPlayerOptionLeft = kNativeDefeat + 3;
constexpr VisualId kPlayerOptionRight = kPlayerOptionLeft + 1;
constexpr std::size_t kVisualCount = kPlayerOptionRight + 1;
/// @brief 保留 ECL 弹幕类型与颜色，非法输入回退普通蓝弹；类型内部颜色转换由战斗目录负责。
constexpr VisualId NativeBulletVisual(int type,int color) {
    return type>=0 && type<10 && color>=0 && color<16
        ? static_cast<VisualId>(kNativeBulletBase+type*16+color) : kBlueBullet;
}

}  // namespace koumakyou::stage1
