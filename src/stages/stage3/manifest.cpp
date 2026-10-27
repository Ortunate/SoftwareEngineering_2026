// 本文件绑定三面普通演员、美铃的 stg3enm 动画、face06 立绘及原关卡音乐。
#include "stages/stage3/data.hpp"
#include "stages/stage3/ids.hpp"
#include "stages/common/assets.hpp"
#include <array>
#include <algorithm>
namespace koumakyou::stage3 {
namespace {
const auto kVisuals=[] {
    auto visuals=stage_assets::CommonBattleVisuals();
    const std::array<VisualDefinition,4> actors{{
        {.id=kFairy,.worldHeight=1.8F,.animation={battle::Atlas::Stage3Enemies,0},.defeatVisual=kNativeDefeat},
        {.id=kDarkFairy,.worldHeight=1.8F,.animation={battle::Atlas::Stage3Enemies,3},.defeatVisual=kNativeDefeat},
        {.id=kOrb,.worldHeight=1.488F,.animation={battle::Atlas::Stage3Enemies,13},.defeatVisual=kNativeDefeat},
        {.id=kMeiling,.worldHeight=3.2F,.animation={battle::Atlas::Stage3Enemies,64},.defeatVisual=kNativeDefeat}
    }};
    visuals.insert(visuals.end(),actors.begin(),actors.end());
    constexpr std::array<std::string_view,4> faces{"stages/stage3/portraits/meiling/face06a_0.png",
        "stages/stage3/portraits/meiling/face06a_1.png","stages/stage3/portraits/meiling/face06b_0.png","stages/stage3/portraits/meiling/face06b_1.png"};
    for(int i=0;i<4;++i)visuals.push_back({.id=kMeilingPortraits[i],.path=faces[i],.mode=VisualMode::Character,.dialogueCrop={0,0,1,0.60F}});
    std::sort(visuals.begin(),visuals.end(),[](const auto& a,const auto& b){return a.id<b.id;});return visuals;
}();
constexpr std::array<SoundDefinition,0> kSounds{};
const StageAssetManifest kAssets{kVisuals,"stages/stage3/stage_theme.mp3","stages/stage3/boss_theme.mp3",kSounds};
}
const StageAssetManifest& Stage3Manifest() {return kAssets;}
}
