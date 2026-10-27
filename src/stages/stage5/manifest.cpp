// 本文件绑定五面晶体、咲夜本体、face09 四个表情及两首关卡音乐，不持有活动动画游标。
#include "stages/stage5/data.hpp"
#include "stages/stage5/ids.hpp"
#include "stages/common/assets.hpp"
#include <algorithm>
#include <array>
namespace koumakyou::stage5 {
namespace {
const auto kVisuals=[] {
    auto visuals=stage_assets::CommonBattleVisuals();
    constexpr std::array<VisualDefinition,4> actors{{
        {.id=kFairy,.worldHeight=1.8F,.animation={battle::Atlas::Stage5Enemies,0},.defeatVisual=kNativeDefeat},
        {.id=kDarkFairy,.worldHeight=1.8F,.animation={battle::Atlas::Stage5Enemies,3},.defeatVisual=kNativeDefeat},
        {.id=kOrb,.worldHeight=1.488F,.animation={battle::Atlas::Stage5Enemies,10},.defeatVisual=kNativeDefeat},
        {.id=kSakuya,.worldHeight=3.2F,.animation={battle::Atlas::Sakuya,128},.defeatVisual=kNativeDefeat}
    }};
    visuals.insert(visuals.end(),actors.begin(),actors.end());
    constexpr std::array<std::string_view,4> faces{"stages/stage5/portraits/sakuya/face09a_0.png","stages/stage5/portraits/sakuya/face09a_1.png",
        "stages/stage5/portraits/sakuya/face09b_0.png","stages/stage5/portraits/sakuya/face09b_1.png"};
    for(int i=0;i<4;++i)visuals.push_back({.id=kSakuyaPortraits[i],.path=faces[i],.mode=VisualMode::Character,.dialogueCrop={0,0,1,0.60F}});
    std::sort(visuals.begin(),visuals.end(),[](const auto& a,const auto& b){return a.id<b.id;});return visuals;
}();
constexpr std::array<SoundDefinition,0> kSounds{};
const StageAssetManifest kAssets{kVisuals,"stages/stage5/stage_theme.mp3","stages/stage5/boss_theme.mp3",kSounds};
}
const StageAssetManifest& Stage5Manifest() {return kAssets;}
}
