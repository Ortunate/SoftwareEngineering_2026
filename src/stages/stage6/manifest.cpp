// 本文件绑定六面晶体、蕾米莉亚与道中咲夜本体及各自立绘及两首关卡音乐，不持有活动动画游标。
#include "stages/stage6/data.hpp"
#include "stages/stage6/ids.hpp"
#include "stages/common/assets.hpp"
#include <algorithm>
#include <array>
namespace koumakyou::stage6 {
namespace {
const auto kVisuals=[] {
    auto visuals=stage_assets::CommonBattleVisuals();
    constexpr std::array<VisualDefinition,5> actors{{
        {.id=kFairy,.animation={battle::Atlas::Stage6Enemies,0},.defeatVisual=kNativeDefeat},
        {.id=kDarkFairy,.animation={battle::Atlas::Stage6Enemies,3},.defeatVisual=kNativeDefeat},
        {.id=kOrb,.animation={battle::Atlas::Stage6Enemies,10},.defeatVisual=kNativeDefeat},
        {.id=kRemilia,.animation={battle::Atlas::Remilia,160},.defeatVisual=kNativeDefeat},
        {.id=kSakuya,.animation={battle::Atlas::Sakuya,128},.defeatVisual=kNativeDefeat}
    }};
    visuals.insert(visuals.end(),actors.begin(),actors.end());
    constexpr std::array<std::string_view,4> faces{"stages/stage6/portraits/remilia/face10a_0.png","stages/stage6/portraits/remilia/face10a_0.png",
        "stages/stage6/portraits/remilia/face10a_0.png","stages/stage6/portraits/remilia/face10b_0.png"};
    for(int i=0;i<4;++i)visuals.push_back({.id=kRemiliaPortraits[i],.path=faces[i],.mode=VisualMode::Character,.dialogueCrop={0,0,1,0.60F}});
    constexpr std::array<std::string_view,2> maidFaces{"stages/stage6/portraits/sakuya/face09a_0.png","stages/stage6/portraits/sakuya/face09a_1.png"};
    for(int i=0;i<2;++i)visuals.push_back({.id=kSakuyaPortraits[i],.path=maidFaces[i],.mode=VisualMode::Character,.dialogueCrop={0,0,1,0.60F}});
    std::sort(visuals.begin(),visuals.end(),[](const auto& a,const auto& b){return a.id<b.id;});return visuals;
}();
constexpr std::array<SoundDefinition,0> kSounds{};
const StageAssetManifest kAssets{kVisuals,"stages/stage6/stage_theme.mp3","stages/stage6/boss_theme.mp3",kSounds};
}
const StageAssetManifest& Stage6Manifest() {return kAssets;}
}
