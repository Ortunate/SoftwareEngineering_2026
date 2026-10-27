// 本文件将二面演员、琪露诺原姿态现代重绘立绘及本关音乐绑定到独立素材清单，公共玩家槽由共享模块提供。
#include "stages/stage2/data.hpp"
#include "stages/stage2/ids.hpp"
#include "stages/common/assets.hpp"
#include <array>
#include <algorithm>
namespace koumakyou::stage2 {
namespace {
const auto kVisuals=[] {
    auto visuals=stage_assets::CommonBattleVisuals();
    const std::array<VisualDefinition,5> actors{{
        {.id=kFairy,.worldHeight=1.8F,.animation={battle::Atlas::Stage2Enemies,0},.defeatVisual=kNativeDefeat},
        {.id=kDarkFairy,.worldHeight=1.8F,.animation={battle::Atlas::Stage2Enemies,3},.defeatVisual=kNativeDefeat},
        {.id=kOrb,.worldHeight=1.488F,.animation={battle::Atlas::Stage2Enemies,12},.defeatVisual=kNativeDefeat},
        {.id=kCirno,.worldHeight=3.2F,.animation={battle::Atlas::Cirno,128},.defeatVisual=kNativeDefeat},
        {.id=kDaiyousei,.worldHeight=2.8F,.animation={battle::Atlas::Stage2Enemies,64},.defeatVisual=kNativeDefeat}
    }};
    visuals.insert(visuals.end(),actors.begin(),actors.end());
    constexpr std::array<std::string_view,2> faces{
        "stages/stage2/portraits/cirno/face05a_0.png","stages/stage2/portraits/cirno/face05a_1.png"};
    // 公共清单保留四个连续槽；未使用的两个槽引用默认图，不编造琪露诺的额外表情。
    for(int expression=0;expression<4;++expression)
        visuals.push_back({.id=kBossPortraits[expression],.path=faces[expression<2 ? expression : 0],.mode=VisualMode::Character,
            .dialogueCrop={0,0,1,0.60F}});
    std::sort(visuals.begin(),visuals.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    return visuals;
}();
constexpr std::array<SoundDefinition,0> kSounds{};
const StageAssetManifest kAssets{kVisuals,"stages/stage2/stage_theme.mp3","stages/stage2/boss_theme.mp3",kSounds};
}
const StageAssetManifest& Stage2Manifest() { return kAssets; }
}
