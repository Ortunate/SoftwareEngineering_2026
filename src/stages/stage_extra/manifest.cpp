// 本文件绑定Extra原生演员、双Boss、立绘、背景和三首音乐；不读取本机原包或生成事件表。
#include "stages/stage_extra/manifest.hpp"
#include "stages/stage_extra/ids.hpp"
#include "stages/common/assets.hpp"
#include <algorithm>
#include <array>
namespace koumakyou::stage_extra {
namespace {
const auto kVisuals=[] {
    auto visuals=stage_assets::CommonBattleVisuals();
    constexpr std::array<VisualDefinition,5> actors{{
        {.id=kFairy,.animation={battle::Atlas::ExtraEnemies,0},.defeatVisual=kNativeDefeat},
        {.id=kDarkFairy,.animation={battle::Atlas::ExtraEnemies,3},.defeatVisual=kNativeDefeat},
        {.id=kOrb,.animation={battle::Atlas::ExtraEnemies,13},.defeatVisual=kNativeDefeat},
        {.id=kFlandre,.animation={battle::Atlas::Flandre,160},.defeatVisual=kNativeDefeat},
        {.id=kPatchouli,.animation={battle::Atlas::ExtraEnemies,65},.defeatVisual=kNativeDefeat}
    }};
    visuals.insert(visuals.end(),actors.begin(),actors.end());
    // MSG2的对手编号以0x4c6为基址：a/b预载于+2/+3，c在非道中对白覆写+0。
    // Flandre未使用+1；公共预留槽显式复用c，不能将文件顺序当作原生表达式编号。
    constexpr std::array<std::string_view,4> flandre{
        "stages/stage_extra/portraits/flandre/face12c_0.png","stages/stage_extra/portraits/flandre/face12c_0.png",
        "stages/stage_extra/portraits/flandre/face12a_0.png","stages/stage_extra/portraits/flandre/face12b_0.png"};
    constexpr std::array<std::string_view,4> patchouli{
        "stages/stage4/portraits/patchouli/face08a_0.png","stages/stage4/portraits/patchouli/face08a_1.png",
        "stages/stage4/portraits/patchouli/face08b_0.png","stages/stage4/portraits/patchouli/face08b_1.png"};
    for(int i=0;i<4;++i) {
        visuals.push_back({.id=kFlandrePortraits[i],.path=flandre[i],.dialogueCrop={0,0,1,.60F}});
        visuals.push_back({.id=kPatchouliPortraits[i],.path=patchouli[i],.dialogueCrop={0,0,1,.60F}});
    }
    visuals.push_back({.id=kBackground,.path="stages/stage_extra/background.dds"});
    std::sort(visuals.begin(),visuals.end(),[](const auto& a,const auto& b){return a.id<b.id;});return visuals;
}();
const StageAssetManifest kAssets{.visuals=kVisuals,.stageBgm="stages/stage_extra/stage_theme.flac",
    .bossBgm="stages/stage_extra/boss_theme.flac",.sounds={},.eventBgm="stages/stage_extra/event_theme.flac"};
}
const StageAssetManifest& StageExtraManifest() {return kAssets;}
}
