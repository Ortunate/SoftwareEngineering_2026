// 本文件绑定四面晶体、发射体、小恶魔、帕秋莉与 face08 立绘；不从文件编号推测角色。
#include "stages/stage4/data.hpp"
#include "stages/stage4/ids.hpp"
#include "stages/common/assets.hpp"
#include <algorithm>
#include <array>
namespace koumakyou::stage4 {
namespace {
const auto kVisuals=[] {
    auto visuals=stage_assets::CommonBattleVisuals();
    const std::array<VisualDefinition,5> actors{{
        {.id=kFairy,.animation={battle::Atlas::Stage4Enemies,0},.defeatVisual=kNativeDefeat},
        {.id=kDarkFairy,.animation={battle::Atlas::Stage4Enemies,3},.defeatVisual=kNativeDefeat},
        {.id=kOrb,.animation={battle::Atlas::Stage4Enemies,8},.defeatVisual=kNativeDefeat},
        {.id=kPatchouli,.animation={battle::Atlas::Stage4Enemies,65},.defeatVisual=kNativeDefeat},
        {.id=kKoakuma,.animation={battle::Atlas::Stage4Enemies,64},.defeatVisual=kNativeDefeat}
    }};
    visuals.insert(visuals.end(),actors.begin(),actors.end());
    constexpr std::array<std::string_view,4> faces{"stages/stage4/portraits/patchouli/face08a_0.png",
        "stages/stage4/portraits/patchouli/face08a_1.png","stages/stage4/portraits/patchouli/face08b_0.png","stages/stage4/portraits/patchouli/face08b_1.png"};
    for(int i=0;i<4;++i)visuals.push_back({.id=kPatchouliPortraits[i],.path=faces[i],.mode=VisualMode::Character,.dialogueCrop={0,0,1,0.60F}});
    std::sort(visuals.begin(),visuals.end(),[](const auto& a,const auto& b){return a.id<b.id;});return visuals;
}();
constexpr std::array<SoundDefinition,0> kSounds{};
const StageAssetManifest kAssets{kVisuals,"stages/stage4/stage_theme.flac","stages/stage4/boss_theme.flac",kSounds};
}
const StageAssetManifest& Stage4Manifest() {return kAssets;}
}
