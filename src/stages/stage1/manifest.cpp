// 本文件负责第一面的演员、Boss、对话与音乐清单，玩家和弹幕复用公共素材定义。
#include "stages/stage1/data.hpp"
#include "stages/stage1/ids.hpp"
#include "stages/common/assets.hpp"
#include <array>
#include <algorithm>
namespace koumakyou::stage1 {
namespace {
const auto kVisuals=[] {
    auto visuals=stage_assets::CommonBattleVisuals();
    constexpr Rectangle kRumiaDialogueCrop{0,0,1,0.60F};
    const std::array<VisualDefinition,8> local{{
    {.id=kFairy, .animation={battle::Atlas::Stage1Enemies,0}, .defeatVisual=kNativeDefeat},
    {.id=kDarkFairy, .animation={battle::Atlas::Stage1Enemies,3}, .defeatVisual=kNativeDefeat},
    {.id=kOrb, .animation=battle::EnemyBullet(6,3), .defeatVisual=kNativeDefeat},
    {.id=kRumia, .pivot={0.5F,0.5F}, .animation={battle::Atlas::Rumia,128}, .defeatVisual=kNativeDefeat},
    {kRumiaPortraits[1], "stages/stage1/portraits/rumia/face03a_1.png", VisualMode::Character,
     1.0F, {0.5F, 0.5F}, 2.6F, false, kRumiaDialogueCrop},
    {kRumiaPortraits[2], "stages/stage1/portraits/rumia/face03b_0.png", VisualMode::Character,
     1.0F, {0.5F, 0.5F}, 2.6F, false, kRumiaDialogueCrop},
    {kRumiaPortraits[3], "stages/stage1/portraits/rumia/face03b_1.png", VisualMode::Character,
     1.0F, {0.5F, 0.5F}, 2.6F, false, kRumiaDialogueCrop},
    {kRumiaPortraits[0], "stages/stage1/portraits/rumia/face03a_0.png", VisualMode::Character,
     1.0F, {0.5F,0.5F}, 2.6F, false, kRumiaDialogueCrop},
    }};
    visuals.insert(visuals.end(),local.begin(),local.end());
    std::sort(visuals.begin(),visuals.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    return visuals;
}();
constexpr std::array<SoundDefinition,0> kSounds{};
const StageAssetManifest kAssets{kVisuals,"stages/stage1/宛如鬼灯的鲜红之魂.flac","stages/stage1/妖魔夜行.flac",kSounds};
}
const StageAssetManifest& Stage1Manifest() { return kAssets; }
}
